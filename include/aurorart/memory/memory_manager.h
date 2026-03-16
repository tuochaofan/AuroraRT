#ifndef AURORART_MEMORY_MANAGER_H
#define AURORART_MEMORY_MANAGER_H

#include <atomic>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>
#include <chrono>

namespace aurorart {
namespace memory {

// 内存块头部结构
typedef struct BlockHeader {
    size_t size;           // 块大小
    bool is_free;          // 是否空闲
    BlockHeader* next;     // 指向下一个块
    BlockHeader* prev;     // 指向上一个块
    char padding[4];       // 对齐到16字节
} BlockHeader;

// 共享内存段结构
struct ShmSegment {
    void* ptr;             // 共享内存指针
    size_t size;           // 共享内存大小
    size_t actualSize;     // 实际分配的大小（可能大于请求大小）
    std::atomic<int> refCount; // 引用计数
    bool isDynamic;        // 是否为动态大小
    bool isPooled;         // 是否来自内存池
    std::chrono::steady_clock::time_point lastAccessTime; // 最后访问时间
    std::atomic<uint64_t> accessCount; // 访问次数
};

// 共享内存池配置
struct ShmPoolConfig {
    size_t blockSize;      // 块大小
    size_t blockCount;     // 块数量
    size_t alignment;      // 对齐要求
};

// 共享内存池结构
struct ShmPool {
    std::string name;      // 池名称
    void* ptr;             // 共享内存指针
    size_t size;           // 池大小
    size_t blockSize;      // 块大小
    size_t blockCount;     // 块数量
    size_t freeBlocks;     // 空闲块数量
    std::atomic<size_t> usedBlocks; // 已使用块数量
    std::vector<bool> blockStatus; // 块状态（true表示已使用）
    std::mutex mutex;      // 互斥锁
    std::chrono::steady_clock::time_point creationTime; // 创建时间
    std::atomic<uint64_t> allocationCount; // 分配次数
    std::atomic<uint64_t> deallocationCount; // 释放次数
};

class MemoryPool {
public:
    MemoryPool(size_t blockSize, size_t blockCount);
    ~MemoryPool();
    
    void* allocate();
    void deallocate(void* ptr);
    
    size_t getBlockSize() const;
    size_t getBlockCount() const;
    size_t getFreeBlocks() const;
    size_t getUsedBlocks() const;
    uint64_t getAllocationCount() const;
    uint64_t getDeallocationCount() const;
    double getUsage() const;
    
    // 检查指针是否在内存池中
    bool contains(void* ptr) const;
    
    // 监控相关方法
    void printStats() const;
    
private:
    size_t blockSize_;
    size_t blockCount_;
    size_t actualBlockSize_; // 实际块大小（包括头部和对齐）
    std::atomic<size_t> freeBlocks_;
    std::atomic<size_t> usedBlocks_;
    void* pool_;
    BlockHeader* firstBlock_;
    BlockHeader* lastBlock_;
    std::atomic<void*> freeList_;
    
    // 统计信息
    std::atomic<uint64_t> allocationCount_;
    std::atomic<uint64_t> deallocationCount_;
    std::atomic<uint64_t> allocationFailures_;
    std::atomic<uint64_t> peakUsedBlocks_;
    std::chrono::steady_clock::time_point lastAdjustTime_;
    std::chrono::steady_clock::time_point creationTime_;
    double memoryUtilization_; // 当前内存利用率
    double lastUtilization_;   // 上次内存利用率
    
    // 合并相邻的空闲块
    void mergeAdjacentBlocks(BlockHeader* block);
    // 从自由链表中移除块
    void removeFromFreeList(BlockHeader* block);
    // 检查并调整内存池
    void checkAndAdjust();
};

class SharedMemoryManager {
public:
    static SharedMemoryManager& instance();
    
    void* allocate(size_t size);
    void* allocateDynamic(size_t size);
    void deallocate(void* ptr);
    void* map(const std::string& name, size_t size);
    void unmap(const std::string& name);
    
    size_t getTotalSharedMemorySize() const;
    
private:
    SharedMemoryManager();
    
    // 共享内存池管理
    void initShmPools();
    void createShmPool(const ShmPoolConfig& config);
    void* allocateFromPool(size_t size);
    bool expandShmPool(ShmPool* pool);
    void freeToPool(void* ptr);
    
    std::unordered_map<std::string, ShmSegment> segments_;
    std::unordered_map<size_t, std::unique_ptr<ShmPool>> shmPools_; // 按块大小索引的共享内存池
    mutable std::mutex mutex_;
};

class MemoryManager {
public:
    static MemoryManager& instance();
    
    void init();
    void shutdown();
    
    void* allocate(size_t size);
    void deallocate(void* ptr);
    void* allocateAligned(size_t size, size_t alignment);
    void deallocateAligned(void* ptr);
    void* allocateShared(size_t size);
    void* allocateDynamicShared(size_t size);
    void deallocateShared(void* ptr);
    
    void printMemoryStats() const;
    
private:
    MemoryManager() = default;
    std::vector<std::unique_ptr<MemoryPool>> pools_;
    std::mutex mutex_;
    size_t allocatedBytes_ = 0;
};

} // namespace memory
} // namespace aurorart

#endif // AURORART_MEMORY_MANAGER_H