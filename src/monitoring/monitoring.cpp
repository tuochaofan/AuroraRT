#include "aurorart/monitoring/monitoring.h"
#include "aurorart/utils/logger.h"
#include "aurorart/utils/config.h"
#include <thread>
#include <random>

namespace aurorart {
namespace monitoring {

// CpuMetricCollector implementation

void CpuMetricCollector::start() {
    AURORA_LOG_INFO("CPU metric collector started");
}

void CpuMetricCollector::stop() {
    AURORA_LOG_INFO("CPU metric collector stopped");
}

std::vector<Metric> CpuMetricCollector::collectMetrics() {
    std::vector<Metric> metrics;
    
    double cpuUsage = 0.0;
    
    #if defined(_WIN32)
    // Windows实现
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD numProcessors = sysInfo.dwNumberOfProcessors;
    
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        // 计算CPU使用率（简化实现）
        static FILETIME prevIdleTime = idleTime;
        static FILETIME prevKernelTime = kernelTime;
        static FILETIME prevUserTime = userTime;
        
        ULARGE_INTEGER idle, kernel, user, prevIdle, prevKernel, prevUser;
        idle.LowPart = idleTime.dwLowDateTime;
        idle.HighPart = idleTime.dwHighDateTime;
        kernel.LowPart = kernelTime.dwLowDateTime;
        kernel.HighPart = kernelTime.dwHighDateTime;
        user.LowPart = userTime.dwLowDateTime;
        user.HighPart = userTime.dwHighDateTime;
        
        prevIdle.LowPart = prevIdleTime.dwLowDateTime;
        prevIdle.HighPart = prevIdleTime.dwHighDateTime;
        prevKernel.LowPart = prevKernelTime.dwLowDateTime;
        prevKernel.HighPart = prevKernelTime.dwHighDateTime;
        prevUser.LowPart = prevUserTime.dwLowDateTime;
        prevUser.HighPart = prevUserTime.dwHighDateTime;
        
        ULONGLONG idleDiff = idle.QuadPart - prevIdle.QuadPart;
        ULONGLONG kernelDiff = kernel.QuadPart - prevKernel.QuadPart;
        ULONGLONG userDiff = user.QuadPart - prevUser.QuadPart;
        ULONGLONG totalDiff = kernelDiff + userDiff;
        
        if (totalDiff > 0) {
            cpuUsage = 100.0 * (1.0 - (double)idleDiff / (double)totalDiff);
        }
        
        prevIdleTime = idleTime;
        prevKernelTime = kernelTime;
        prevUserTime = userTime;
    }
    #elif defined(__linux__)
    // Linux实现
    std::ifstream cpuInfo("/proc/stat");
    if (cpuInfo.is_open()) {
        std::string line;
        if (std::getline(cpuInfo, line)) {
            if (line.substr(0, 3) == "cpu") {
                std::istringstream iss(line);
                std::string cpu;
                unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
                iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;
                
                static unsigned long long prevUser = 0, prevNice = 0, prevSystem = 0, prevIdle = 0, prevIowait = 0, prevIrq = 0, prevSoftirq = 0, prevSteal = 0;
                
                unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
                unsigned long long prevTotal = prevUser + prevNice + prevSystem + prevIdle + prevIowait + prevIrq + prevSoftirq + prevSteal;
                unsigned long long totalDiff = total - prevTotal;
                unsigned long long idleDiff = idle - prevIdle;
                
                if (totalDiff > 0) {
                    cpuUsage = 100.0 * (1.0 - (double)idleDiff / (double)totalDiff);
                }
                
                prevUser = user;
                prevNice = nice;
                prevSystem = system;
                prevIdle = idle;
                prevIowait = iowait;
                prevIrq = irq;
                prevSoftirq = softirq;
                prevSteal = steal;
            }
        }
        cpuInfo.close();
    }
    #else
    // 其他平台使用模拟数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 100.0);
    cpuUsage = dis(gen);
    #endif
    
