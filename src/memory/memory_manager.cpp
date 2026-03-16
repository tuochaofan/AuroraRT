#include "aurorart/memory/memory_manager.h"
#include "aurorart/platform/platform_abstraction.h"
#include "aurorart/utils/performance.h"
#include "aurorart/utils/logger.h"
#include <algorithm>
#include <iostream>

namespace aurorart {
namespace memory {

// 内存块头部结构
typedef struct {
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

// 内存池配置结构
struct MemoryPoolConfig {
    size_t block_size;     // 块大小
    size_t block_count;    // 块数量
    size_t alignment;      // 对齐要求
};

MemoryPool::MemoryPool(size_t blockSize, size_t blockCount)
    : blockSize_(blockSize), blockCount_(blockCount), freeBlocks_(blockCount), usedBlocks_(0),
      pool_(nullptr), firstBlock_(nullptr), lastBlock_(nullptr),
      allocationCount_(0), deallocationCount_(0),
      allocationFailures_(0), peakUsedBlocks_(0),
      lastAdjustTime_(std::chrono::steady_clock::now()),
      creationTime_(std::chrono::steady_clock::now()),
      memoryUtilization_(0.0), lastUtilization_(0.0) {
    // 计算实际块大小（加上头部）
    size_t actualBlockSize = blockSize + sizeof(BlockHeader);
    // 确保对齐到64字节，提高缓存命中率
    actualBlockSize = (actualBlockSize + 63) & ~63;
    actualBlockSize_ = actualBlockSize;
    
    // 分配内存池
    size_t totalSize = actualBlockSize * blockCount;
    pool_ = malloc(totalSize);
    if (!pool_) {
        AURORA_LOG_ERROR("Failed to allocate memory pool: {} bytes", totalSize);
        return;
    }
    
    // 锁定内存以防止分页（实时系统关键功能）
    #ifdef __linux__
    mlock(pool_, totalSize);
    #endif
    
    // 初始化自由链表
    freeList_.store(nullptr);
    char* current = static_cast<char*>(pool_);
    
    for (size_t i = 0; i < blockCount; ++i) {
        // 初始化块头部
        BlockHeader* header = reinterpret_cast<BlockHeader*>(current);
        header->size = blockSize;
        header->is_free = true;
        header->next = freeList_.load();
        header->prev = nullptr;
        
        // 更新自由链表
        freeList_.store(header);
        
        // 维护块链接
        if (i == 0) {
            firstBlock_ = header;
        } else {
            BlockHeader* prevHeader = reinterpret_cast<BlockHeader*>(current - actualBlockSize);
            prevHeader->next = header;
            header->prev = prevHeader;
        }
        
        if (i == blockCount - 1) {
            lastBlock_ = header;
        }
        
        current += actualBlockSize;
    }
    
    AURORA_LOG_INFO("Created memory pool: block size={}, block count={}, total size={} bytes, actual block size={}, alignment=64", 
                   blockSize, blockCount, totalSize, actualBlockSize);
}

MemoryPool::~MemoryPool() {
    if (pool_) {
        free(pool_);
        AURORA_LOG_INFO("Destroyed memory pool: block size={}, block count={}", 
                       blockSize_, blockCount_);
    }
}

void* MemoryPool::allocate() {
    void* current = freeList_.load();
    while (current && !freeList_.compare_exchange_weak(current, reinterpret_cast<BlockHeader*>(current)->next)) {
        // 原子操作失败，重试
    }
    
    if (current) {
        BlockHeader* header = reinterpret_cast<BlockHeader*>(current);
        header->is_free = false;
        freeBlocks_--;
        size_t used = usedBlocks_++;
        allocationCount_++;
        
        // 更新峰值使用块
        size_t peak = peakUsedBlocks_.load();
        while (used > peak && !peakUsedBlocks_.compare_exchange_weak(peak, used)) {
            // 原子操作失败，重试
        }
        
        // 检查是否需要动态调整
        checkAndAdjust();
        
        // 返回数据区域
        return reinterpret_cast<char*>(current) + sizeof(BlockHeader);
    } else {
        allocationFailures_++;
        return nullptr;
    }
}

void MemoryPool::deallocate(void* ptr) {
    if (!ptr) return;
    
    // 获取块头部
    BlockHeader* header = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(ptr) - sizeof(BlockHeader));
    header->is_free = true;
    
    // 尝试合并相邻的空闲块
    mergeAdjacentBlocks(header);
    
    // 将块放回自由链表
    void* current = freeList_.load();
    while (!freeList_.compare_exchange_weak(current, header)) {
        // 原子操作失败，重试
    }
    
    header->next = reinterpret_cast<BlockHeader*>(current);
    if (header->next) {
        header->next->prev = header;
    }
    header->prev = nullptr;
    
    freeBlocks_++;
    usedBlocks_--;
    deallocationCount_++;
    
    // 检查是否需要动态调整
    checkAndAdjust();
}

void MemoryPool::mergeAdjacentBlocks(BlockHeader* header) {
    // 合并前一个空闲块
    if (header->prev && header->prev->is_free) {
        BlockHeader* prevBlock = header->prev;
        
        // 从自由链表中移除前一个块
        removeFromFreeList(prevBlock);
        
        // 合并块
        prevBlock->size += sizeof(BlockHeader) + header->size;
        prevBlock->next = header->next;
        if (header->next) {
            header->next->prev = prevBlock;
        }
        
        // 更新最后块指针
        if (header == lastBlock_) {
            lastBlock_ = prevBlock;
        }
        
        header = prevBlock;
        AURORA_LOG_DEBUG("Merged with previous block, new size: {}", header->size);
    }
    
    // 合并后一个空闲块
    if (header->next && header->next->is_free) {
        BlockHeader* nextBlock = header->next;
        
        // 从自由链表中移除后一个块
        removeFromFreeList(nextBlock);
        
        // 合并块
        header->size += sizeof(BlockHeader) + nextBlock->size;
        header->next = nextBlock->next;
        if (nextBlock->next) {
            nextBlock->next->prev = header;
        }
        
        // 更新最后块指针
        if (nextBlock == lastBlock_) {
            lastBlock_ = header;
        }
        
        AURORA_LOG_DEBUG("Merged with next block, new size: {}", header->size);
    }
}

void MemoryPool::removeFromFreeList(BlockHeader* header) {
    // 从自由链表中移除块
    void* current = freeList_.load();
    while (current) {
        BlockHeader* currentBlock = reinterpret_cast<BlockHeader*>(current);
        if (currentBlock == header) {
            // 找到要移除的块
            if (freeList_.compare_exchange_weak(current, header->next)) {
                if (header->next) {
                    header->next->prev = nullptr;
                }
                break;
            }
        } else {
            current = currentBlock->next;
        }
    }
}

void MemoryPool::checkAndAdjust() {
    // 检查是否需要调整，每100次操作或每1秒检查一次
    static constexpr uint64_t CHECK_INTERVAL = 100;
    static constexpr std::chrono::seconds TIME_INTERVAL = std::chrono::seconds(1);
    
    uint64_t allocCount = allocationCount_.load();
    uint64_t deallocCount = deallocationCount_.load();
    
    if (allocCount % CHECK_INTERVAL != 0) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    if (now - lastAdjustTime_ < TIME_INTERVAL) {
        return;
    }
    
    lastAdjustTime_ = now;
    
    size_t used = usedBlocks_.load();
    size_t total = blockCount_;
    double usage = static_cast<double>(used) / total;
    
    // 更新内存利用率
    lastUtilization_ = memoryUtilization_;
    memoryUtilization_ = usage;
    
    // 计算利用率变化率
    double utilizationChange = memoryUtilization_ - lastUtilization_;
    
    AURORA_LOG_DEBUG("MemoryPool check: blockSize={}, usage={:.2f}%, allocations={}, deallocations={}, change={:.2f}%", 
                   blockSize_, usage * 100, allocCount, deallocCount, utilizationChange * 100);
    
    // 动态调整内存池大小
    // 1. 如果使用率超过80%，考虑增加块数量
    if (usage > 0.8) {
        // 根据利用率变化率动态调整扩展比例
        size_t expansionFactor = utilizationChange > 0.1 ? 2 : 1.5; // 快速增长时更大幅度扩展
        size_t newBlockCount = total * expansionFactor;
        AURORA_LOG_INFO("MemoryPool: Usage high ({:.2f}%), increasing blocks from {} to {}", 
                       usage * 100, total, newBlockCount);
        // 实现内存池扩展逻辑
        expand(newBlockCount);
    }
    // 2. 如果使用率低于20%，考虑减少块数量
    else if (usage < 0.2 && total > 16) {
        size_t newBlockCount = total * 0.6; // 减少到60%
        if (newBlockCount < 16) newBlockCount = 16; // 保持最小块数量
        AURORA_LOG_INFO("MemoryPool: Usage low ({:.2f}%), reducing blocks from {} to {}", 
                       usage * 100, total, newBlockCount);
        // 实现内存池收缩逻辑
        shrink(newBlockCount);
    }
    // 3. 如果使用率在60-80%之间，且持续增长，预扩展
    else if (usage > 0.6 && usage < 0.8 && utilizationChange > 0.05) {
        size_t newBlockCount = total * 1.2; // 预扩展20%
        AURORA_LOG_INFO("MemoryPool: Usage growing ({:.2f}% → {:.2f}%), pre-emptive expansion from {} to {}", 
                       lastUtilization_ * 100, usage * 100, total, newBlockCount);
        expand(newBlockCount);
    }
    
    // 4. 检查分配失败率
    if (allocationFailures_.load() > 0) {
        double failureRate = static_cast<double>(allocationFailures_.load()) / allocCount;
        if (failureRate > 0.05) { // 失败率超过5%
            AURORA_LOG_WARN("MemoryPool: High allocation failure rate ({:.2f}%), consider increasing pool size", 
                          failureRate * 100);
            // 立即扩展内存池
            size_t newBlockCount = total * 2; // 增加100%
            AURORA_LOG_INFO("MemoryPool: Expanding due to high failure rate, increasing blocks from {} to {}", 
                           total, newBlockCount);
            expand(newBlockCount);
        }
    }
    
    // 5. 检查内存池碎片率
    size_t free = freeBlocks_.load();
    if (free > total * 0.3) { // 30%以上空闲但可能存在碎片
        AURORA_LOG_DEBUG("MemoryPool: Checking for fragmentation - free blocks: {}/{}", free, total);
        // 这里可以添加碎片整理逻辑
    }
}

bool MemoryPool::contains(void* ptr) const {
    if (!pool_ || !ptr) return false;
    
    char* poolStart = static_cast<char*>(pool_);
    char* poolEnd = poolStart + blockCount_ * ((blockSize_ + sizeof(BlockHeader) + 15) & ~15);
    char* ptrAddr = static_cast<char*>(ptr);
    
    return ptrAddr >= poolStart && ptrAddr < poolEnd;
}

size_t MemoryPool::getBlockSize() const { return blockSize_; }
size_t MemoryPool::getBlockCount() const { return blockCount_; }
size_t MemoryPool::getFreeBlocks() const { return freeBlocks_; }
size_t MemoryPool::getUsedBlocks() const { return usedBlocks_; }
uint64_t MemoryPool::getAllocationCount() const { return allocationCount_; }
uint64_t MemoryPool::getDeallocationCount() const { return deallocationCount_; }
double MemoryPool::getUsage() const {
    return static_cast<double>(usedBlocks_) / blockCount_ * 100.0;
}

void MemoryPool::expand(size_t newBlockCount) {
    if (newBlockCount <= blockCount_) {
        AURORA_LOG_WARN("MemoryPool::expand: New block count ({}) is not larger than current ({})", newBlockCount, blockCount_);
        return;
    }
    
    // 使用预计算的实际块大小
    size_t actualBlockSize = actualBlockSize_;
    
    // 计算需要添加的块数量
    size_t additionalBlocks = newBlockCount - blockCount_;
    size_t additionalSize = actualBlockSize * additionalBlocks;
    size_t newTotalSize = blockCount_ * actualBlockSize + additionalSize;
    
    // 分配新的内存
    void* newPool = realloc(pool_, newTotalSize);
    if (!newPool) {
        AURORA_LOG_ERROR("MemoryPool::expand: Failed to allocate additional memory: {} bytes", additionalSize);
        return;
    }
    
    // 解锁旧内存
    #ifdef __linux__
    munlock(pool_, blockCount_ * actualBlockSize);
    #endif
    
    pool_ = newPool;
    
    // 锁定新内存
    #ifdef __linux__
    mlock(pool_, newTotalSize);
    #endif
    
    // 初始化新添加的块
    char* current = static_cast<char*>(pool_) + blockCount_ * actualBlockSize;
    for (size_t i = 0; i < additionalBlocks; ++i) {
        // 初始化块头部
        BlockHeader* header = reinterpret_cast<BlockHeader*>(current);
        header->size = blockSize_;
        header->is_free = true;
        header->next = freeList_.load();
        header->prev = nullptr;
        
        // 更新自由链表
        freeList_.store(header);
        
        // 维护块链接
        BlockHeader* prevHeader = reinterpret_cast<BlockHeader*>(current - actualBlockSize);
        prevHeader->next = header;
        header->prev = prevHeader;
        
        if (i == additionalBlocks - 1) {
            lastBlock_ = header;
        }
        
        current += actualBlockSize;
    }
    
    // 更新块数量和自由块数量
    blockCount_ = newBlockCount;
    freeBlocks_ += additionalBlocks;
    
    AURORA_LOG_INFO("MemoryPool expanded: block count={}, free blocks={}, total size={} bytes", 
                   blockCount_, freeBlocks_, newTotalSize);
}

void MemoryPool::shrink(size_t newBlockCount) {
    if (newBlockCount >= blockCount_ || newBlockCount < 16) {
        AURORA_LOG_WARN("MemoryPool::shrink: New block count ({}) is not valid", newBlockCount);
        return;
    }
    
    // 检查是否有足够的自由块可以释放
    size_t used = usedBlocks_.load();
    if (newBlockCount < used) {
        AURORA_LOG_WARN("MemoryPool::shrink: New block count ({}) is less than used blocks ({})", newBlockCount, used);
        return;
    }
    
    // 使用预计算的实际块大小
    size_t actualBlockSize = actualBlockSize_;
    
    // 计算需要保留的内存大小
    size_t newSize = newBlockCount * actualBlockSize;
    
    // 解锁旧内存
    #ifdef __linux__
    munlock(pool_, blockCount_ * actualBlockSize);
    #endif
    
    // 重新分配内存
    void* newPool = realloc(pool_, newSize);
    if (!newPool) {
        AURORA_LOG_ERROR("MemoryPool::shrink: Failed to reallocate memory: {} bytes", newSize);
        // 重新锁定旧内存
        #ifdef __linux__
        mlock(pool_, blockCount_ * actualBlockSize);
        #endif
        return;
    }
    
    pool_ = newPool;
    
    // 锁定新内存
    #ifdef __linux__
    mlock(pool_, newSize);
    #endif
    
    // 更新块数量和自由块数量
    size_t removedBlocks = blockCount_ - newBlockCount;
    blockCount_ = newBlockCount;
    freeBlocks_ -= removedBlocks;
    
    // 更新最后块指针
    lastBlock_ = reinterpret_cast<BlockHeader*>(static_cast<char*>(pool_) + (newBlockCount - 1) * actualBlockSize);
    lastBlock_->next = nullptr;
    
    // 清理自由链表中可能指向已释放内存的指针
    // 更安全的清理方法
    void* current = freeList_.load();
    void* new_free_list = nullptr;
    
    while (current) {
        BlockHeader* header = reinterpret_cast<BlockHeader*>(current);
        void* block_data = reinterpret_cast<char*>(header) + sizeof(BlockHeader);
        
        if (contains(block_data)) {
            // 保留在自由链表中
            header->next = reinterpret_cast<BlockHeader*>(new_free_list);
            new_free_list = header;
        }
        
        current = header->next;
    }
    
    // 原子更新自由链表
    freeList_.store(new_free_list);
    
    AURORA_LOG_INFO("MemoryPool shrunk: block count={}, free blocks={}, total size={} bytes", 
                   blockCount_, freeBlocks_, newSize);
}

void MemoryPool::printStats() const {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - creationTime_).count();
    
