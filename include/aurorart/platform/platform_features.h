#ifndef AURORART_PLATFORM_FEATURES_H
#define AURORART_PLATFORM_FEATURES_H

/**
 * @file platform_features.h
 * @brief AuroraRT 平台特性检测和功能宏定义
 * 
 * 参考 ZMQ、iceoryx2、ROS2 等成熟项目的跨平台实现经验
 * 提供统一的平台能力检测和特性宏
 */

// ============================================================================
// 平台检测宏
// ============================================================================

#if defined(__QNX__) || defined(__QNXNTO__)
    #define AURORART_PLATFORM_QNX 1
    #define AURORART_PLATFORM_NAME "QNX Neutrino"
    #define AURORART_PLATFORM_POSIX 1
    #define AURORART_PLATFORM_REALTIME 1
    
#elif defined(__linux__)
    #define AURORART_PLATFORM_LINUX 1
    #define AURORART_PLATFORM_NAME "Linux"
    #define AURORART_PLATFORM_POSIX 1
    #define AURORART_PLATFORM_REALTIME 1
    
#elif defined(_WIN32) || defined(_WIN64)
    #define AURORART_PLATFORM_WINDOWS 1
    #define AURORART_PLATFORM_NAME "Windows"
    #define AURORART_PLATFORM_WIN32 1
    
#elif defined(__APPLE__)
    #define AURORART_PLATFORM_MACOS 1
    #define AURORART_PLATFORM_NAME "macOS"
    #define AURORART_PLATFORM_POSIX 1
    #define AURORART_PLATFORM_BSD 1
    
#elif defined(__VXWORKS__)
    #define AURORART_PLATFORM_VXWORKS 1
    #define AURORART_PLATFORM_NAME "VxWorks"
    #define AURORART_PLATFORM_POSIX 1
    #define AURORART_PLATFORM_REALTIME 1
    
#else
    #define AURORART_PLATFORM_UNKNOWN 1
    #define AURORART_PLATFORM_NAME "Unknown"
#endif

// ============================================================================
// 编译器检测
// ============================================================================

#if defined(__GNUC__)
    #define AURORART_COMPILER_GCC 1
    #define AURORART_COMPILER_NAME "GCC"
    #define AURORART_COMPILER_VERSION __VERSION__
    
#elif defined(__clang__)
    #define AURORART_COMPILER_CLANG 1
    #define AURORART_COMPILER_NAME "Clang"
    #define AURORART_COMPILER_VERSION __clang_version__
    
#elif defined(_MSC_VER)
    #define AURORART_COMPILER_MSVC 1
    #define AURORART_COMPILER_NAME "MSVC"
    #define AURORART_COMPILER_VERSION _MSC_VER
    
#else
    #define AURORART_COMPILER_UNKNOWN 1
    #define AURORART_COMPILER_NAME "Unknown"
#endif

// ============================================================================
// 架构检测
// ============================================================================

#if defined(__x86_64__) || defined(_M_X64)
    #define AURORART_ARCH_X86_64 1
    #define AURORART_ARCH_NAME "x86_64"
    #define AURORART_ARCH_64BIT 1
    
#elif defined(__i386__) || defined(_M_IX86)
    #define AURORART_ARCH_X86 1
    #define AURORART_ARCH_NAME "x86"
    #define AURORART_ARCH_32BIT 1
    
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define AURORART_ARCH_ARM64 1
    #define AURORART_ARCH_NAME "ARM64"
    #define AURORART_ARCH_64BIT 1
    
#elif defined(__arm__) || defined(_M_ARM)
    #define AURORART_ARCH_ARM 1
    #define AURORART_ARCH_NAME "ARM"
    #define AURORART_ARCH_32BIT 1
    
#else
    #define AURORART_ARCH_UNKNOWN 1
    #define AURORART_ARCH_NAME "Unknown"
#endif

// ============================================================================
// 平台特性检测
// ============================================================================

// POSIX 共享内存支持
#if defined(AURORART_PLATFORM_POSIX)
    #define AURORART_HAS_POSIX_SHM 1
    #define AURORART_HAS_MMAP 1
    #define AURORART_HAS_PTHREAD 1
    #define AURORART_HAS_SEMAPHORE 1
#endif

// 实时调度支持
#if defined(AURORART_PLATFORM_REALTIME)
    #define AURORART_HAS_REALTIME_SCHED 1
    #define AURORART_HAS_SCHED_FIFO 1
    #define AURORART_HAS_CLOCK_GETTIME 1
#elif defined(AURORART_PLATFORM_WINDOWS)
    // Windows 非实时内核，不支持实时调度
    #define AURORART_HAS_REALTIME_SCHED 0
    #define AURORART_HAS_SCHED_FIFO 0
    // Windows 有 GetSystemTimePreciseAsFileTime 等替代方案
    #define AURORART_HAS_CLOCK_GETTIME 0
#endif

// 内存锁定支持
#if defined(AURORART_PLATFORM_POSIX)
    #define AURORART_HAS_MLOCK 1