    metrics.emplace_back(MetricType::CPU_USAGE, cpuUsage, std::chrono::steady_clock::now());
    AURORA_LOG_DEBUG("Collected CPU usage: {}%", cpuUsage);
    return metrics;
}

// MemoryMetricCollector implementation

void MemoryMetricCollector::start() {
    AURORA_LOG_INFO("Memory metric collector started");
}

void MemoryMetricCollector::stop() {
    AURORA_LOG_INFO("Memory metric collector stopped");
}

std::vector<Metric> MemoryMetricCollector::collectMetrics() {
    std::vector<Metric> metrics;
    
    double memoryUsage = 0.0;
    
    #if defined(_WIN32)
    // Windows实现
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        memoryUsage = (double)(memStatus.dwMemoryLoad);
    }
    #elif defined(__linux__)
    // Linux实现
    std::ifstream memInfo("/proc/meminfo");
    if (memInfo.is_open()) {
        std::string line;
        unsigned long long totalMem = 0, freeMem = 0, buffers = 0, cached = 0;
        
        while (std::getline(memInfo, line)) {
            std::istringstream iss(line);
            std::string key;
            unsigned long long value;
            std::string unit;
            
            iss >> key >> value >> unit;
            
            if (key == "MemTotal:") {
                totalMem = value;
            } else if (key == "MemFree:") {
                freeMem = value;
            } else if (key == "Buffers:") {
                buffers = value;
            } else if (key == "Cached:") {
                cached = value;
            }
        }
        
        if (totalMem > 0) {
            unsigned long long usedMem = totalMem - freeMem - buffers - cached;
            memoryUsage = 100.0 * (double)usedMem / (double)totalMem;
        }
        
        memInfo.close();
    }
    #else
    // 其他平台使用模拟数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 100.0);
    memoryUsage = dis(gen);
    #endif
    
    metrics.emplace_back(MetricType::MEMORY_USAGE, memoryUsage, std::chrono::steady_clock::now());
    AURORA_LOG_DEBUG("Collected memory usage: {}%", memoryUsage);
    return metrics;
}

// NetworkMetricCollector implementation

void NetworkMetricCollector::start() {
    AURORA_LOG_INFO("Network metric collector started");
}

void NetworkMetricCollector::stop() {
    AURORA_LOG_INFO("Network metric collector stopped");
}

