#ifndef AURORART_PLATFORM_ABSTRACTION_H
#define AURORART_PLATFORM_ABSTRACTION_H

#include <string>
#include <memory>
#include <vector>

// 内联函数宏定义
#if defined(_MSC_VER)
#define always_inline __forceinline
#elif defined(__GNUC__)
#define always_inline __attribute__((always_inline))
#else
#define always_inline inline
#endif

namespace aurorart {
namespace platform {

// 线程函数类型
typedef void* (*ThreadFunc)(void*);

class PlatformAbstraction {
public:
    virtual ~PlatformAbstraction() = default;
    
    // 内存管理
    virtual void* allocateMemory(size_t size) = 0;
    virtual void freeMemory(void* ptr) = 0;
    
    // 共享内存管理
    virtual void* createSharedMemory(const std::string& name, size_t size) = 0;
    virtual void destroySharedMemory(const std::string& name) = 0;
    virtual void* mapSharedMemory(const std::string& name, size_t size) = 0;
    virtual void unmapSharedMemory(void* ptr, size_t size) = 0;
    
    // 网络管理
    virtual int createSocket(int domain, int type, int protocol) = 0;
    virtual int closeSocket(int sockfd) = 0;
    virtual bool setSocketNonBlocking(int sockfd) = 0;
    virtual bool setSocketReuseAddr(int sockfd) = 0;
    
    // 线程管理
    virtual void* createThread(ThreadFunc func, void* arg) = 0;
    virtual void joinThread(void* thread) = 0;
    // 带优先级的线程创建
    virtual void* createThreadWithPriority(ThreadFunc func, void* arg, int priority) = 0;
    
    // 互斥量管理
    virtual void* createMutex() = 0;
    virtual void destroyMutex(void* mutex) = 0;
    virtual void lockMutex(void* mutex) = 0;
    virtual void unlockMutex(void* mutex) = 0;
    
    // 条件变量管理
    virtual void* createCondition() = 0;
    virtual void destroyCondition(void* cond) = 0;
    virtual void waitCondition(void* cond, void* mutex) = 0;
    virtual void signalCondition(void* cond) = 0;
    virtual void broadcastCondition(void* cond) = 0;
    
    // 高精度定时器
    virtual uint64_t getTimestampUs() = 0;
    
    // 文件系统操作
    virtual bool fileExists(const std::string& path) = 0;
    virtual bool createDirectory(const std::string& path) = 0;
    virtual bool removeFile(const std::string& path) = 0;
    
    // 进程间通信增强
    virtual int createSemaphore(const std::string& name, unsigned int initialCount) = 0;
    virtual void destroySemaphore(int semid) = 0;
    virtual void waitSemaphore(int semid) = 0;
    virtual void signalSemaphore(int semid) = 0;
    
    // 内存锁定（实时系统关键功能）
    virtual bool lockMemory(void* addr, size_t len) = 0;
    virtual bool unlockMemory(void* addr, size_t len) = 0;
    
    // 线程优先级设置（实时调度）
    virtual bool setThreadPriority(void* thread, int priority) = 0;
    virtual int getThreadPriority(void* thread) = 0;
    
    // CPU 亲和性设置
    virtual bool setThreadAffinity(void* thread, int cpuCore) = 0;
    
    // 平台信息查询
    virtual std::string getPlatformName() = 0;
    virtual std::string getOSVersion() = 0;
    virtual int getNumberOfCores() = 0;
    
    // 原子操作
    virtual bool atomicCompareExchange(volatile void* ptr, void* expected, void* desired, size_t size) = 0;
    virtual void atomicStore(volatile void* ptr, void* value, size_t size) = 0;
    virtual void atomicLoad(volatile void* ptr, void* value, size_t size) = 0;
    virtual void atomicAdd(volatile void* ptr, int64_t value, size_t size) = 0;
    virtual void atomicSub(volatile void* ptr, int64_t value, size_t size) = 0;
    virtual void atomicAnd(volatile void* ptr, uint64_t value, size_t size) = 0;
    virtual void atomicOr(volatile void* ptr, uint64_t value, size_t size) = 0;
    virtual void atomicXor(volatile void* ptr, uint64_t value, size_t size) = 0;
    