#elif defined(AURORART_PLATFORM_WINDOWS)
    #define AURORART_HAS_VIRTUAL_LOCK 1
#endif

// CPU 亲和性支持
#if defined(AURORART_PLATFORM_LINUX) || defined(AURORART_PLATFORM_QNX) || defined(AURORART_PLATFORM_VXWORKS)
    #define AURORART_HAS_CPU_AFFINITY_PTHREAD 1
#elif defined(AURORART_PLATFORM_MACOS)
    #define AURORART_HAS_CPU_AFFINITY_MACH 1
#elif defined(AURORART_PLATFORM_WINDOWS)
    #define AURORART_HAS_CPU_AFFINITY_WIN 1
#endif

// 高精度时间戳支持
#if defined(AURORART_ARCH_X86) || defined(AURORART_ARCH_X86_64)
    #define AURORART_HAS_RDTSC 1
    #define AURORART_HAS_ASM_TIMESTAMP 1
#endif

// 零拷贝支持
#if defined(AURORART_PLATFORM_POSIX)
    #define AURORART_HAS_ZERO_COPY_SHM 1
    #define AURORART_HAS_LOCK_FREE_QUEUE 1
#endif

// ============================================================================
// 性能优化宏
// ============================================================================

// 分支预测优化
#if defined(AURORART_COMPILER_GCC) || defined(AURORART_COMPILER_CLANG)
    #define AURORART_LIKELY(x) __builtin_expect(!!(x), 1)
    #define AURORART_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define AURORART_LIKELY(x) (x)
    #define AURORART_UNLIKELY(x) (x)
#endif

// 内存对齐
#if defined(AURORART_COMPILER_MSVC)
    #define AURORART_ALIGNAS(n) __declspec(align(n))
    #define AURORART_ALIGNED_ALLOC(size, alignment) _aligned_malloc(size, alignment)
    #define AURORART_ALIGNED_FREE(ptr) _aligned_free(ptr)
#else
    #define AURORART_ALIGNAS(n) __attribute__((aligned(n)))
    #define AURORART_ALIGNED_ALLOC(size, alignment) aligned_alloc(alignment, size)
    #define AURORART_ALIGNED_FREE(ptr) free(ptr)
#endif

// 缓存行大小
#define AURORART_CACHE_LINE_SIZE 64
#define AURORART_CACHE_ALIGNED AURORART_ALIGNAS(AURORART_CACHE_LINE_SIZE)

// 内联优化
#if defined(AURORART_COMPILER_MSVC)
    #define AURORART_FORCE_INLINE __forceinline
    #define AURORART_NOINLINE __declspec(noinline)
#else
    #define AURORART_FORCE_INLINE inline __attribute__((always_inline))
    #define AURORART_NOINLINE __attribute__((noinline))
#endif

// 线程局部存储
#if defined(AURORART_COMPILER_MSVC)
    #define AURORART_THREAD_LOCAL __declspec(thread)
#else
    #define AURORART_THREAD_LOCAL __thread
#endif

// ============================================================================
// 平台能力查询接口（运行时）
// ============================================================================

namespace aurorart {
namespace platform {

struct PlatformCapabilities {
    bool hasSharedMemory;
    bool hasRealtimeScheduling;
    bool hasCpuAffinity;
    bool hasMemoryLocking;
    bool hasHighPrecisionTimer;
    bool hasZeroCopySupport;
    bool hasLockFreeQueue;
    bool hasTSNSupport;
    bool hasSSE2;
    bool hasAVX;
    bool hasAVX2;
    bool hasAVX512;
    bool hasNEON;
    int numberOfCores;
    int cacheLineSize;
    std::string platformName;
    std::string osVersion;
    std::string archName;
    std::string compilerName;
};

/**
 * @brief 获取当前平台能力信息
 * @return PlatformCapabilities 平台能力结构体
 */
PlatformCapabilities getPlatformCapabilities();

/**
 * @brief 检查是否支持共享内存通信
 * @return true 如果支持
 */
bool hasSharedMemorySupport();

/**
 * @brief 检查是否支持实时调度
 * @return true 如果支持
 */
bool hasRealtimeSchedulingSupport();

/**
 * @brief 检查是否支持零拷贝传输
 * @return true 如果支持
 */
bool hasZeroCopySupport();

/**
 * @brief 获取 CPU 核心数
 * @return int CPU 核心数量
 */
int getNumberOfCores();

/**
 * @brief 获取缓存行大小
 * @return int 缓存行大小（字节）
 */
int getCacheLineSize();

/**
 * @brief 获取硬件特性字符串描述
 * @return std::string 硬件特性描述字符串
 */
std::string getHardwareFeaturesString();

/**
 * @brief 检查特定硬件特性
 * @param feature 特性名称
 * @return true 如果支持该特性
 */
bool hasHardwareFeature(const std::string& feature);

} // namespace platform
} // namespace aurorart

#endif // AURORART_PLATFORM_FEATURES_H