    AURORA_LOG_INFO("MemoryPool stats:");
    AURORA_LOG_INFO("  Block size: {} bytes", blockSize_);
    AURORA_LOG_INFO("  Total blocks: {}", blockCount_);
    AURORA_LOG_INFO("  Used blocks: {}", usedBlocks_);
    AURORA_LOG_INFO("  Free blocks: {}", freeBlocks_);
    AURORA_LOG_INFO("  Usage: {:.2f}%", getUsage());
    AURORA_LOG_INFO("  Allocations: {}", allocationCount_);
    AURORA_LOG_INFO("  Deallocations: {}", deallocationCount_);
    AURORA_LOG_INFO("  Allocation failures: {}", allocationFailures_);
    AURORA_LOG_INFO("  Peak used blocks: {}", peakUsedBlocks_);
    AURORA_LOG_INFO("  Uptime: {} seconds", uptime);
}

SharedMemoryManager& SharedMemoryManager::instance() {
    static SharedMemoryManager instance;
    return instance;
}

SharedMemoryManager::SharedMemoryManager() {
    // 初始化共享内存池
    initShmPools();
}

void SharedMemoryManager::initShmPools() {
    // 预创建不同大小的共享内存池
    std::vector<ShmPoolConfig> poolConfigs = {
        {64, 1024, 64},      // 64字节块，1024个
        {256, 512, 64},      // 256字节块，512个
        {1024, 256, 64},     // 1024字节块，256个
        {4096, 128, 64},     // 4096字节块，128个
        {16384, 64, 64},      // 16384字节块，64个
        {65536, 32, 64}       // 65536字节块，32个
    };
    
    for (size_t i = 0; i < poolConfigs.size(); ++i) {
        const auto& config = poolConfigs[i];
        createShmPool(config);
    }
    
    AURORA_LOG_INFO("Initialized {} shared memory pools", shmPools_.size());
}

void SharedMemoryManager::createShmPool(const ShmPoolConfig& config) {
    std::string poolName = "aurorart_shm_pool_" + std::to_string(config.blockSize);
    size_t poolSize = config.blockSize * config.blockCount;
    
    void* ptr = platform::PlatformManager::instance().getPlatform()->createSharedMemory(poolName, poolSize);
    if (ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto pool = std::make_unique<ShmPool>();
        pool->name = poolName;
        pool->ptr = ptr;
        pool->size = poolSize;
        pool->blockSize = config.blockSize;
        pool->blockCount = config.blockCount;
        pool->freeBlocks = config.blockCount;
        pool->usedBlocks = 0;
        pool->blockStatus.resize(config.blockCount, false);
        pool->creationTime = std::chrono::steady_clock::now();
        pool->allocationCount = 0;
        pool->deallocationCount = 0;
        
        shmPools_[config.blockSize] = std::move(pool);
        AURORA_LOG_INFO("Created shared memory pool: name={}, blockSize={}, blockCount={}, totalSize={} bytes", 
                       poolName, config.blockSize, config.blockCount, poolSize);
    } else {
        AURORA_LOG_ERROR("Failed to create shared memory pool: blockSize={}, blockCount={}", 
                       config.blockSize, config.blockCount);
    }
}

void* SharedMemoryManager::allocateFromPool(size_t size) {
    // 找到最合适的共享内存池
    size_t bestBlockSize = std::numeric_limits<size_t>::max();
    ShmPool* bestPool = nullptr;
    
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [blockSize, pool] : shmPools_) {
            if (blockSize >= size && blockSize < bestBlockSize) {
                bestPool = pool.get();
                bestBlockSize = blockSize;
            }
        }
    }
    
    if (bestPool) {
        std::lock_guard<std::mutex> lock(bestPool->mutex);
        
        // 找到空闲块
        for (size_t i = 0; i < bestPool->blockCount; ++i) {
            if (!bestPool->blockStatus[i]) {
                // 标记块为已使用
                bestPool->blockStatus[i] = true;
                bestPool->freeBlocks--;
                bestPool->usedBlocks++;
                bestPool->allocationCount++;
                
                // 计算块的地址
                void* ptr = static_cast<char*>(bestPool->ptr) + i * bestPool->blockSize;
                
                // 记录到段列表
                std::string segmentName = bestPool->name + "_block_" + std::to_string(i);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    ShmSegment segment;
                    segment.ptr = ptr;
                    segment.size = size;
                    segment.actualSize = bestPool->blockSize;
                    segment.refCount = 1;
                    segment.isDynamic = false;
                    segment.isPooled = true;
                    segment.lastAccessTime = std::chrono::steady_clock::now();
                    segment.accessCount = 1;
                    segments_[segmentName] = segment;
                }
                
                AURORA_LOG_DEBUG("Allocated {} bytes from shared memory pool: blockSize={}, blockIndex={}", 
                               size, bestPool->blockSize, i);
                return ptr;
            }
        }
        
        // 该池没有空闲块，尝试扩展
        AURORA_LOG_WARN("Shared memory pool exhausted: blockSize={}, expanding...", bestPool->blockSize);
        if (expandShmPool(bestPool)) {
            // 扩展成功后再次尝试分配
            return allocateFromPool(size);
        }
    }
    
    return nullptr;
}