    // 批量原子操作
    template <typename T>
    void atomicBatchAdd(volatile T* ptr, const std::vector<T>& values) {
        for (size_t i = 0; i < values.size(); ++i) {
            atomicAdd(&ptr[i], values[i], sizeof(T));
        }
    }
    
    template <typename T>
    void atomicBatchStore(volatile T* ptr, const std::vector<T>& values) {
        for (size_t i = 0; i < values.size(); ++i) {
            atomicStore(&ptr[i], const_cast<T*>(&values[i]), sizeof(T));
        }
    }
    
    template <typename T>
    void atomicBatchLoad(volatile T* ptr, std::vector<T>& values) {
        for (size_t i = 0; i < values.size(); ++i) {
            atomicLoad(&ptr[i], &values[i], sizeof(T));
        }
    }
    
    // TSN支持增强
    virtual bool hasTSNSupport() {
        return false;
    }
    
    virtual bool enableTSN(const std::string& interface) {
        return false;
    }
    
    virtual bool disableTSN(const std::string& interface) {
        return false;
    }
    
    virtual bool configureTSN(const std::string& interface, uint32_t streamId, uint32_t priority) {
        return false;
    }
    
    virtual bool configureTSNStream(const std::string& interface, uint32_t streamId, 
                                  uint32_t priority, uint64_t bandwidth, 
                                  uint64_t maxLatency, uint32_t timeSlot) {
        return false;
    }
    
    virtual bool getTSNStreamInfo(const std::string& interface, uint32_t streamId, 
                                 uint32_t& priority, uint64_t& bandwidth, 
                                 uint64_t& maxLatency, uint32_t& timeSlot) {
        return false;
    }
    
    virtual bool syncTime(const std::string& interface) {
        return false;
    }
    
    virtual uint64_t getSyncTime() {
        return 0;
    }
    
    virtual bool setTimeSyncInterval(const std::string& interface, uint32_t intervalMs) {
        return false;
    }
};

class LinuxPlatform : public PlatformAbstraction {
public:
    void* allocateMemory(size_t size) override;
    void freeMemory(void* ptr) override;
    void* createSharedMemory(const std::string& name, size_t size) override;
    void destroySharedMemory(const std::string& name) override;
    void* mapSharedMemory(const std::string& name, size_t size) override;
    void unmapSharedMemory(void* ptr, size_t size) override;
    int createSocket(int domain, int type, int protocol) override;
    int closeSocket(int sockfd) override;
    bool setSocketNonBlocking(int sockfd) override;
    bool setSocketReuseAddr(int sockfd) override;
    void* createThread(ThreadFunc func, void* arg) override;
    void joinThread(void* thread) override;
    void* createThreadWithPriority(ThreadFunc func, void* arg, int priority) override;
    void* createMutex() override;
    void destroyMutex(void* mutex) override;
    void lockMutex(void* mutex) override;
    void unlockMutex(void* mutex) override;
    void* createCondition() override;
    void destroyCondition(void* cond) override;
    void waitCondition(void* cond, void* mutex) override;
    void signalCondition(void* cond) override;
    void broadcastCondition(void* cond) override;
    uint64_t getTimestampUs() override;
    
    // 文件系统操作
    bool fileExists(const std::string& path) override;
    bool createDirectory(const std::string& path) override;
    bool removeFile(const std::string& path) override;
    
    // 进程间通信增强
    int createSemaphore(const std::string& name, unsigned int initialCount) override;
    void destroySemaphore(int semid) override;
    void waitSemaphore(int semid) override;
    void signalSemaphore(int semid) override;
    
    // 内存锁定
    bool lockMemory(void* addr, size_t len) override;
    bool unlockMemory(void* addr, size_t len) override;
    
    // 线程优先级设置
    bool setThreadPriority(void* thread, int priority) override;
    int getThreadPriority(void* thread) override;
    
    // CPU 亲和性设置
    bool setThreadAffinity(void* thread, int cpuCore) override;
    
