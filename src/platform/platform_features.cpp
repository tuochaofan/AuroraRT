#include "aurorart/platform/platform_features.h"
#include "aurorart/platform/platform_abstraction.h"

#ifdef __linux__
#include <sys/sysinfo.h>
#include <cpuid.h>
#include <dirent.h>
#include <unistd.h>
#include <cstdio>
#elif defined(_WIN32)
#include <intrin.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <windows.h>
#include <winreg.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

namespace aurorart {
namespace platform {

// 硬件特性检测策略接口
class HardwareFeatureDetector {
public:
    virtual ~HardwareFeatureDetector() = default;
    virtual bool detectTSNSupport() = 0;
    virtual bool detectSSE2Support() = 0;
    virtual bool detectAVXSupport() = 0;
    virtual bool detectAVX2Support() = 0;
    virtual bool detectAVX512Support() = 0;
    virtual bool detectNEONSupport() = 0;
};

// Linux平台硬件特性检测
#ifdef __linux__
class LinuxHardwareDetector : public HardwareFeatureDetector {
public:
    bool detectTSNSupport() override {
        // 检查是否存在TSN相关的网络设备特性
        DIR* dir = opendir("/sys/class/net");
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                // 跳过.和..目录
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                
                // 检查设备是否支持TSN特性
                std::string path = "/sys/class/net/" + std::string(entry->d_name) + "/tsn";
                if (access(path.c_str(), F_OK) == 0) {
                    closedir(dir);
                    return true;
                }
                
                // 检查是否支持IEEE 802.1Qbv（时间感知调度）
                path = "/sys/class/net/" + std::string(entry->d_name) + "/queues/tx-0/bytes";
                if (access(path.c_str(), F_OK) == 0) {
                    // 存在队列文件，可能支持TSN
                    closedir(dir);
                    return true;
                }
            }
            closedir(dir);
        }
        
        // 尝试使用ethtool命令检测
        FILE* pipe = popen("ethtool -k eth0 2>/dev/null | grep tsn", "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                pclose(pipe);
                return true;
            }
            pclose(pipe);
        }
        
        return false;
    }
    
    bool detectSSE2Support() override {
        unsigned int eax, ebx, ecx, edx;
        __get_cpuid(1, &eax, &ebx, &ecx, &edx);
        return (edx & (1 << 26)) != 0;
    }
    
    bool detectAVXSupport() override {
        unsigned int eax, ebx, ecx, edx;
        __get_cpuid(1, &eax, &ebx, &ecx, &edx);
        return (ecx & (1 << 28)) != 0;
    }
    
    bool detectAVX2Support() override {
        unsigned int eax, ebx, ecx, edx;
        __get_cpuid(7, &eax, &ebx, &ecx, &edx);
        return (ebx & (1 << 5)) != 0;
    }
    
    bool detectAVX512Support() override {
        unsigned int eax, ebx, ecx, edx;
        __get_cpuid(7, &eax, &ebx, &ecx, &edx);
        return (ebx & (1 << 16)) != 0;
    }
    
    bool detectNEONSupport() override {
#ifdef __aarch64__
        return true;
#else
        return false;
#endif
    }
};

#elif defined(_WIN32)
// Windows平台硬件特性检测
class WindowsHardwareDetector : public HardwareFeatureDetector {
public:
    bool detectTSNSupport() override {
        // Windows平台TSN检测
        // 使用GetAdaptersAddresses获取网络适配器信息
        
        PIP_ADAPTER_ADDRESSES pAdapterAddresses = NULL;
        ULONG outBufLen = 0;
        ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
        
        // 第一次调用获取所需缓冲区大小
        if (GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAdapterAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
            pAdapterAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
            if (pAdapterAddresses) {
                // 第二次调用获取适配器信息
                if (GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAdapterAddresses, &outBufLen) == NO_ERROR) {
                    PIP_ADAPTER_ADDRESSES pCurrentAdapter = pAdapterAddresses;
                    while (pCurrentAdapter) {
                        // 检查适配器名称和描述，寻找支持TSN的适配器
                        if (pCurrentAdapter->Description) {
                            std::string description(pCurrentAdapter->Description);
                            // 检查描述中是否包含TSN相关关键词
                            if (description.find("TSN") != std::string::npos || 
                                description.find("Time-Sensitive") != std::string::npos ||
                                description.find("802.1Qbv") != std::string::npos) {
                                free(pAdapterAddresses);
                                return true;
                            }
                        }
                        pCurrentAdapter = pCurrentAdapter->Next;
                    }
                }
                free(pAdapterAddresses);
            }
        }
        