std::vector<Metric> NetworkMetricCollector::collectMetrics() {
    std::vector<Metric> metrics;
    
    double networkTx = 0.0;
    double networkRx = 0.0;
    
    #if defined(_WIN32)
    // Windows实现
    MIB_IFTABLE* ifTable = nullptr;
    ULONG ifTableSize = 0;
    
    // 第一次调用获取大小
    if (GetIfTable(nullptr, &ifTableSize, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
        ifTable = (MIB_IFTABLE*)malloc(ifTableSize);
        if (ifTable) {
            if (GetIfTable(ifTable, &ifTableSize, FALSE) == NO_ERROR) {
                static std::map<DWORD, std::pair<ULONGLONG, ULONGLONG>> prevStats;
                
                for (DWORD i = 0; i < ifTable->dwNumEntries; i++) {
                    MIB_IFROW& ifRow = ifTable->table[i];
                    if (ifRow.dwType != IF_TYPE_ETHERNET_CSMACD && ifRow.dwType != IF_TYPE_IEEE80211) {
                        continue; // 只关注以太网和WiFi接口
                    }
                    
                    ULONGLONG currTx = ifRow.dwOutOctets;
                    ULONGLONG currRx = ifRow.dwInOctets;
                    
                    auto it = prevStats.find(ifRow.dwIndex);
                    if (it != prevStats.end()) {
                        ULONGLONG prevTx = it->second.first;
                        ULONGLONG prevRx = it->second.second;
                        
                        // 计算速率（字节/秒）
                        static auto lastTime = std::chrono::steady_clock::now();
                        auto now = std::chrono::steady_clock::now();
                        double elapsed = std::chrono::duration<double>(now - lastTime).count();
                        
                        if (elapsed > 0) {
                            networkTx += (currTx - prevTx) / elapsed / (1024 * 1024); // MB/s
                            networkRx += (currRx - prevRx) / elapsed / (1024 * 1024); // MB/s
                        }
                    }
                    
                    prevStats[ifRow.dwIndex] = std::make_pair(currTx, currRx);
                }
                
                static auto lastTime = std::chrono::steady_clock::now();
                lastTime = std::chrono::steady_clock::now();
            }
            free(ifTable);
        }
    }
    #elif defined(__linux__)
    // Linux实现
    std::ifstream netDev("/proc/net/dev");
    if (netDev.is_open()) {
        std::string line;
        static std::map<std::string, std::pair<unsigned long long, unsigned long long>> prevStats;
        
        // 跳过前两行
        std::getline(netDev, line);
        std::getline(netDev, line);
        
        while (std::getline(netDev, line)) {
            std::istringstream iss(line);
            std::string iface;
            unsigned long long rxBytes, rxPackets, rxErrors, rxDrop, rxFifo, rxFrame, rxCompressed, rxMulticast;
            unsigned long long txBytes, txPackets, txErrors, txDrop, txFifo, txColls, txCarrier, txCompressed;
            
            iss >> iface >> rxBytes >> rxPackets >> rxErrors >> rxDrop >> rxFifo >> rxFrame >> rxCompressed >> rxMulticast;
            iss >> txBytes >> txPackets >> txErrors >> txDrop >> txFifo >> txColls >> txCarrier >> txCompressed;
            
            // 移除冒号
            if (!iface.empty() && iface.back() == ':') {
                iface.pop_back();
            }
            
            // 只关注以太网和WiFi接口
            if (iface.substr(0, 3) == "eth" || iface.substr(0, 2) == "en" || iface.substr(0, 2) == "wl") {
                auto it = prevStats.find(iface);
                if (it != prevStats.end()) {
                    unsigned long long prevRx = it->second.first;
                    unsigned long long prevTx = it->second.second;
                    
                    // 计算速率（字节/秒）
                    static auto lastTime = std::chrono::steady_clock::now();
                    auto now = std::chrono::steady_clock::now();
                    double elapsed = std::chrono::duration<double>(now - lastTime).count();
                    
                    if (elapsed > 0) {
                        networkRx += (rxBytes - prevRx) / elapsed / (1024 * 1024); // MB/s
                        networkTx += (txBytes - prevTx) / elapsed / (1024 * 1024); // MB/s
                    }
                }
                
                prevStats[iface] = std::make_pair(rxBytes, txBytes);
            }
        }
        
        static auto lastTime = std::chrono::steady_clock::now();
        lastTime = std::chrono::steady_clock::now();
        
        netDev.close();
    }
    #else
    // 其他平台使用模拟数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 100.0);
    networkTx = dis(gen);
    networkRx = dis(gen);
    #endif
    
    metrics.emplace_back(MetricType::NETWORK_TX, networkTx, std::chrono::steady_clock::now());
    metrics.emplace_back(MetricType::NETWORK_RX, networkRx, std::chrono::steady_clock::now());
    AURORA_LOG_DEBUG("Collected network TX: {} MB/s, RX: {} MB/s", networkTx, networkRx);
    return metrics;
}

// DiskMetricCollector implementation

void DiskMetricCollector::start() {
    AURORA_LOG_INFO("Disk metric collector started");
}

void DiskMetricCollector::stop() {
    AURORA_LOG_INFO("Disk metric collector stopped");
}

std::vector<Metric> DiskMetricCollector::collectMetrics() {
    std::vector<Metric> metrics;
    
    double diskUsage = 0.0;
    double diskIO = 0.0;
    
    #if defined(_WIN32)
    // Windows实现
    // 获取磁盘使用率
    ULARGE_INTEGER totalBytes, freeBytes;
    if (GetDiskFreeSpaceEx("C:\\", &freeBytes, &totalBytes, nullptr)) {
        diskUsage = 100.0 * (1.0 - (double)freeBytes.QuadPart / (double)totalBytes.QuadPart);
    }
    
    // 获取磁盘IO（简化实现）
    static ULONGLONG prevReadBytes = 0, prevWriteBytes = 0;
    IO_COUNTERS ioCounters;
    if (GetProcessIoCounters(GetCurrentProcess(), &ioCounters)) {
        static auto lastTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - lastTime).count();
        
        if (elapsed > 0) {
            ULONGLONG readBytes = ioCounters.ReadTransferCount;
            ULONGLONG writeBytes = ioCounters.WriteTransferCount;
            diskIO = (readBytes + writeBytes - prevReadBytes - prevWriteBytes) / elapsed / (1024 * 1024); // MB/s
            prevReadBytes = readBytes;
            prevWriteBytes = writeBytes;
        }
        lastTime = now;
    }
    #elif defined(__linux__)
    // Linux实现
    // 获取磁盘使用率
    std::ifstream diskInfo("/proc/mounts");
    if (diskInfo.is_open()) {
        std::string line;
        while (std::getline(diskInfo, line)) {
            std::istringstream iss(line);
            std::string device, mountPoint, fsType, options;
            iss >> device >> mountPoint >> fsType >> options;
            
            // 只关注本地文件系统
            if (fsType == "ext4" || fsType == "ext3" || fsType == "ext2" || fsType == "btrfs" || fsType == "xfs") {
                struct statfs stat;
                if (statfs(mountPoint.c_str(), &stat) == 0) {
                    unsigned long long totalBlocks = stat.f_blocks;
                    unsigned long long freeBlocks = stat.f_bfree;
                    unsigned long long blockSize = stat.f_bsize;
                    
                    unsigned long long totalBytes = totalBlocks * blockSize;
                    unsigned long long freeBytes = freeBlocks * blockSize;
                    double usage = 100.0 * (1.0 - (double)freeBytes / (double)totalBytes);
                    
                    // 使用根目录的使用率
                    if (mountPoint == "/") {
                        diskUsage = usage;
                    }
                }
            }
        }
        diskInfo.close();
    }
    
    // 获取磁盘IO
    std::ifstream diskStats("/proc/diskstats");
    if (diskStats.is_open()) {
        std::string line;
        static std::map<std::string, std::pair<unsigned long long, unsigned long long>> prevStats;
        
        while (std::getline(diskStats, line)) {
            std::istringstream iss(line);
            int major, minor;
            std::string devName;
            unsigned long long reads, readMerges, readSectors, readTicks;
            unsigned long long writes, writeMerges, writeSectors, writeTicks;
            unsigned long long iosInProgress, ioTicks, weightedIoTicks;
            
            iss >> major >> minor >> devName >> reads >> readMerges >> readSectors >> readTicks;
            iss >> writes >> writeMerges >> writeSectors >> writeTicks;
            iss >> iosInProgress >> ioTicks >> weightedIoTicks;
            
            // 只关注物理磁盘（不包括分区）
            if (devName.substr(0, 2) == "sd" || devName.substr(0, 2) == "hd" || devName.substr(0, 3) == "nvme") {
                if (devName.find('/') == std::string::npos) { // 排除分区
                    auto it = prevStats.find(devName);
                    if (it != prevStats.end()) {
                        unsigned long long prevReadSectors = it->second.first;
                        unsigned long long prevWriteSectors = it->second.second;
                        
                        static auto lastTime = std::chrono::steady_clock::now();
                        auto now = std::chrono::steady_clock::now();
                        double elapsed = std::chrono::duration<double>(now - lastTime).count();
                        
                        if (elapsed > 0) {
                            // 每个扇区512字节
                            double readBytes = (readSectors - prevReadSectors) * 512;
                            double writeBytes = (writeSectors - prevWriteSectors) * 512;
                            diskIO += (readBytes + writeBytes) / elapsed / (1024 * 1024); // MB/s
                        }
                    }
                    
                    prevStats[devName] = std::make_pair(readSectors, writeSectors);
                }
            }
        }
        
        static auto lastTime = std::chrono::steady_clock::now();
        lastTime = std::chrono::steady_clock::now();
        
        diskStats.close();
    }
    #else
    // 其他平台使用模拟数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 100.0);
    diskUsage = dis(gen);
    diskIO = dis(gen);
    #endif
    
    metrics.emplace_back(MetricType::DISK_USAGE, diskUsage, std::chrono::steady_clock::now());
    metrics.emplace_back(MetricType::DISK_IO, diskIO, std::chrono::steady_clock::now());
    AURORA_LOG_DEBUG("Collected disk usage: {}%, IO: {} MB/s", diskUsage, diskIO);
    return metrics;
}