    // 平台信息查询
    std::string getPlatformName() override;
    std::string getOSVersion() override;
    int getNumberOfCores() override;
};

class WindowsPlatform : public PlatformAbstraction {
public:
    void* allocateMemory(size_t size) override;
    void freeMemory(void* ptr) override;
    void* createSharedMemory(const std::string& name, size_t size) override;
    void destroySharedMemory(const std::string& name) override;
    void* mapSharedMemory(const std::string& name, size_t size) override;
    void unmapSharedMemory(void* ptr, size_t size) override;
    int createSocket(int domain, int type, int protocol) override;
    int closeSocket(int sockfd) override;
    bool setSocketNonBlocking(int sockfd) override;
    bool setSocketReuseAddr(int sockfd) override;
    void* createThread(ThreadFunc func, void* arg) override;
    void joinThread(void* thread) override;
    void* createThreadWithPriority(ThreadFunc func, void* arg, int priority) override;
    void* createMutex() override;
    void destroyMutex(void* mutex) override;
    void lockMutex(void* mutex) override;
    void unlockMutex(void* mutex) override;
    void* createCondition() override;
    void destroyCondition(void* cond) override;
    void waitCondition(void* cond, void* mutex) override;
    void signalCondition(void* cond) override;
    void broadcastCondition(void* cond) override;
    uint64_t getTimestampUs() override;
    
    // 文件系统操作
    bool fileExists(const std::string& path) override;
    bool createDirectory(const std::string& path) override;
    bool removeFile(const std::string& path) override;
    
    // 进程间通信增强
    int createSemaphore(const std::string& name, unsigned int initialCount) override;
    void destroySemaphore(int semid) override;
    void waitSemaphore(int semid) override;
    void signalSemaphore(int semid) override;
    
    // 内存锁定
    bool lockMemory(void* addr, size_t len) override;
    bool unlockMemory(void* addr, size_t len) override;
    
    // 线程优先级设置
    bool setThreadPriority(void* thread, int priority) override;
    int getThreadPriority(void* thread) override;
    
    // CPU 亲和性设置
    bool setThreadAffinity(void* thread, int cpuCore) override;
    
    // 平台信息查询
    std::string getPlatformName() override;
    std::string getOSVersion() override;
    int getNumberOfCores() override;
};

class QNXPlatform : public PlatformAbstraction {
public:
    void* allocateMemory(size_t size) override;
    void freeMemory(void* ptr) override;
    void* createSharedMemory(const std::string& name, size_t size) override;
    void destroySharedMemory(const std::string& name) override;
    void* mapSharedMemory(const std::string& name, size_t size) override;
    void unmapSharedMemory(void* ptr, size_t size) override;
    int createSocket(int domain, int type, int protocol) override;
    int closeSocket(int sockfd) override;
    bool setSocketNonBlocking(int sockfd) override;
    bool setSocketReuseAddr(int sockfd) override;
    void* createThread(ThreadFunc func, void* arg) override;
    void joinThread(void* thread) override;
    void* createThreadWithPriority(ThreadFunc func, void* arg, int priority) override;
    void* createMutex() override;
    void destroyMutex(void* mutex) override;
    void lockMutex(void* mutex) override;
    void unlockMutex(void* mutex) override;
    void* createCondition() override;
    void destroyCondition(void* cond) override;
    void waitCondition(void* cond, void* mutex) override;
    void signalCondition(void* cond) override;
    void broadcastCondition(void* cond) override;
    uint64_t getTimestampUs() override;
    
    // 文件系统操作
    bool fileExists(const std::string& path) override;
    bool createDirectory(const std::string& path) override;
    bool removeFile(const std::string& path) override;
    
    // 进程间通信增强
    int createSemaphore(const std::string& name, unsigned int initialCount) override;
    void destroySemaphore(int semid) override;
    void waitSemaphore(int semid) override;
    void signalSemaphore(int semid) override;
    
    // 内存锁定
    bool lockMemory(void* addr, size_t len) override;
    bool unlockMemory(void* addr, size_t len) override;
    
    // 线程优先级设置
    bool setThreadPriority(void* thread, int priority) override;
    int getThreadPriority(void* thread) override;
    
    // CPU 亲和性设置
    bool setThreadAffinity(void* thread, int cpuCore) override;
    