bool SharedMemoryManager::expandShmPool(ShmPool* pool) {
    if (!pool) return false;
    
    size_t newBlockCount = pool->blockCount * 2;
    size_t newSize = pool->blockSize * newBlockCount;
    std::string newPoolName = pool->name + "_expanded";
    
    void* newPtr = platform::PlatformManager::instance().getPlatform()->createSharedMemory(newPoolName, newSize);
    if (!newPtr) {
        AURORA_LOG_ERROR("Failed to expand shared memory pool: {} bytes", newSize);
        return false;
    }
    
    // 复制现有数据
    memcpy(newPtr, pool->ptr, pool->size);
    
    // 更新池信息
    platform::PlatformManager::instance().getPlatform()->destroySharedMemory(pool->name);
    pool->ptr = newPtr;
    pool->size = newSize;
    pool->blockCount = newBlockCount;
    pool->freeBlocks += pool->blockCount / 2;
    pool->blockStatus.resize(newBlockCount, false);
    
    AURORA_LOG_INFO("Expanded shared memory pool: name={}, newBlockCount={}, newSize={} bytes", 
                   pool->name, newBlockCount, newSize);
    return true;
}

void SharedMemoryManager::freeToPool(void* ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto it = segments_.begin(); it != segments_.end(); ++it) {
        if (it->second.ptr == ptr && it->second.isPooled) {
            // 找到对应的共享内存池
            size_t blockSize = it->second.actualSize;
            auto poolIt = shmPools_.find(blockSize);
            if (poolIt != shmPools_.end()) {
                ShmPool* pool = poolIt->second.get();
                std::lock_guard<std::mutex> poolLock(pool->mutex);
                
                // 计算块索引
                size_t blockIndex = (static_cast<char*>(ptr) - static_cast<char*>(pool->ptr)) / pool->blockSize;
                if (blockIndex < pool->blockCount) {
                    // 标记块为空闲
                    pool->blockStatus[blockIndex] = false;
                    pool->freeBlocks++;
                    pool->usedBlocks--;
                    pool->deallocationCount++;
                    
                    AURORA_LOG_DEBUG("Freed shared memory pool block: blockSize={}, blockIndex={}", 
                                   pool->blockSize, blockIndex);
                }
            }
            
            segments_.erase(it);
            return;
        }
    }
}