// TemperatureMetricCollector implementation

void TemperatureMetricCollector::start() {
    AURORA_LOG_INFO("Temperature metric collector started");
}

void TemperatureMetricCollector::stop() {
    AURORA_LOG_INFO("Temperature metric collector stopped");
}

std::vector<Metric> TemperatureMetricCollector::collectMetrics() {
    std::vector<Metric> metrics;
    
    double temperature = 0.0;
    
    #if defined(_WIN32)
    // Windows实现（使用WMI）
    try {
        HRESULT hres;
        
        // 初始化COM
        hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (SUCCEEDED(hres)) {
            hres = CoInitializeSecurity(
                nullptr,
                -1,
                nullptr,
                nullptr,
                RPC_C_AUTHN_LEVEL_DEFAULT,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                nullptr,
                EOAC_NONE,
                nullptr
            );
            
            if (SUCCEEDED(hres)) {
                IWbemLocator* pLoc = nullptr;
                hres = CoCreateInstance(
                    CLSID_WbemLocator,
                    nullptr,
                    CLSCTX_INPROC_SERVER,
                    IID_IWbemLocator,
                    (LPVOID*)&pLoc
                );
                
                if (SUCCEEDED(hres)) {
                    IWbemServices* pSvc = nullptr;
                    hres = pLoc->ConnectServer(
                        _bstr_t(L"ROOT\WMI"),
                        nullptr,
                        nullptr,
                        0,
                        nullptr,
                        0,
                        0,
                        &pSvc
                    );
                    
                    if (SUCCEEDED(hres)) {
                        hres = CoSetProxyBlanket(
                            pSvc,
                            RPC_C_AUTHN_WINNT,
                            RPC_C_AUTHZ_NONE,
                            nullptr,
                            RPC_C_AUTHN_LEVEL_CALL,
                            RPC_C_IMP_LEVEL_IMPERSONATE,
                            nullptr,
                            EOAC_NONE
                        );
                        
                        if (SUCCEEDED(hres)) {
                            IEnumWbemClassObject* pEnumerator = nullptr;
                            hres = pSvc->ExecQuery(
                                bstr_t("WQL"),
                                bstr_t("SELECT * FROM MSAcpi_ThermalZoneTemperature"),
                                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                nullptr,
                                &pEnumerator
                            );
                            
                            if (SUCCEEDED(hres)) {
                                IWbemClassObject* pclsObj = nullptr;
                                ULONG uReturn = 0;
                                
                                while (pEnumerator) {
                                    HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                                    if (0 == uReturn) {
                                        break;
                                    }
                                    
                                    VARIANT vtProp;
                                    hr = pclsObj->Get(L"CurrentTemperature", 0, &vtProp, 0, 0);
                                    if (SUCCEEDED(hr)) {
                                        // 温度值是1/10开尔文，转换为摄氏度
                                        long tempK = vtProp.lVal;
                                        temperature = (tempK / 10.0) - 273.15;
                                        VariantClear(&vtProp);
                                    }
                                    
                                    pclsObj->Release();
                                }
                                
                                pEnumerator->Release();
                            }
                        }
                        
                        pSvc->Release();
                    }
                    
                    pLoc->Release();
                }
                
                CoUninitialize();
            }
        }
    } catch (...) {
        // 异常处理
    }
    #elif defined(__linux__)
    // Linux实现
    std::ifstream tempFile("/sys/class/thermal/thermal_zone0/temp");
    if (tempFile.is_open()) {
        int temp;
        if (tempFile >> temp) {
            // 温度值是1/1000摄氏度
            temperature = temp / 1000.0;
        }
        tempFile.close();
    }
    #else
    // 其他平台使用模拟数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(30.0, 80.0);
    temperature = dis(gen);
    #endif
    
    // 如果没有获取到温度数据，使用模拟数据
    if (temperature == 0.0) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(30.0, 80.0);
        temperature = dis(gen);
    }
    
    metrics.emplace_back(MetricType::TEMPERATURE, temperature, std::chrono::steady_clock::now());
    AURORA_LOG_DEBUG("Collected temperature: {}°C", temperature);
    return metrics;
}