        // 尝试检查注册表中的网络适配器特性
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                        "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}", 
                        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD index = 0;
            char subKeyName[256];
            DWORD subKeyNameSize = sizeof(subKeyName);
            
            while (RegEnumKeyEx(hKey, index, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                // 检查每个网络适配器的特性
                HKEY hSubKey;
                if (RegOpenKeyEx(hKey, subKeyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                    char driverDesc[256];
                    DWORD driverDescSize = sizeof(driverDesc);
                    if (RegQueryValueEx(hSubKey, "DriverDesc", NULL, NULL, (LPBYTE)driverDesc, &driverDescSize) == ERROR_SUCCESS) {
                        std::string description(driverDesc);
                        if (description.find("TSN") != std::string::npos || 
                            description.find("Time-Sensitive") != std::string::npos ||
                            description.find("802.1Qbv") != std::string::npos) {
                            RegCloseKey(hSubKey);
                            RegCloseKey(hKey);
                            return true;
                        }
                    }
                    RegCloseKey(hSubKey);
                }
                index++;
                subKeyNameSize = sizeof(subKeyName);
            }
            RegCloseKey(hKey);
        }
        
        return false;
    }
    
    bool detectSSE2Support() override {
        int CPUInfo[4] = { -1 };
        __cpuid(CPUInfo, 1);
        return (CPUInfo[3] & (1 << 26)) != 0;
    }
    
    bool detectAVXSupport() override {
        int CPUInfo[4] = { -1 };
        __cpuid(CPUInfo, 1);
        return (CPUInfo[2] & (1 << 28)) != 0;
    }
    
    bool detectAVX2Support() override {
        int CPUInfo[4] = { -1 };
        __cpuid(CPUInfo, 7);
        return (CPUInfo[1] & (1 << 5)) != 0;
    }
    
    bool detectAVX512Support() override {
        int CPUInfo[4] = { -1 };
        __cpuid(CPUInfo, 7);
        return (CPUInfo[1] & (1 << 16)) != 0;
    }
    
    bool detectNEONSupport() override {
        return false;
    }
};

#else
// 默认硬件特性检测
class DefaultHardwareDetector : public HardwareFeatureDetector {
public:
    bool detectTSNSupport() override {
        return false;
    }
    
    bool detectSSE2Support() override {
        return false;
    }
    
    bool detectAVXSupport() override {
        return false;
    }
    
    bool detectAVX2Support() override {
        return false;
    }
    
    bool detectAVX512Support() override {
        return false;
    }
    
    bool detectNEONSupport() override {
#ifdef __aarch64__
        return true;
#else
        return false;
#endif
    }
};
#endif

// 平台能力管理器（单例模式）
class PlatformCapabilitiesManager {
public:
    static PlatformCapabilitiesManager& instance() {
        static PlatformCapabilitiesManager instance;
        return instance;
    }
    
    PlatformCapabilities getPlatformCapabilities() {
        if (!initialized_) {
            initialize();
        }
        return capabilities_;
    }
    
    bool hasFeature(const std::string& feature) {
        if (!initialized_) {
            initialize();
        }
        
        if (feature == "TSN") return capabilities_.hasTSNSupport;
        if (feature == "SSE2") return capabilities_.hasSSE2;
        if (feature == "AVX") return capabilities_.hasAVX;
        if (feature == "AVX2") return capabilities_.hasAVX2;
        if (feature == "AVX512") return capabilities_.hasAVX512;
        if (feature == "NEON") return capabilities_.hasNEON;
        if (feature == "SharedMemory") return capabilities_.hasSharedMemory;
        if (feature == "RealtimeScheduling") return capabilities_.hasRealtimeScheduling;
        if (feature == "CpuAffinity") return capabilities_.hasCpuAffinity;
        if (feature == "MemoryLocking") return capabilities_.hasMemoryLocking;
        if (feature == "HighPrecisionTimer") return capabilities_.hasHighPrecisionTimer;
        if (feature == "ZeroCopySupport") return capabilities_.hasZeroCopySupport;
        if (feature == "LockFreeQueue") return capabilities_.hasLockFreeQueue;
        
        return false;
    }
    
private:
    PlatformCapabilitiesManager() : initialized_(false), detector_(createDetector()) {}
    