void* SharedMemoryManager::allocate(size_t size) {
    if (size == 0) {
        AURORA_LOG_ERROR("SharedMemoryManager::allocate: Invalid size 0");
        return nullptr;
    }
    
    // 尝试从共享内存池分配
    void* ptr = allocateFromPool(size);
    if (ptr) {
        return ptr;
    }
    
    // 没有合适的共享内存池，创建新的共享内存
    static std::atomic<int> counter(0);
    int id = counter.fetch_add(1);
    std::string name = "aurorart_shm_" + std::to_string(id);
    
    // 创建共享内存
    ptr = platform::PlatformManager::instance().getPlatform()->createSharedMemory(name, size);
    if (ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        ShmSegment segment;
        segment.ptr = ptr;
        segment.size = size;
        segment.actualSize = size;
        segment.refCount = 1;
        segment.isDynamic = false;
        segment.isPooled = false;
        segment.lastAccessTime = std::chrono::steady_clock::now();
        segment.accessCount = 1;
        segments_[name] = segment;
        AURORA_LOG_INFO("Allocated shared memory: name={}, size={} bytes, ptr={}", name, size, ptr);
    } else {
        AURORA_LOG_ERROR("Failed to allocate shared memory: {} bytes", size);
    }
    return ptr;
}

void* SharedMemoryManager::allocateDynamic(size_t size) {
    if (size == 0) {
        AURORA_LOG_ERROR("SharedMemoryManager::allocateDynamic: Invalid size 0");
        return nullptr;
    }
    
    // 生成唯一的共享内存名称
    static std::atomic<int> counter(0);
    int id = counter.fetch_add(1);
    std::string name = "aurorart_shm_dynamic_" + std::to_string(id);
    
    // 创建共享内存
    void* ptr = platform::PlatformManager::instance().getPlatform()->createSharedMemory(name, size);
    if (ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        ShmSegment segment;
        segment.ptr = ptr;
        segment.size = size;
        segment.actualSize = size;
        segment.refCount = 1;
        segment.isDynamic = true;
        segment.isPooled = false;
        segment.lastAccessTime = std::chrono::steady_clock::now();
        segment.accessCount = 1;
        segments_[name] = segment;
        AURORA_LOG_INFO("Allocated dynamic shared memory: name={}, size={} bytes, ptr={}", name, size, ptr);
    } else {
        AURORA_LOG_ERROR("Failed to allocate dynamic shared memory: {} bytes", size);
    }
    return ptr;
}