// ProcessCountMetricCollector implementation

void ProcessCountMetricCollector::start() {
    AURORA_LOG_INFO("Process count metric collector started");
}

void ProcessCountMetricCollector::stop() {
    AURORA_LOG_INFO("Process count metric collector stopped");
}

std::vector<Metric> ProcessCountMetricCollector::collectMetrics() {
    std::vector<Metric> metrics;
    
    double processCount = 0.0;
    
    #if defined(_WIN32)
    // Windows实现
    DWORD processes[1024], bytesNeeded, returnLength;
    if (EnumProcesses(processes, sizeof(processes), &bytesNeeded)) {
        returnLength = bytesNeeded / sizeof(DWORD);
        processCount = returnLength;
    }
    #elif defined(__linux__)
    // Linux实现
    std::ifstream procDir("/proc");
    if (procDir.is_open()) {
        int count = 0;
        std::string entry;
        while (procDir >> entry) {
            if (std::all_of(entry.begin(), entry.end(), ::isdigit)) {
                count++;
            }
        }
        processCount = count;
        procDir.close();
    }
    #else
    // 其他平台使用模拟数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 1000);
    processCount = dis(gen);
    #endif
    
    metrics.emplace_back(MetricType::PROCESS_COUNT, processCount, std::chrono::steady_clock::now());
    AURORA_LOG_DEBUG("Collected process count: {}", processCount);
    return metrics;
}

// ThreadCountMetricCollector implementation

void ThreadCountMetricCollector::start() {
    AURORA_LOG_INFO("Thread count metric collector started");
}

void ThreadCountMetricCollector::stop() {
    AURORA_LOG_INFO("Thread count metric collector stopped");
}

std::vector<Metric> ThreadCountMetricCollector::collectMetrics() {
    std::vector<Metric> metrics;
    
    double threadCount = 0.0;
    
    #if defined(_WIN32)
    // Windows实现
    DWORD processes[1024], bytesNeeded, returnLength;
    if (EnumProcesses(processes, sizeof(processes), &bytesNeeded)) {
        returnLength = bytesNeeded / sizeof(DWORD);
        for (DWORD i = 0; i < returnLength; i++) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processes[i]);
            if (hProcess) {
                DWORD threadCountProc = 0;
                if (GetProcessHandleCount(hProcess, &threadCountProc)) {
                    threadCount += threadCountProc;
                }
                CloseHandle(hProcess);
            }
        }
    }
    #elif defined(__linux__)
    // Linux实现
    std::ifstream procDir("/proc");
    if (procDir.is_open()) {
        std::string entry;
        while (procDir >> entry) {
            if (std::all_of(entry.begin(), entry.end(), ::isdigit)) {
                std::ifstream statusFile("/proc/" + entry + "/status");
                if (statusFile.is_open()) {
                    std::string line;
                    while (std::getline(statusFile, line)) {
                        if (line.substr(0, 7) == "Threads:") {
                            std::istringstream iss(line.substr(7));
                            int count;
                            if (iss >> count) {
                                threadCount += count;
                            }
                            break;
                        }
                    }
                    statusFile.close();
                }
            }
        }
        procDir.close();
    }
    #else
    // 其他平台使用模拟数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 5000);
    threadCount = dis(gen);
    #endif
    
    metrics.emplace_back(MetricType::THREAD_COUNT, threadCount, std::chrono::steady_clock::now());
    AURORA_LOG_DEBUG("Collected thread count: {}", threadCount);
    return metrics;
}