    // 平台信息查询
    std::string getPlatformName() override;
    std::string getOSVersion() override;
    int getNumberOfCores() override;
};

class MacOSPlatform : public PlatformAbstraction {
public:
    void* allocateMemory(size_t size) override;
    void freeMemory(void* ptr) override;
    void* createSharedMemory(const std::string& name, size_t size) override;
    void destroySharedMemory(const std::string& name) override;
    void* mapSharedMemory(const std::string& name, size_t size) override;
    void unmapSharedMemory(void* ptr, size_t size) override;
    int createSocket(int domain, int type, int protocol) override;
    int closeSocket(int sockfd) override;
    bool setSocketNonBlocking(int sockfd) override;
    bool setSocketReuseAddr(int sockfd) override;
    void* createThread(ThreadFunc func, void* arg) override;
    void joinThread(void* thread) override;
    void* createThreadWithPriority(ThreadFunc func, void* arg, int priority) override;
    void* createMutex() override;
    void destroyMutex(void* mutex) override;
    void lockMutex(void* mutex) override;
    void unlockMutex(void* mutex) override;
    void* createCondition() override;
    void destroyCondition(void* cond) override;
    void waitCondition(void* cond, void* mutex) override;
    void signalCondition(void* cond) override;
    void broadcastCondition(void* cond) override;
    uint64_t getTimestampUs() override;
    
    // 文件系统操作
    bool fileExists(const std::string& path) override;
    bool createDirectory(const std::string& path) override;
    bool removeFile(const std::string& path) override;
    
    // 进程间通信增强
    int createSemaphore(const std::string& name, unsigned int initialCount) override;
    void destroySemaphore(int semid) override;
    void waitSemaphore(int semid) override;
    void signalSemaphore(int semid) override;
    
    // 内存锁定
    bool lockMemory(void* addr, size_t len) override;
    bool unlockMemory(void* addr, size_t len) override;
    
    // 线程优先级设置
    bool setThreadPriority(void* thread, int priority) override;
    int getThreadPriority(void* thread) override;
    
    // CPU 亲和性设置
    bool setThreadAffinity(void* thread, int cpuCore) override;
    
    // 平台信息查询
    std::string getPlatformName() override;
    std::string getOSVersion() override;
    int getNumberOfCores() override;
};

class VxWorksPlatform : public PlatformAbstraction {
public:
    void* allocateMemory(size_t size) override;
    void freeMemory(void* ptr) override;
    void* createSharedMemory(const std::string& name, size_t size) override;
    void destroySharedMemory(const std::string& name) override;
    void* mapSharedMemory(const std::string& name, size_t size) override;
    void unmapSharedMemory(void* ptr, size_t size) override;
    int createSocket(int domain, int type, int protocol) override;
    int closeSocket(int sockfd) override;
    bool setSocketNonBlocking(int sockfd) override;
    bool setSocketReuseAddr(int sockfd) override;
    void* createThread(ThreadFunc func, void* arg) override;
    void joinThread(void* thread) override;
    void* createThreadWithPriority(ThreadFunc func, void* arg, int priority) override;
    void* createMutex() override;
    void destroyMutex(void* mutex) override;
    void lockMutex(void* mutex) override;
    void unlockMutex(void* mutex) override;
    void* createCondition() override;
    void destroyCondition(void* cond) override;
    void waitCondition(void* cond, void* mutex) override;
    void signalCondition(void* cond) override;
    void broadcastCondition(void* cond) override;
    uint64_t getTimestampUs() override;
    
    // 文件系统操作
    bool fileExists(const std::string& path) override;
    bool createDirectory(const std::string& path) override;
    bool removeFile(const std::string& path) override;
    
    // 进程间通信增强
    int createSemaphore(const std::string& name, unsigned int initialCount) override;
    void destroySemaphore(int semid) override;
    void waitSemaphore(int semid) override;
    void signalSemaphore(int semid) override;
    
    // 内存锁定
    bool lockMemory(void* addr, size_t len) override;
    bool unlockMemory(void* addr, size_t len) override;
    
    // 线程优先级设置
    bool setThreadPriority(void* thread, int priority) override;
    int getThreadPriority(void* thread) override;
    
    // CPU 亲和性设置
    bool setThreadAffinity(void* thread, int cpuCore) override;
    
    // 平台信息查询
    std::string getPlatformName() override;
    std::string getOSVersion() override;
    int getNumberOfCores() override;
};

class PlatformManager {
public:
    static PlatformManager& instance();
    void init();
    PlatformAbstraction* getPlatform();
    
private:
    PlatformManager() = default;
    std::unique_ptr<PlatformAbstraction> platform_;
};

} // namespace platform
} // namespace aurorart

#endif // AURORART_PLATFORM_ABSTRACTION_H