void SharedMemoryManager::deallocate(void* ptr) {
    if (!ptr) {
        AURORA_LOG_ERROR("SharedMemoryManager::deallocate: Invalid null pointer");
        return;
    }
    
    // 检查是否是从共享内存池分配的
    freeToPool(ptr);
    
    // 处理非池化的共享内存
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = segments_.begin(); it != segments_.end(); ++it) {
        if (it->second.ptr == ptr) {
            int newRefCount = --it->second.refCount;
            AURORA_LOG_DEBUG("SharedMemoryManager::deallocate: name={}, refCount={}→{}", 
                           it->first, newRefCount + 1, newRefCount);
            
            if (newRefCount == 0) {
                platform::PlatformManager::instance().getPlatform()->destroySharedMemory(it->first);
                AURORA_LOG_INFO("Deallocated shared memory: name={}, size={} bytes, ptr={}", 
                               it->first, it->second.size, ptr);
                segments_.erase(it);
            }
            return;
        }
    }
    
    AURORA_LOG_WARN("SharedMemoryManager::deallocate: Pointer not found: {}", ptr);
}

void* SharedMemoryManager::map(const std::string& name, size_t size) {
    if (name.empty()) {
        AURORA_LOG_ERROR("SharedMemoryManager::map: Empty name");
        return nullptr;
    }
    
    if (size == 0) {
        AURORA_LOG_ERROR("SharedMemoryManager::map: Invalid size 0");
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = segments_.find(name);
    if (it != segments_.end()) {
        int newRefCount = ++it->second.refCount;
        AURORA_LOG_DEBUG("SharedMemoryManager::map: name={}, refCount={}→{}", 
                       name, newRefCount - 1, newRefCount);
        return it->second.ptr;
    }
    
    void* ptr = platform::PlatformManager::instance().getPlatform()->mapSharedMemory(name, size);
    if (ptr) {
        ShmSegment segment;
        segment.ptr = ptr;
        segment.size = size;
        segment.refCount = 1;
        segment.isDynamic = false;
        segments_[name] = segment;
        AURORA_LOG_INFO("Mapped shared memory: name={}, size={} bytes, ptr={}", name, size, ptr);
    } else {
        AURORA_LOG_ERROR("Failed to map shared memory: name={}, size={} bytes", name, size);
    }
    return ptr;
}

void SharedMemoryManager::unmap(const std::string& name) {
    if (name.empty()) {
        AURORA_LOG_ERROR("SharedMemoryManager::unmap: Empty name");
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = segments_.find(name);
    if (it != segments_.end()) {
        int newRefCount = --it->second.refCount;
        AURORA_LOG_DEBUG("SharedMemoryManager::unmap: name={}, refCount={}→{}", 
                       name, newRefCount + 1, newRefCount);
        
        if (newRefCount == 0) {
            platform::PlatformManager::instance().getPlatform()->unmapSharedMemory(it->second.ptr, it->second.size);
            AURORA_LOG_INFO("Unmapped shared memory: name={}, size={} bytes, ptr={}", 
                           name, it->second.size, it->second.ptr);
            segments_.erase(it);
        }
    } else {
        AURORA_LOG_WARN("SharedMemoryManager::unmap: Name not found: {}", name);
    }
}

size_t SharedMemoryManager::getTotalSharedMemorySize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = 0;
    for (const auto& entry : segments_) {
        total += entry.second.size;
    }
    return total;
}

MemoryManager& MemoryManager::instance() {
    static MemoryManager instance;
    return instance;
}

void MemoryManager::init() {
    // 预创建不同大小的内存池，使用更合理的块大小分布
    pools_.emplace_back(std::make_unique<MemoryPool>(32, 2048));    // 32字节块
    pools_.emplace_back(std::make_unique<MemoryPool>(64, 1024));    // 64字节块
    pools_.emplace_back(std::make_unique<MemoryPool>(128, 512));    // 128字节块
    pools_.emplace_back(std::make_unique<MemoryPool>(256, 256));    // 256字节块
    pools_.emplace_back(std::make_unique<MemoryPool>(512, 128));    // 512字节块
    pools_.emplace_back(std::make_unique<MemoryPool>(1024, 64));    // 1024字节块
    pools_.emplace_back(std::make_unique<MemoryPool>(2048, 32));    // 2048字节块
    pools_.emplace_back(std::make_unique<MemoryPool>(4096, 16));    // 4096字节块
    
    AURORA_LOG_INFO("Memory manager initialized with {} memory pools", pools_.size());
}

void MemoryManager::shutdown() {
    AURORA_LOG_INFO("Shutting down memory manager...");
    pools_.clear();
    AURORA_LOG_INFO("Memory manager shutdown complete");
}

void* MemoryManager::allocate(size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 找到最合适的内存池（最小的大于等于size的块）
    MemoryPool* bestPool = nullptr;
    size_t bestBlockSize = std::numeric_limits<size_t>::max();
    
    for (auto& pool : pools_) {
        size_t blockSize = pool->getBlockSize();
        if (blockSize >= size && blockSize < bestBlockSize) {
            // 检查内存池是否有足够的空闲块
            if (pool->getFreeBlocks() > 0) {
                bestPool = pool.get();
                bestBlockSize = blockSize;
            }
        }
    }
    
    // 如果找到了合适的内存池
    if (bestPool) {
        void* ptr = bestPool->allocate();
        if (ptr) {
            allocatedBytes_ += bestBlockSize;
            AURORA_LOG_DEBUG("Allocated {} bytes from memory pool with block size {}", size, bestBlockSize);
            return ptr;
        }
    }
    
    // 没有合适的内存池，尝试使用稍大的内存池
    for (auto& pool : pools_) {
        size_t blockSize = pool->getBlockSize();
        if (blockSize > size && pool->getFreeBlocks() > 0) {
            void* ptr = pool->allocate();
            if (ptr) {
                allocatedBytes_ += blockSize;
                AURORA_LOG_DEBUG("Allocated {} bytes from memory pool with block size {} (next best)", size, blockSize);
                return ptr;
            }
        }
    }
    
    // 没有合适的内存池，使用标准分配器
    void* ptr = malloc(size);
    if (ptr) {
        allocatedBytes_ += size;
        AURORA_LOG_DEBUG("Allocated {} bytes using standard allocator", size);
    } else {
        AURORA_LOG_ERROR("Failed to allocate {} bytes", size);
    }
    return ptr;
}

void MemoryManager::deallocate(void* ptr) {
    if (!ptr) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 尝试在内存池中释放
    for (auto& pool : pools_) {
        if (pool->contains(ptr)) {
            pool->deallocate(ptr);
            allocatedBytes_ -= pool->getBlockSize();
            AURORA_LOG_DEBUG("Deallocated memory from memory pool with block size {}", pool->getBlockSize());
            return;
        }
    }
    
    // 不在任何内存池中，使用标准释放器
    free(ptr);
    // 注意：这里无法准确计算释放的字节数，因为我们不知道分配时的大小
    AURORA_LOG_DEBUG("Deallocated memory using standard deallocator");
}

void* MemoryManager::allocateAligned(size_t size, size_t alignment) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 对于对齐要求高的分配，直接使用标准分配器
    void* ptr = nullptr;
    #if defined(_WIN32)
    ptr = _aligned_malloc(size, alignment);
    #else
    int result = posix_memalign(&ptr, alignment, size);
    if (result != 0) {
        ptr = nullptr;
    }
    #endif
    
    if (ptr) {
        allocatedBytes_ += size;
        AURORA_LOG_DEBUG("Allocated {} bytes with alignment {} using aligned allocator", size, alignment);
    } else {
        AURORA_LOG_ERROR("Failed to allocate {} bytes with alignment {}", size, alignment);
    }
    return ptr;
}