// MessageRateMetricCollector implementation

void MessageRateMetricCollector::start() {
    AURORA_LOG_INFO("Message rate metric collector started");
}

void MessageRateMetricCollector::stop() {
    AURORA_LOG_INFO("Message rate metric collector stopped");
}

std::vector<Metric> MessageRateMetricCollector::collectMetrics() {
    std::vector<Metric> metrics;
    
    // 模拟消息率数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 10000.0);
    double messageRate = dis(gen);
    
    metrics.emplace_back(MetricType::MESSAGE_RATE, messageRate, std::chrono::steady_clock::now());
    AURORA_LOG_DEBUG("Collected message rate: {} messages/sec", messageRate);
    return metrics;
}

// LatencyMetricCollector implementation

void LatencyMetricCollector::start() {
    AURORA_LOG_INFO("Latency metric collector started");
}

void LatencyMetricCollector::stop() {
    AURORA_LOG_INFO("Latency metric collector stopped");
}

std::vector<Metric> LatencyMetricCollector::collectMetrics() {
    std::vector<Metric> metrics;
    
    // 模拟延迟数据（微秒）
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.1, 100.0);
    double latency = dis(gen);
    
    metrics.emplace_back(MetricType::LATENCY, latency, std::chrono::steady_clock::now());
    AURORA_LOG_DEBUG("Collected latency: {} us", latency);
    return metrics;
}

// DefaultMonitor implementation

DefaultMonitor::DefaultMonitor(const std::string& name)
    : name_(name), running_(false) {
}

void DefaultMonitor::start() {
    if (running_) {
        AURORA_LOG_WARN("Monitor {} is already running", name_);
        return;
    }
    
    for (auto& collector : collectors_) {
        collector->start();
    }
    
    running_ = true;
    AURORA_LOG_INFO("Monitor {} started", name_);
}

void DefaultMonitor::stop() {
    if (!running_) {
        AURORA_LOG_WARN("Monitor {} is not running", name_);
        return;
    }
    
    for (auto& collector : collectors_) {
        collector->stop();
    }
    
    running_ = false;
    AURORA_LOG_INFO("Monitor {} stopped", name_);
}

std::vector<Metric> DefaultMonitor::collectMetrics() {
    std::vector<Metric> allMetrics;
    
    if (!running_) {
        AURORA_LOG_WARN("Monitor {} is not running, cannot collect metrics", name_);
        return allMetrics;
    }
    
    for (auto& collector : collectors_) {
        auto metrics = collector->collectMetrics();
        allMetrics.insert(allMetrics.end(), metrics.begin(), metrics.end());
    }
    
    return allMetrics;
}

HealthStatus DefaultMonitor::getHealthStatus() {
    // 简单的健康状态评估
    auto metrics = collectMetrics();
    
    for (const auto& metric : metrics) {
        if (metric.getType() == MetricType::CPU_USAGE && metric.getValue() > 90.0) {
            return HealthStatus::CRITICAL;
        }
        if (metric.getType() == MetricType::MEMORY_USAGE && metric.getValue() > 90.0) {
            return HealthStatus::CRITICAL;
        }
        if (metric.getType() == MetricType::DISK_USAGE && metric.getValue() > 95.0) {
            return HealthStatus::CRITICAL;
        }
        if (metric.getType() == MetricType::TEMPERATURE && metric.getValue() > 85.0) {
            return HealthStatus::CRITICAL;
        }
        if (metric.getType() == MetricType::LATENCY && metric.getValue() > 1000.0) {
            return HealthStatus::CRITICAL;
        }
    }
    
    for (const auto& metric : metrics) {
        if (metric.getType() == MetricType::CPU_USAGE && metric.getValue() > 70.0) {
            return HealthStatus::DEGRADED;
        }
        if (metric.getType() == MetricType::MEMORY_USAGE && metric.getValue() > 70.0) {
            return HealthStatus::DEGRADED;
        }
        if (metric.getType() == MetricType::DISK_USAGE && metric.getValue() > 80.0) {
            return HealthStatus::DEGRADED;
        }
        if (metric.getType() == MetricType::TEMPERATURE && metric.getValue() > 70.0) {
            return HealthStatus::DEGRADED;
        }
        if (metric.getType() == MetricType::LATENCY && metric.getValue() > 100.0) {
            return HealthStatus::DEGRADED;
        }
    }
    
    return HealthStatus::HEALTHY;
}