    void initialize() {
        // 平台基本信息
        capabilities_.platformName = AURORART_PLATFORM_NAME;
        capabilities_.archName = AURORART_ARCH_NAME;
        capabilities_.compilerName = AURORART_COMPILER_NAME;
        
        // 获取 OS 版本
        auto* platform = PlatformManager::instance().getPlatform();
        if (platform) {
            capabilities_.osVersion = platform->getOSVersion();
            capabilities_.numberOfCores = platform->getNumberOfCores();
        } else {
            capabilities_.osVersion = "unknown";
            capabilities_.numberOfCores = 1;
        }
        
        // 缓存行大小
        capabilities_.cacheLineSize = AURORART_CACHE_LINE_SIZE;
        
        // 特性检测
#if defined(AURORART_HAS_POSIX_SHM) || defined(AURORART_PLATFORM_WINDOWS)
        capabilities_.hasSharedMemory = true;
#else
        capabilities_.hasSharedMemory = false;
#endif

#if defined(AURORART_HAS_REALTIME_SCHED)
        capabilities_.hasRealtimeScheduling = true;
#else
        capabilities_.hasRealtimeScheduling = false;
#endif

#if defined(AURORART_HAS_CPU_AFFINITY_PTHREAD) || \
    defined(AURORART_HAS_CPU_AFFINITY_MACH) || \
    defined(AURORART_HAS_CPU_AFFINITY_WIN)
        capabilities_.hasCpuAffinity = true;
#else
        capabilities_.hasCpuAffinity = false;
#endif

#if defined(AURORART_HAS_MLOCK) || defined(AURORART_HAS_VIRTUAL_LOCK)
        capabilities_.hasMemoryLocking = true;
#else
        capabilities_.hasMemoryLocking = false;
#endif

#if defined(AURORART_HAS_ASM_TIMESTAMP) || defined(AURORART_HAS_RDTSC)
        capabilities_.hasHighPrecisionTimer = true;
#else
        capabilities_.hasHighPrecisionTimer = false;
#endif

#if defined(AURORART_HAS_ZERO_COPY_SHM)
        capabilities_.hasZeroCopySupport = true;
#else
        capabilities_.hasZeroCopySupport = false;
#endif

#if defined(AURORART_HAS_LOCK_FREE_QUEUE)
        capabilities_.hasLockFreeQueue = true;
#else
        capabilities_.hasLockFreeQueue = false;
#endif

        // 动态检测硬件特性
        capabilities_.hasTSNSupport = detector_->detectTSNSupport();
        capabilities_.hasSSE2 = detector_->detectSSE2Support();
        capabilities_.hasAVX = detector_->detectAVXSupport();
        capabilities_.hasAVX2 = detector_->detectAVX2Support();
        capabilities_.hasAVX512 = detector_->detectAVX512Support();
        capabilities_.hasNEON = detector_->detectNEONSupport();
        
        initialized_ = true;
    }
    
    std::unique_ptr<HardwareFeatureDetector> createDetector() {
#ifdef __linux__
        return std::make_unique<LinuxHardwareDetector>();
#elif defined(_WIN32)
        return std::make_unique<WindowsHardwareDetector>();
#else
        return std::make_unique<DefaultHardwareDetector>();
#endif
    }
    
    bool initialized_;
    PlatformCapabilities capabilities_;
    std::unique_ptr<HardwareFeatureDetector> detector_;
};

PlatformCapabilities getPlatformCapabilities() {
    return PlatformCapabilitiesManager::instance().getPlatformCapabilities();
}

bool hasSharedMemorySupport() {
    return PlatformCapabilitiesManager::instance().hasFeature("SharedMemory");
}

bool hasRealtimeSchedulingSupport() {
    return PlatformCapabilitiesManager::instance().hasFeature("RealtimeScheduling");
}

bool hasZeroCopySupport() {
    return PlatformCapabilitiesManager::instance().hasFeature("ZeroCopySupport");
}

int getNumberOfCores() {
    auto caps = PlatformCapabilitiesManager::instance().getPlatformCapabilities();
    return caps.numberOfCores;
}

int getCacheLineSize() {
    return AURORART_CACHE_LINE_SIZE;
}

// 新增：获取硬件特性字符串描述
std::string getHardwareFeaturesString() {
    auto caps = PlatformCapabilitiesManager::instance().getPlatformCapabilities();
    std::string features = "Hardware Features: ";
    
    if (caps.hasTSNSupport) features += "TSN ";
    if (caps.hasSSE2) features += "SSE2 ";
    if (caps.hasAVX) features += "AVX ";
    if (caps.hasAVX2) features += "AVX2 ";
    if (caps.hasAVX512) features += "AVX512 ";
    if (caps.hasNEON) features += "NEON ";
    if (caps.hasHighPrecisionTimer) features += "HighPrecisionTimer ";
    if (caps.hasZeroCopySupport) features += "ZeroCopy ";
    if (caps.hasLockFreeQueue) features += "LockFreeQueue ";
    
    return features;
}

// 新增：检查特定硬件特性
bool hasHardwareFeature(const std::string& feature) {
    return PlatformCapabilitiesManager::instance().hasFeature(feature);
}

} // namespace platform
} // namespace aurorart