void MemoryManager::deallocateAligned(void* ptr) {
    if (!ptr) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    #if defined(_WIN32)
    _aligned_free(ptr);
    #else
    free(ptr);
    #endif
    
    AURORA_LOG_DEBUG("Deallocated aligned memory");
}

void* MemoryManager::allocateShared(size_t size) {
    return SharedMemoryManager::instance().allocate(size);
}

void* MemoryManager::allocateDynamicShared(size_t size) {
    return SharedMemoryManager::instance().allocateDynamic(size);
}

void MemoryManager::deallocateShared(void* ptr) {
    SharedMemoryManager::instance().deallocate(ptr);
}

void MemoryManager::printMemoryStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AURORA_LOG_INFO("Memory Manager Statistics:");
    AURORA_LOG_INFO("Total allocated bytes: {}", allocatedBytes_);
    AURORA_LOG_INFO("Shared memory size: {}", SharedMemoryManager::instance().getTotalSharedMemorySize());
    AURORA_LOG_INFO("Number of memory pools: {}", pools_.size());
    
    for (size_t i = 0; i < pools_.size(); ++i) {
        const auto& pool = pools_[i];
        AURORA_LOG_INFO("\nMemory Pool {}", i);
        pool->printStats();
    }
}

} // namespace memory
} // namespace aurorart