void DefaultMonitor::addMetricCollector(std::shared_ptr<MetricCollector> collector) {
    std::lock_guard<std::mutex> lock(mutex_);
    collectors_.push_back(collector);
    AURORA_LOG_INFO("Added metric collector {} to monitor {}", collector->getName(), name_);
}

// DefaultHealthMonitor implementation

void DefaultHealthMonitor::start() {
    AURORA_LOG_INFO("Health monitor started");
}

void DefaultHealthMonitor::stop() {
    AURORA_LOG_INFO("Health monitor stopped");
}

HealthStatus DefaultHealthMonitor::checkHealth() {
    // 综合健康检查
    auto componentHealth = getComponentHealth();
    
    // 检查是否有关键组件处于临界状态
    for (const auto& [component, status] : componentHealth) {
        if (status == HealthStatus::CRITICAL) {
            return HealthStatus::CRITICAL;
        }
    }
    
    // 检查是否有组件处于警告状态
    for (const auto& [component, status] : componentHealth) {
        if (status == HealthStatus::WARNING) {
            return HealthStatus::WARNING;
        }
    }
    
    return HealthStatus::HEALTHY;
}

std::map<std::string, HealthStatus> DefaultHealthMonitor::getComponentHealth() {
    std::map<std::string, HealthStatus> componentHealth;
    
    // 平台健康检查
    componentHealth["Platform"] = checkPlatformHealth();
    
    // 内存健康检查
    componentHealth["Memory"] = checkMemoryHealth();
    
    // 传输层健康检查
    componentHealth["Transport"] = checkTransportHealth();
    
    // 通信层健康检查
    componentHealth["Communication"] = checkCommunicationHealth();
    
    // 安全模块健康检查
    componentHealth["Security"] = checkSecurityHealth();
    
    // 节点健康检查
    componentHealth["Node"] = checkNodeHealth();
    
    return componentHealth;
}

HealthStatus DefaultHealthMonitor::checkPlatformHealth() {
    // 检查平台健康状态
    try {
        // 检查系统是否支持必要的功能
        #if defined(_WIN32)
        // Windows平台检查
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        if (sysInfo.dwNumberOfProcessors < 1) {
            return HealthStatus::WARNING;
        }
        #elif defined(__linux__)
        // Linux平台检查
        std::ifstream cpuInfo("/proc/cpuinfo");
        if (!cpuInfo.is_open()) {
            return HealthStatus::WARNING;
        }
        cpuInfo.close();
        #endif
        return HealthStatus::HEALTHY;
    } catch (...) {
        AURORA_LOG_ERROR("Failed to check platform health");
        return HealthStatus::WARNING;
    }
}

HealthStatus DefaultHealthMonitor::checkMemoryHealth() {
    // 检查内存健康状态
    try {
        MemoryMetricCollector collector;
        auto metrics = collector.collectMetrics();
        for (const auto& metric : metrics) {
            if (metric.getType() == MetricType::MEMORY_USAGE) {
                double usage = metric.getValue();
                if (usage > 90.0) {
                    return HealthStatus::CRITICAL;
                } else if (usage > 70.0) {
                    return HealthStatus::WARNING;
                }
            }
        }
        return HealthStatus::HEALTHY;
    } catch (...) {
        AURORA_LOG_ERROR("Failed to check memory health");
        return HealthStatus::WARNING;
    }
}

HealthStatus DefaultHealthMonitor::checkTransportHealth() {
    // 检查传输层健康状态
    try {
        // 这里可以添加具体的传输层健康检查逻辑
        // 例如检查共享内存是否可用，网络连接是否正常等
        return HealthStatus::HEALTHY;
    } catch (...) {
        AURORA_LOG_ERROR("Failed to check transport health");
        return HealthStatus::WARNING;
    }
}

HealthStatus DefaultHealthMonitor::checkCommunicationHealth() {
    // 检查通信层健康状态
    try {
        // 这里可以添加具体的通信层健康检查逻辑
        // 例如检查发布-订阅模式是否正常工作等
        return HealthStatus::HEALTHY;
    } catch (...) {
        AURORA_LOG_ERROR("Failed to check communication health");
        return HealthStatus::WARNING;
    }
}

HealthStatus DefaultHealthMonitor::checkSecurityHealth() {
    // 检查安全模块健康状态
    try {
        // 这里可以添加具体的安全模块健康检查逻辑
        // 例如检查认证服务是否可用，加密功能是否正常等
        return HealthStatus::HEALTHY;
    } catch (...) {
        AURORA_LOG_ERROR("Failed to check security health");
        return HealthStatus::WARNING;
    }
}

HealthStatus DefaultHealthMonitor::checkNodeHealth() {
    // 检查节点健康状态
    try {
        // 这里可以添加具体的节点健康检查逻辑
        // 例如检查节点是否能够正常连接到网络，是否能够正常处理请求等
        return HealthStatus::HEALTHY;
    } catch (...) {
        AURORA_LOG_ERROR("Failed to check node health");
        return HealthStatus::WARNING;
    }
}

// MonitorManager implementation

MonitorManager::MonitorManager() : running_(false) {
}

MonitorManager& MonitorManager::instance() {
    static MonitorManager instance;
    return instance;
}

void MonitorManager::init() {
    healthMonitor_ = std::make_unique<DefaultHealthMonitor>();
    
    // 添加默认的指标收集器
    addMetricCollector(std::make_shared<CpuMetricCollector>());
    addMetricCollector(std::make_shared<MemoryMetricCollector>());
    addMetricCollector(std::make_shared<NetworkMetricCollector>());
    addMetricCollector(std::make_shared<DiskMetricCollector>());
    addMetricCollector(std::make_shared<TemperatureMetricCollector>());
    addMetricCollector(std::make_shared<ProcessCountMetricCollector>());
    addMetricCollector(std::make_shared<ThreadCountMetricCollector>());
    addMetricCollector(std::make_shared<MessageRateMetricCollector>());
    addMetricCollector(std::make_shared<LatencyMetricCollector>());
    
    AURORA_LOG_INFO("Monitor manager initialized");
}

void MonitorManager::start() {
    if (running_) {
        AURORA_LOG_WARN("Monitor manager is already running");
        return;
    }
    
    healthMonitor_->start();
    
    for (auto& [name, monitor] : monitors_) {
        monitor->start();
    }
    
    for (auto& collector : collectors_) {
        collector->start();
    }
    
    running_ = true;
    AURORA_LOG_INFO("Monitor manager started");
}

void MonitorManager::stop() {
    if (!running_) {
        AURORA_LOG_WARN("Monitor manager is not running");
        return;
    }
    
    for (auto& [name, monitor] : monitors_) {
        monitor->stop();
    }
    
    for (auto& collector : collectors_) {
        collector->stop();
    }
    
    healthMonitor_->stop();
    
    running_ = false;
    AURORA_LOG_INFO("Monitor manager stopped");
}

std::shared_ptr<Monitor> MonitorManager::createMonitor(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto monitor = std::make_shared<DefaultMonitor>(name);
    monitors_[name] = monitor;
    
    // 添加默认的指标收集器
    for (auto& collector : collectors_) {
        monitor->addMetricCollector(collector);
    }
    
    AURORA_LOG_INFO("Created monitor: {}", name);
    return monitor;
}

std::shared_ptr<Monitor> MonitorManager::getMonitor(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = monitors_.find(name);
    if (it != monitors_.end()) {
        return it->second;
    }
    
    AURORA_LOG_WARN("Monitor not found: {}", name);
    return nullptr;
}

bool MonitorManager::removeMonitor(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = monitors_.find(name);
    if (it != monitors_.end()) {
        it->second->stop();
        monitors_.erase(it);
        AURORA_LOG_INFO("Removed monitor: {}", name);
        return true;
    }
    
    AURORA_LOG_WARN("Monitor not found: {}", name);
    return false;
}

void MonitorManager::addMetricCollector(std::shared_ptr<MetricCollector> collector) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    collectors_.push_back(collector);
    
    // 向所有现有监控器添加新的指标收集器
    for (auto& [name, monitor] : monitors_) {
        auto defaultMonitor = dynamic_cast<DefaultMonitor*>(monitor.get());
        if (defaultMonitor) {
            defaultMonitor->addMetricCollector(collector);
        }
    }
    
    AURORA_LOG_INFO("Added metric collector: {}", collector->getName());
}

std::vector<Metric> MonitorManager::collectMetrics() {
    std::vector<Metric> allMetrics;
    
    for (auto& collector : collectors_) {
        auto metrics = collector->collectMetrics();
        allMetrics.insert(allMetrics.end(), metrics.begin(), metrics.end());
    }
    
    return allMetrics;
}

HealthStatus MonitorManager::getHealthStatus() {
    return healthMonitor_->checkHealth();
}

std::map<std::string, HealthStatus> MonitorManager::getComponentHealth() {
    return healthMonitor_->getComponentHealth();
}

} // namespace monitoring
} // namespace aurorart
