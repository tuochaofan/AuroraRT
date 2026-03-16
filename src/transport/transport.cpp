#include "aurorart/transport/transport.h"
#include "aurorart/platform/platform_abstraction.h"
#include "aurorart/memory/memory_manager.h"
#include "aurorart/utils/logger.h"
#include <vector>
#include <cstring>

#if defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace aurorart {
namespace transport {

// IntraProcessTransport implementation

void IntraProcessTransport::init() {
    // 初始化无锁队列
    queue_ = std::make_unique<LockFreeQueue<std::vector<char>>>();
    status_ = TransportStatus::INITIALIZED;
    AURORA_LOG_INFO("IntraProcessTransport initialized with lock-free queue");
}

void IntraProcessTransport::start() {
    status_ = TransportStatus::RUNNING;
    AURORA_LOG_INFO("IntraProcessTransport started");
}

void Transport::stop() {
    status_ = TransportStatus::STOPPED;
    AURORA_LOG_INFO("Transport stopped");
}

void IntraProcessTransport::stop() {
    Transport::stop();
    // 清空队列
    if (queue_) {
        std::vector<char> dummy;
        while (queue_->dequeue(dummy)) {
            // 清空队列
        }
    }
    AURORA_LOG_INFO("IntraProcessTransport stopped");
}

bool IntraProcessTransport::send(const void* data, size_t size) {
    if (status_ != TransportStatus::RUNNING) {
        AURORA_LOG_ERROR("IntraProcessTransport not running");
        return false;
    }
    
    try {
        // 分配并复制数据
        std::vector<char> buffer(size);
        memcpy(buffer.data(), data, size);
        
        // 入队
        queue_->enqueue(std::move(buffer));
        
        // 更新统计信息
        total_bytes_sent_ += size;
        total_transactions_++;
        
        AURORA_LOG_DEBUG("IntraProcessTransport sent {} bytes", size);
        return true;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("IntraProcessTransport send error: {}", e.what());
        failure_count_++;
        return false;
    }
}

bool IntraProcessTransport::receive(void* data, size_t size) {
    if (status_ != TransportStatus::RUNNING) {
        AURORA_LOG_ERROR("IntraProcessTransport not running");
        return false;
    }
    
    std::vector<char> buffer;
    if (!queue_->dequeue(buffer)) {
        AURORA_LOG_DEBUG("IntraProcessTransport queue empty");
        return false;
    }
    
    if (buffer.size() < size) {
        AURORA_LOG_WARN("IntraProcessTransport buffer size insufficient: {} < {}", buffer.size(), size);
        return false;
    }
    
    try {
        memcpy(data, buffer.data(), size);
        
        // 更新统计信息
        total_bytes_received_ += size;
        
        AURORA_LOG_DEBUG("IntraProcessTransport received {} bytes", size);
        return true;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("IntraProcessTransport receive error: {}", e.what());
        failure_count_++;
        return false;
    }
}

TransportStatus IntraProcessTransport::getStatus() const {
    return status_;
}

void IntraProcessTransport::printStats() const {
    AURORA_LOG_INFO("IntraProcessTransport stats:");
    AURORA_LOG_INFO("  Total bytes sent: {}", total_bytes_sent_);
    AURORA_LOG_INFO("  Total bytes received: {}", total_bytes_received_);
    AURORA_LOG_INFO("  Total transactions: {}", total_transactions_);
    AURORA_LOG_INFO("  Failure count: {}", failure_count_);
    if (total_transactions_ > 0) {
        AURORA_LOG_INFO("  Average transaction size: {:.2f} bytes", 
            static_cast<double>(total_bytes_sent_) / total_transactions_);
    }
}

// 无锁队列实现 (基于Michael-Scott算法，增强版)
template <typename T>
class LockFreeQueue {
public:
    struct Node {
        T data;
        std::atomic<Node*> next;
        
        Node() : next(nullptr) {}
        Node(T d) : data(std::move(d)), next(nullptr) {}
    };
    
    LockFreeQueue() : head_(new Node()), tail_(head_), size_(0),
                     enqueue_count_(0), dequeue_count_(0),
                     peak_size_(0) {
    }
    
    ~LockFreeQueue() {
        Node* node = head_.load(std::memory_order_relaxed);
        while (node) {
            Node* next = node->next.load(std::memory_order_relaxed);
            delete node;
            node = next;
        }
    }
    
    // 优化：使用移动语义减少拷贝
    void enqueue(T data) {
        Node* new_node = new Node(std::move(data));
        new_node->next.store(nullptr, std::memory_order_relaxed);
        
        Node* old_tail = tail_.load(std::memory_order_relaxed);
        while (true) {
            Node* old_tail_next = old_tail->next.load(std::memory_order_acquire);
            if (old_tail == tail_.load(std::memory_order_relaxed)) {
                if (old_tail_next == nullptr) {
                    if (old_tail->next.compare_exchange_weak(
                        old_tail_next, new_node, 
                        std::memory_order_release, 
                        std::memory_order_relaxed)) {
                        tail_.compare_exchange_weak(
                            old_tail, new_node, 
                            std::memory_order_release, 
                            std::memory_order_relaxed);
                        // 更新统计信息
                        size_t current_size = size_.fetch_add(1, std::memory_order_relaxed) + 1;
                        size_t current_peak = peak_size_.load(std::memory_order_relaxed);
                        while (current_size > current_peak && 
                               !peak_size_.compare_exchange_weak(current_peak, current_size, 
                                                               std::memory_order_relaxed)) {
                            // 重试
                        }
                        enqueue_count_.fetch_add(1, std::memory_order_relaxed);
                        return;
                    }
                } else {
                    tail_.compare_exchange_weak(
                        old_tail, old_tail_next, 
                        std::memory_order_release, 
                        std::memory_order_relaxed);
                }
            }
            old_tail = tail_.load(std::memory_order_relaxed);
        }
    }
    
    // 批量入队优化
    template <typename Iterator>
    void enqueueBatch(Iterator begin, Iterator end) {
        size_t count = 0;
        Node* first = nullptr;
        Node* last = nullptr;
        
        // 构建节点链
        for (auto it = begin; it != end; ++it) {
            Node* new_node = new Node(std::move(*it));
            new_node->next.store(nullptr, std::memory_order_relaxed);
            
            if (!first) {
                first = new_node;
            } else {
                last->next.store(new_node, std::memory_order_relaxed);
            }
            last = new_node;
            count++;
        }
        
        if (!first) return;
        
        // 原子添加到队列
        Node* old_tail = tail_.load(std::memory_order_relaxed);
        while (true) {
            Node* old_tail_next = old_tail->next.load(std::memory_order_acquire);
            if (old_tail == tail_.load(std::memory_order_relaxed)) {
                if (old_tail_next == nullptr) {
                    if (old_tail->next.compare_exchange_weak(
                        old_tail_next, first, 
                        std::memory_order_release, 
                        std::memory_order_relaxed)) {
                        tail_.compare_exchange_weak(
                            old_tail, last, 
                            std::memory_order_release, 
                            std::memory_order_relaxed);
                        // 更新统计信息
                        size_t current_size = size_.fetch_add(count, std::memory_order_relaxed) + count;
                        size_t current_peak = peak_size_.load(std::memory_order_relaxed);
                        while (current_size > current_peak && 
                               !peak_size_.compare_exchange_weak(current_peak, current_size, 
                                                               std::memory_order_relaxed)) {
                            // 重试
                        }
                        enqueue_count_.fetch_add(count, std::memory_order_relaxed);
                        return;
                    }
                } else {
                    tail_.compare_exchange_weak(
                        old_tail, old_tail_next, 
                        std::memory_order_release, 
                        std::memory_order_relaxed);
                }
            }
            old_tail = tail_.load(std::memory_order_relaxed);
        }
    }
    
    bool dequeue(T& data) {
        while (true) {
            Node* old_head = head_.load(std::memory_order_relaxed);
            Node* old_tail = tail_.load(std::memory_order_relaxed);
            Node* old_head_next = old_head->next.load(std::memory_order_acquire);
            
            if (old_head == head_.load(std::memory_order_relaxed)) {
                if (old_head == old_tail) {
                    if (old_head_next == nullptr) {
                        return false;
                    }
                    tail_.compare_exchange_weak(
                        old_tail, old_head_next, 
                        std::memory_order_release, 
                        std::memory_order_relaxed);
                } else {
                    data = std::move(old_head_next->data);
                    if (head_.compare_exchange_weak(
                        old_head, old_head_next, 
                        std::memory_order_release, 
                        std::memory_order_relaxed)) {
                        delete old_head;
                        size_.fetch_sub(1, std::memory_order_relaxed);
                        dequeue_count_.fetch_add(1, std::memory_order_relaxed);
                        return true;
                    }
                }
            }
        }
    }
    
    // 批量出队优化
    template <typename Container>
    size_t dequeueBatch(Container& container, size_t max_count) {
        size_t count = 0;
        Node* old_head = head_.load(std::memory_order_relaxed);
        
        while (count < max_count) {
            Node* old_tail = tail_.load(std::memory_order_relaxed);
            Node* old_head_next = old_head->next.load(std::memory_order_acquire);
            
            if (old_head == head_.load(std::memory_order_relaxed)) {
                if (old_head == old_tail) {
                    if (old_head_next == nullptr) {
                        break;
                    }
                    tail_.compare_exchange_weak(
                        old_tail, old_head_next, 
                        std::memory_order_release, 
                        std::memory_order_relaxed);
                } else {
                    container.push_back(std::move(old_head_next->data));
                    Node* new_head = old_head_next;
                    if (head_.compare_exchange_weak(
                        old_head, new_head, 
                        std::memory_order_release, 
                        std::memory_order_relaxed)) {
                        delete old_head;
                        count++;
                        old_head = new_head;
                    } else {
                        break; // 竞争失败，退出批量操作
                    }
                }
            } else {
                old_head = head_.load(std::memory_order_relaxed);
            }
        }
        
        if (count > 0) {
            size_.fetch_sub(count, std::memory_order_relaxed);
            dequeue_count_.fetch_add(count, std::memory_order_relaxed);
        }
        
        return count;
    }
    
    bool empty() const {
        return size_.load(std::memory_order_acquire) == 0;
    }
    
    // 优化：使用原子变量直接获取大小
    size_t size() const {
        return size_.load(std::memory_order_acquire);
    }
    
    // 获取统计信息
    size_t getEnqueueCount() const {
        return enqueue_count_.load(std::memory_order_acquire);
    }
    
    size_t getDequeueCount() const {
        return dequeue_count_.load(std::memory_order_acquire);
    }
    
    size_t getPeakSize() const {
        return peak_size_.load(std::memory_order_acquire);
    }
    
    // 清空队列
    void clear() {
        T dummy;
        while (dequeue(dummy)) {
            // 清空队列
        }
    }
    
private:
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    std::atomic<size_t> size_;          // 队列大小
    std::atomic<size_t> enqueue_count_; // 入队计数
    std::atomic<size_t> dequeue_count_; // 出队计数
    std::atomic<size_t> peak_size_;     // 峰值大小
};

// 共享内存块结构
typedef struct {
    std::atomic<size_t> size;     // 数据大小
    std::atomic<bool> is_valid;   // 块是否有效
    std::atomic<bool> is_free;    // 块是否空闲
    size_t block_size;            // 实际块大小
    uint64_t sequence;            // 序列号，用于排序和追踪
    uint32_t crc;                 // CRC校验
    char padding[12];             // 对齐到64字节
    char data[0];                 // 数据区域
} ShmBlock;

// 内存池配置结构
struct MemoryPoolConfig {
    size_t block_size;        // 块大小
    size_t block_count;       // 块数量
    size_t alignment;         // 对齐要求
};

// 多级内存池管理 (使用无锁设计)
class MultiLevelMemoryPool {
private:
    struct PoolLevel {
        size_t block_size;
        std::unique_ptr<LockFreeQueue<ShmBlock*>> free_blocks;
        std::atomic<size_t> used_count;
        std::atomic<size_t> total_count;
        std::atomic<size_t> allocation_count;
        std::atomic<size_t> deallocation_count;
        
        PoolLevel(size_t size) : block_size(size), used_count(0), total_count(0),
                               allocation_count(0), deallocation_count(0) {
            free_blocks = std::make_unique<LockFreeQueue<ShmBlock*>>();
        }
    };
    
    std::vector<std::unique_ptr<PoolLevel>> pool_levels_;
    std::atomic<size_t> total_memory_;
    std::atomic<size_t> used_memory_;
    std::atomic<size_t> allocation_failures_;
    std::atomic<size_t> total_allocations_;
    std::atomic<size_t> total_deallocations_;
    std::atomic<bool> initialized_;
    uint64_t next_sequence_;
    
public:
    MultiLevelMemoryPool() : total_memory_(0), used_memory_(0), allocation_failures_(0),
                           total_allocations_(0), total_deallocations_(0),
                           initialized_(false), next_sequence_(0) {}
    
    void initialize(char* buffer, size_t size) {
        // 定义多级内存池的块大小（使用指数增长，更合理的分布）
        std::vector<size_t> block_sizes = {128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};
        
        char* current = buffer;
        size_t remaining = size;
        
        for (size_t block_size : block_sizes) {
            if (remaining < block_size + sizeof(ShmBlock)) {
                break;
            }
            
            size_t block_count = remaining / (block_size + sizeof(ShmBlock));
            if (block_count == 0) {
                continue;
            }
            
            // 确保至少有一个块
            block_count = std::max(size_t(1), block_count);
            
            auto pool_level = std::make_unique<PoolLevel>(block_size);
            pool_level->total_count = block_count;
            
            for (size_t i = 0; i < block_count; ++i) {
                // 对齐处理（64字节对齐，提高缓存命中率）
                size_t alignment = 64;
                size_t align_offset = (alignment - (reinterpret_cast<uintptr_t>(current) % alignment)) % alignment;
                current += align_offset;
                
                ShmBlock* block = reinterpret_cast<ShmBlock*>(current);
                block->size.store(0, std::memory_order_relaxed);
                block->is_valid.store(true, std::memory_order_relaxed);
                block->is_free.store(true, std::memory_order_relaxed);
                block->block_size = block_size;
                block->sequence = 0;
                block->crc = 0;
                pool_level->free_blocks->enqueue(block);
                
                current += block_size + sizeof(ShmBlock);
                remaining -= (block_size + sizeof(ShmBlock) + align_offset);
            }
            
            pool_levels_.push_back(std::move(pool_level));
        }
        
        total_memory_ = size - remaining;
        initialized_ = true;
        AURORA_LOG_INFO("MultiLevelMemoryPool initialized with {} bytes total memory, {} pool levels", 
                       total_memory_.load(), pool_levels_.size());
    }
    
    ShmBlock* allocate(size_t size) {
        if (!initialized_) {
            AURORA_LOG_ERROR("MultiLevelMemoryPool not initialized");
            return nullptr;
        }
        
        total_allocations_.fetch_add(1, std::memory_order_relaxed);
        
        // 找到最合适的内存池级别
        for (auto& pool_level : pool_levels_) {
            if (pool_level->block_size >= size) {
                ShmBlock* block = nullptr;
                if (pool_level->free_blocks->dequeue(block)) {
                    // 标记块为非空闲
                    block->is_free.store(false, std::memory_order_release);
                    block->is_valid.store(true, std::memory_order_release);
                    block->size.store(0, std::memory_order_relaxed);
                    block->sequence = next_sequence_++;
                    block->crc = 0;
                    
                    pool_level->used_count.fetch_add(1, std::memory_order_relaxed);
                    pool_level->allocation_count.fetch_add(1, std::memory_order_relaxed);
                    used_memory_.fetch_add(pool_level->block_size, std::memory_order_relaxed);
                    
                    return block;
                }
            }
        }
        
        // 没有找到合适的块，尝试更大的块
        for (auto& pool_level : pool_levels_) {
            if (pool_level->block_size > size) {
                ShmBlock* block = nullptr;
                if (pool_level->free_blocks->dequeue(block)) {
                    // 标记块为非空闲
                    block->is_free.store(false, std::memory_order_release);
                    block->is_valid.store(true, std::memory_order_release);
                    block->size.store(0, std::memory_order_relaxed);
                    block->sequence = next_sequence_++;
                    block->crc = 0;
                    
                    pool_level->used_count.fetch_add(1, std::memory_order_relaxed);
                    pool_level->allocation_count.fetch_add(1, std::memory_order_relaxed);
                    used_memory_.fetch_add(pool_level->block_size, std::memory_order_relaxed);
                    
                    return block;
                }
            }
        }
        
        // 所有池都没有可用块
        allocation_failures_.fetch_add(1, std::memory_order_relaxed);
        AURORA_LOG_WARN("MultiLevelMemoryPool allocation failed for size {}", size);
        return nullptr;
    }
    
    // 批量分配
    std::vector<ShmBlock*> allocateBatch(size_t size, size_t count) {
        std::vector<ShmBlock*> blocks;
        blocks.reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            ShmBlock* block = allocate(size);
            if (block) {
                blocks.push_back(block);
            } else {
                break;
            }
        }
        
        return blocks;
    }
    
    void deallocate(ShmBlock* block) {
        if (!block || !initialized_) return;
        
        total_deallocations_.fetch_add(1, std::memory_order_relaxed);
        
        // 标记块为空闲和有效
        block->size.store(0, std::memory_order_relaxed);
        block->is_free.store(true, std::memory_order_release);
        block->is_valid.store(true, std::memory_order_relaxed);
        block->sequence = 0;
        block->crc = 0;
        
        // 快速路径：找到对应的内存池级别并返回块
        size_t block_size = block->block_size;
        for (auto& pool_level : pool_levels_) {
            if (pool_level->block_size == block_size) {
                pool_level->free_blocks->enqueue(block);
                pool_level->used_count.fetch_sub(1, std::memory_order_relaxed);
                pool_level->deallocation_count.fetch_add(1, std::memory_order_relaxed);
                used_memory_.fetch_sub(pool_level->block_size, std::memory_order_relaxed);
                break;
            }
        }
    }
    
    // 批量释放
    void deallocateBatch(const std::vector<ShmBlock*>& blocks) {
        for (ShmBlock* block : blocks) {
            deallocate(block);
        }
    }
    
    size_t getBlockSize(ShmBlock* block) {
        if (block) {
            return block->block_size;
        }
        return 0;
    }
    
    size_t getTotalMemory() const { return total_memory_.load(std::memory_order_acquire); }
    size_t getUsedMemory() const { return used_memory_.load(std::memory_order_acquire); }
    size_t getAllocationFailures() const { return allocation_failures_.load(std::memory_order_acquire); }
    size_t getTotalAllocations() const { return total_allocations_.load(std::memory_order_acquire); }
    size_t getTotalDeallocations() const { return total_deallocations_.load(std::memory_order_acquire); }
    bool isInitialized() const { return initialized_.load(std::memory_order_acquire); }
    
    void printStats() {
        AURORA_LOG_INFO("MultiLevelMemoryPool stats:");
        AURORA_LOG_INFO("Total memory: {} bytes", getTotalMemory());
        AURORA_LOG_INFO("Used memory: {} bytes", getUsedMemory());
        AURORA_LOG_INFO("Total allocations: {}", getTotalAllocations());
        AURORA_LOG_INFO("Total deallocations: {}", getTotalDeallocations());
        AURORA_LOG_INFO("Allocation failures: {}", getAllocationFailures());
        
        for (size_t i = 0; i < pool_levels_.size(); ++i) {
            auto& pool_level = pool_levels_[i];
            AURORA_LOG_INFO("Level {}: block size={}, used={}/{}, allocations={}, deallocations={}", 
                           i, pool_level->block_size, 
                           pool_level->used_count.load(std::memory_order_acquire), 
                           pool_level->total_count.load(std::memory_order_acquire),
                           pool_level->allocation_count.load(std::memory_order_acquire),
                           pool_level->deallocation_count.load(std::memory_order_acquire));
        }
    }
};

// CRC32 校验函数
static uint32_t calculateCRC32(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    
    return ~crc;
}

// SharedMemoryTransport implementation

SharedMemoryTransport::SharedMemoryTransport(const std::string& name, size_t size)
    : name_(name), size_(size), shm_(nullptr), buffer_(nullptr), 
      queue_(std::make_unique<LockFreeQueue<ShmBlock*>>()),
      memory_pool_(std::make_unique<MultiLevelMemoryPool>()),
      current_read_block_(nullptr), current_write_block_(nullptr),
      dataAvailable_(false),
      total_bytes_sent_(0), total_bytes_received_(0),
      total_transactions_(0), failure_count_(0),
      peak_memory_usage_(0), current_memory_usage_(0),
      queue_size_(0) {
}

void SharedMemoryTransport::init() {
    // 创建共享内存
    shm_ = memory::SharedMemoryManager::instance().allocate(size_);
    if (shm_) {
        buffer_ = static_cast<char*>(shm_);
        // 初始化内存池
        initializeMemoryPool();
        status_ = TransportStatus::INITIALIZED;
        AURORA_LOG_INFO("SharedMemoryTransport initialized with name: {}, size: {} bytes", name_, size_);
    } else {
        status_ = TransportStatus::ERROR;
        AURORA_LOG_ERROR("Failed to allocate shared memory: {} bytes", size_);
    }
}

void SharedMemoryTransport::initializeMemoryPool() {
    // 使用多级内存池初始化
    memory_pool_->initialize(buffer_, size_);
    memory_pool_->printStats();
    AURORA_LOG_INFO("SharedMemoryTransport multi-level memory pool initialized");
}

void SharedMemoryTransport::start() {
    if (status_ == TransportStatus::INITIALIZED) {
        status_ = TransportStatus::RUNNING;
        AURORA_LOG_INFO("SharedMemoryTransport started");
    } else {
        AURORA_LOG_ERROR("SharedMemoryTransport cannot start in current status: {}", static_cast<int>(status_));
    }
}

void SharedMemoryTransport::stop() {
    // 释放共享内存
    if (shm_) {
        memory::SharedMemoryManager::instance().deallocate(shm_);
        shm_ = nullptr;
        buffer_ = nullptr;
    }
    
    // 清空队列
    queue_.reset(new LockFreeQueue<ShmBlock*>());
    memory_pool_.reset(new MultiLevelMemoryPool());
    
    current_read_block_ = nullptr;
    current_write_block_ = nullptr;
    dataAvailable_ = false;
    queue_size_ = 0;
    
    status_ = TransportStatus::STOPPED;
    AURORA_LOG_INFO("SharedMemoryTransport stopped");
}

bool SharedMemoryTransport::send(const void* data, size_t size) {
    if (status_ != TransportStatus::RUNNING) {
        AURORA_LOG_ERROR("SharedMemoryTransport not running");
        failure_count_++;
        return false;
    }
    
    if (!data) {
        AURORA_LOG_ERROR("SharedMemoryTransport::send: Invalid null data");
        failure_count_++;
        return false;
    }
    
    if (size == 0) {
        AURORA_LOG_ERROR("SharedMemoryTransport::send: Invalid size 0");
        failure_count_++;
        return false;
    }
    
    // 分配共享内存块
    ShmBlock* block = allocateBlock(size);
    if (!block) {
        AURORA_LOG_ERROR("Failed to allocate shared memory block for {} bytes", size);
        failure_count_++;
        return false;
    }
    
    try {
        // 复制数据到共享内存
        memcpy(block->data, data, size);
        block->size.store(size, std::memory_order_release);
        
        // 计算CRC校验
        block->crc = calculateCRC32(data, size);
        
        // 将块加入队列
        queue_->enqueue(block);
        dataAvailable_ = true;
        queue_size_++;
        
        // 更新统计信息
        total_bytes_sent_ += size;
        total_transactions_++;
        
        // 更新内存使用统计
        current_memory_usage_ += size;
        size_t current = current_memory_usage_.load();
        size_t peak = peak_memory_usage_.load();
        while (current > peak && !peak_memory_usage_.compare_exchange_weak(peak, current)) {
            // 原子操作失败，重试
        }
        
        AURORA_LOG_DEBUG("SharedMemoryTransport sent {} bytes, sequence={}, crc={:08x}", 
                       size, block->sequence, block->crc);
        return true;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("SharedMemoryTransport::send error: {}", e.what());
        freeBlock(block);
        failure_count_++;
        return false;
    }
}

// 批量发送
bool SharedMemoryTransport::sendBatch(const std::vector<std::pair<const void*, size_t>>& data_items) {
    if (status_ != TransportStatus::RUNNING) {
        AURORA_LOG_ERROR("SharedMemoryTransport not running");
        failure_count_++;
        return false;
    }
    
    if (data_items.empty()) {
        return true;
    }
    
    std::vector<ShmBlock*> blocks;
    blocks.reserve(data_items.size());
    
    // 分配所有需要的内存块
    for (const auto& item : data_items) {
        const void* data = item.first;
        size_t size = item.second;
        
        if (!data || size == 0) {
            AURORA_LOG_ERROR("SharedMemoryTransport::sendBatch: Invalid data or size");
            // 释放已分配的块
            for (ShmBlock* block : blocks) {
                freeBlock(block);
            }
            failure_count_++;
            return false;
        }
        
        ShmBlock* block = allocateBlock(size);
        if (!block) {
            AURORA_LOG_ERROR("Failed to allocate shared memory block for batch send");
            // 释放已分配的块
            for (ShmBlock* block : blocks) {
                freeBlock(block);
            }
            failure_count_++;
            return false;
        }
        
        // 复制数据并设置块信息
        memcpy(block->data, data, size);
        block->size.store(size, std::memory_order_release);
        block->crc = calculateCRC32(data, size);
        blocks.push_back(block);
    }
    
    // 批量入队
    for (ShmBlock* block : blocks) {
        queue_->enqueue(block);
        queue_size_++;
    }
    
    dataAvailable_ = true;
    
    // 更新统计信息
    for (size_t i = 0; i < blocks.size(); ++i) {
        size_t size = data_items[i].second;
        total_bytes_sent_ += size;
        total_transactions_++;
        current_memory_usage_ += size;
    }
    
    // 更新峰值内存使用
    size_t current = current_memory_usage_.load();
    size_t peak = peak_memory_usage_.load();
    while (current > peak && !peak_memory_usage_.compare_exchange_weak(peak, current)) {
        // 原子操作失败，重试
    }
    
    AURORA_LOG_DEBUG("SharedMemoryTransport sent batch of {} items", blocks.size());
    return true;
}

bool SharedMemoryTransport::receive(void* data, size_t size) {
    if (status_ != TransportStatus::RUNNING) {
        AURORA_LOG_ERROR("SharedMemoryTransport not running");
        failure_count_++;
        return false;
    }
    
    if (!data) {
        AURORA_LOG_ERROR("SharedMemoryTransport::receive: Invalid null data");
        failure_count_++;
        return false;
    }
    
    if (size == 0) {
        AURORA_LOG_ERROR("SharedMemoryTransport::receive: Invalid size 0");
        failure_count_++;
        return false;
    }
    
    ShmBlock* block = nullptr;
    if (!queue_->dequeue(block)) {
        dataAvailable_ = false;
        return false;
    }
    
    try {
        size_t block_size = block->size.load(std::memory_order_acquire);
        if (size < block_size) {
            AURORA_LOG_ERROR("Buffer size insufficient: {} < {}", size, block_size);
            freeBlock(block);
            failure_count_++;
            return false;
        }
        
        // 验证 CRC
        uint32_t calculated_crc = calculateCRC32(block->data, block_size);
        uint32_t block_crc = block->crc;
        if (calculated_crc != block_crc) {
            AURORA_LOG_ERROR("CRC mismatch: calculated={:08x}, block={:08x}", calculated_crc, block_crc);
            freeBlock(block);
            failure_count_++;
            return false;
        }
        
        // 复制数据
        memcpy(data, block->data, block_size);
        
        // 释放块
        freeBlock(block);
        
        // 检查是否还有数据
        dataAvailable_ = !queue_->empty();
        queue_size_--;
        
        // 更新统计信息
        total_bytes_received_ += block_size;
        
        AURORA_LOG_DEBUG("SharedMemoryTransport received {} bytes, sequence={}, crc={:08x}", 
                       block_size, block->sequence, block_crc);
        return true;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("SharedMemoryTransport::receive error: {}", e.what());
        if (block) {
            freeBlock(block);
        }
        failure_count_++;
        return false;
    }
}

// 批量接收
size_t SharedMemoryTransport::receiveBatch(std::vector<std::pair<void*, size_t>>& data_items) {
    if (status_ != TransportStatus::RUNNING) {
        AURORA_LOG_ERROR("SharedMemoryTransport not running");
        failure_count_++;
        return 0;
    }
    
    size_t received_count = 0;
    
    for (auto& item : data_items) {
        void* data = item.first;
        size_t size = item.second;
        
        if (!data || size == 0) {
            AURORA_LOG_ERROR("SharedMemoryTransport::receiveBatch: Invalid data or size");
            continue;
        }
        
        ShmBlock* block = nullptr;
        if (!queue_->dequeue(block)) {
            break;
        }
        
        try {
            size_t block_size = block->size.load(std::memory_order_acquire);
            if (size < block_size) {
                AURORA_LOG_ERROR("Buffer size insufficient: {} < {}", size, block_size);
                freeBlock(block);
                failure_count_++;
                continue;
            }
            
            // 验证 CRC
            uint32_t calculated_crc = calculateCRC32(block->data, block_size);
            uint32_t block_crc = block->crc;
            if (calculated_crc != block_crc) {
                AURORA_LOG_ERROR("CRC mismatch: calculated={:08x}, block={:08x}", calculated_crc, block_crc);
                freeBlock(block);
                failure_count_++;
                continue;
            }
            
            // 复制数据
            memcpy(data, block->data, block_size);
            item.second = block_size; // 更新实际接收的大小
            
            // 释放块
            freeBlock(block);
            
            // 更新统计信息
            total_bytes_received_ += block_size;
            received_count++;
            
        } catch (const std::exception& e) {
            AURORA_LOG_ERROR("SharedMemoryTransport::receiveBatch error: {}", e.what());
            if (block) {
                freeBlock(block);
            }
            failure_count_++;
        }
    }
    
    // 检查是否还有数据
    dataAvailable_ = !queue_->empty();
    queue_size_ -= received_count;
    
    AURORA_LOG_DEBUG("SharedMemoryTransport received batch of {} items", received_count);
    return received_count;
}

const void* SharedMemoryTransport::getReadBuffer() const {
    if (status_ != TransportStatus::RUNNING) {
        AURORA_LOG_ERROR("SharedMemoryTransport not running");
        return nullptr;
    }
    
    // 零拷贝读取
    ShmBlock* block = nullptr;
    if (!queue_->dequeue(block)) {
        dataAvailable_ = false;
        return nullptr;
    }
    
    try {
        // 检查块是否有效
        if (!block->is_valid.load(std::memory_order_acquire)) {
            AURORA_LOG_WARN("SharedMemoryTransport::getReadBuffer: Invalid block");
            freeBlock(block);
            dataAvailable_ = !queue_->empty();
            failure_count_++;
            return nullptr;
        }
        
        // 检查块大小
        size_t block_size = block->size.load(std::memory_order_acquire);
        if (block_size == 0) {
            AURORA_LOG_WARN("SharedMemoryTransport::getReadBuffer: Empty block");
            freeBlock(block);
            dataAvailable_ = !queue_->empty();
            failure_count_++;
            return nullptr;
        }
        
        // 保存当前读取的块
        current_read_block_ = block;
        queue_size_--;
        
        AURORA_LOG_DEBUG("SharedMemoryTransport::getReadBuffer: Got buffer of size {}", block_size);
        return block->data;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("SharedMemoryTransport::getReadBuffer error: {}", e.what());
        if (block) {
            freeBlock(block);
        }
        failure_count_++;
        dataAvailable_ = !queue_->empty();
        return nullptr;
    }
}

void* SharedMemoryTransport::getWriteBuffer(size_t size) {
    if (status_ != TransportStatus::RUNNING) {
        AURORA_LOG_ERROR("SharedMemoryTransport not running");
        return nullptr;
    }
    
    if (size == 0) {
        AURORA_LOG_ERROR("SharedMemoryTransport::getWriteBuffer: Invalid size 0");
        return nullptr;
    }
    
    try {
        // 分配块
        ShmBlock* block = allocateBlock(size);
        if (!block) {
            return nullptr;
        }
        
        // 标记块为有效
        block->is_valid.store(true, std::memory_order_release);
        
        // 保存当前写入的块
        current_write_block_ = block;
        
        AURORA_LOG_DEBUG("SharedMemoryTransport::getWriteBuffer: Got buffer of size {}", size);
        return block->data;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("SharedMemoryTransport::getWriteBuffer error: {}", e.what());
        failure_count_++;
        return nullptr;
    }
}

bool SharedMemoryTransport::commitWrite(size_t size) {
    if (!current_write_block_) {
        AURORA_LOG_ERROR("SharedMemoryTransport::commitWrite: No current write block");
        return false;
    }
    
    try {
        // 确保块大小不超过分配的大小
        size_t block_size = memory_pool_->getBlockSize(current_write_block_);
        if (size > block_size) {
            size = block_size;
            AURORA_LOG_WARN("Write size truncated to block size: {}", size);
        }
        
        current_write_block_->size.store(size, std::memory_order_release);
        current_write_block_->is_valid.store(true, std::memory_order_release);
        queue_->enqueue(current_write_block_);
        current_write_block_ = nullptr;
        dataAvailable_ = true;
        queue_size_++;
        
        // 更新统计信息
        total_bytes_sent_ += size;
        total_transactions_++;
        
        // 更新内存使用统计
        current_memory_usage_ += size;
        size_t current = current_memory_usage_.load();
        size_t peak = peak_memory_usage_.load();
        while (current > peak && !peak_memory_usage_.compare_exchange_weak(peak, current)) {
            // 原子操作失败，重试
        }
        
        AURORA_LOG_DEBUG("SharedMemoryTransport committed write of {} bytes", size);
        return true;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("SharedMemoryTransport::commitWrite error: {}", e.what());
        if (current_write_block_) {
            freeBlock(current_write_block_);
            current_write_block_ = nullptr;
        }
        failure_count_++;
        return false;
    }
}

bool SharedMemoryTransport::commitRead() {
    if (!current_read_block_) {
        AURORA_LOG_ERROR("SharedMemoryTransport::commitRead: No current read block");
        return false;
    }
    
    try {
        // 标记块为无效
        current_read_block_->is_valid.store(false, std::memory_order_release);
        freeBlock(current_read_block_);
        current_read_block_ = nullptr;
        
        // 检查是否还有数据
        dataAvailable_ = !queue_->empty();
        
        AURORA_LOG_DEBUG("SharedMemoryTransport committed read");
        return true;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("SharedMemoryTransport::commitRead error: {}", e.what());
        if (current_read_block_) {
            try {
                freeBlock(current_read_block_);
            } catch (...) {
                // 避免嵌套异常
            }
            current_read_block_ = nullptr;
        }
        failure_count_++;
        return false;
    }
}

ShmBlock* SharedMemoryTransport::allocateBlock(size_t size) {
    if (size == 0) {
        AURORA_LOG_ERROR("SharedMemoryTransport::allocateBlock: Invalid size 0");
        return nullptr;
    }
    
    // 检查内存池是否初始化
    if (!memory_pool_ || !memory_pool_->isInitialized()) {
        AURORA_LOG_ERROR("SharedMemoryTransport::allocateBlock: Memory pool not initialized");
        failure_count_++;
        return nullptr;
    }
    
    // 尝试分配块
    ShmBlock* block = memory_pool_->allocate(size);
    if (!block) {
        // 内存池分配失败，尝试扩展共享内存
        AURORA_LOG_WARN("Failed to allocate shared memory block for {} bytes, attempting to expand", size);
        if (expandSharedMemory()) {
            // 扩展成功，再次尝试分配
            block = memory_pool_->allocate(size);
            if (!block) {
                AURORA_LOG_ERROR("Failed to allocate shared memory block even after expansion");
                failure_count_++;
                return nullptr;
            }
        } else {
            AURORA_LOG_ERROR("Failed to expand shared memory");
            failure_count_++;
            return nullptr;
        }
    }
    
    // 检查块大小是否足够
    size_t block_size = memory_pool_->getBlockSize(block);
    if (block_size < size) {
        // 块大小不足，返回自由队列
        memory_pool_->deallocate(block);
        AURORA_LOG_WARN("Block size insufficient: {} < {}", block_size, size);
        failure_count_++;
        return nullptr;
    }
    
    // 标记块为非空闲和有效
    block->is_free.store(false, std::memory_order_release);
    block->is_valid.store(true, std::memory_order_release);
    block->size.store(0, std::memory_order_relaxed);
    
    AURORA_LOG_DEBUG("Allocated shared memory block: size={}, block_size={}", size, block_size);
    return block;
}

void SharedMemoryTransport::freeBlock(ShmBlock* block) {
    if (!block) {
        AURORA_LOG_ERROR("SharedMemoryTransport::freeBlock: Invalid null block");
        return;
    }
    
    size_t block_size = block->size.load(std::memory_order_acquire);
    memory_pool_->deallocate(block);
    
    // 更新内存使用统计
    current_memory_usage_ -= block_size;
    
    AURORA_LOG_DEBUG("Freed shared memory block: size={}", block_size);
}

bool SharedMemoryTransport::expandSharedMemory() {
    // 尝试扩展共享内存
    size_t new_size = size_ * 2; // 翻倍扩展
    AURORA_LOG_INFO("Attempting to expand shared memory from {} to {} bytes", size_, new_size);
    
    try {
        // 分配新的共享内存
        void* new_shm = memory::SharedMemoryManager::instance().allocate(new_size);
        if (!new_shm) {
            AURORA_LOG_ERROR("Failed to allocate new shared memory");
            return false;
        }
        
        // 复制旧内存数据到新内存
        if (shm_ && buffer_) {
            memcpy(new_shm, buffer_, size_);
        }
        
        // 释放旧的共享内存
        if (shm_) {
            memory::SharedMemoryManager::instance().deallocate(shm_);
        }
        
        // 更新共享内存指针和大小
        shm_ = new_shm;
        buffer_ = static_cast<char*>(shm_);
        size_ = new_size;
        
        // 重新初始化内存池
        initializeMemoryPool();
        
        AURORA_LOG_INFO("Successfully expanded shared memory to {} bytes", size_);
        return true;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("Error expanding shared memory: {}", e.what());
        return false;
    }
}

// 模板方法实现
template <typename T>
T* SharedMemoryTransport::getTypedWriteBuffer() {
    return reinterpret_cast<T*>(getWriteBuffer(sizeof(T)));
}

template <typename T>
const T* SharedMemoryTransport::getTypedReadBuffer() const {
    return reinterpret_cast<const T*>(getReadBuffer());
}

TransportStatus SharedMemoryTransport::getStatus() const {
    return status_;
}

void SharedMemoryTransport::printStats() const {
    AURORA_LOG_INFO("SharedMemoryTransport stats:");
    AURORA_LOG_INFO("  Name: {}", name_);
    AURORA_LOG_INFO("  Size: {} bytes", size_);
    AURORA_LOG_INFO("  Total bytes sent: {}", total_bytes_sent_);
    AURORA_LOG_INFO("  Total bytes received: {}", total_bytes_received_);
    AURORA_LOG_INFO("  Total transactions: {}", total_transactions_);
    AURORA_LOG_INFO("  Failure count: {}", failure_count_);
    AURORA_LOG_INFO("  Current memory usage: {} bytes", current_memory_usage_);
    AURORA_LOG_INFO("  Peak memory usage: {} bytes", peak_memory_usage_);
    AURORA_LOG_INFO("  Queue size: {}", queue_size_);
    if (total_transactions_ > 0) {
        AURORA_LOG_INFO("  Average transaction size: {:.2f} bytes", 
            static_cast<double>(total_bytes_sent_) / total_transactions_);
    }
    if (memory_pool_) {
        memory_pool_->printStats();
    }
}

// 连接池结构
struct ConnectionPool {
    std::vector<int> sockets;
    std::mutex mutex;
    std::condition_variable cond;
    size_t max_connections;
    size_t current_connections;
    bool shutting_down;
    
    ConnectionPool(size_t max_conn) : max_connections(max_conn), current_connections(0), shutting_down(false) {}
};

// NetworkTransport implementation

NetworkTransport::NetworkTransport(const std::string& host, int port)
    : host_(host), port_(port), sockfd_(-1), available_(false),
      send_timeout_(5000), receive_timeout_(5000),
      recv_buffer_size_(8192), send_buffer_size_(8192),
      total_bytes_sent_(0), total_bytes_received_(0),
      total_transactions_(0), failure_count_(0),
      reconnect_attempts_(0), max_reconnect_attempts_(5),
      reconnect_delay_(1000), last_activity_time_(0),
      connection_pool_(std::make_unique<ConnectionPool>(10)) {
    // 初始化远程地址
    memset(&remote_addr_, 0, sizeof(remote_addr_));
    remote_addr_.sin_family = AF_INET;
    remote_addr_.sin_port = htons(port_);
    remote_addr_.sin_addr.s_addr = inet_addr(host_.c_str());
    
    // 初始化连接池
    initConnectionPool();
}

void NetworkTransport::initConnectionPool() {
    // 预创建一些连接
    for (size_t i = 0; i < 3; ++i) {
        int sock = createConnection();
        if (sock != -1) {
            std::lock_guard<std::mutex> lock(connection_pool_->mutex);
            connection_pool_->sockets.push_back(sock);
            connection_pool_->current_connections++;
        }
    }
    AURORA_LOG_INFO("Connection pool initialized with {} connections", connection_pool_->current_connections);
}

int NetworkTransport::createConnection() {
    // 创建新的套接字连接
    int sock = platform::PlatformManager::instance().getPlatform()->createSocket(
        AF_INET, SOCK_DGRAM, 0
    );
    
    if (sock != -1) {
        // 设置非阻塞和地址重用
        platform::PlatformManager::instance().getPlatform()->setSocketNonBlocking(sock);
        platform::PlatformManager::instance().getPlatform()->setSocketReuseAddr(sock);
        
        // 优化套接字选项
        #if defined(__linux__) || defined(__APPLE__)
        // 设置缓冲区大小
        int opt = recv_buffer_size_;
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
        opt = send_buffer_size_;
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt));
        
        // 设置发送超时
        opt = send_timeout_;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &opt, sizeof(opt));
        
        // 设置接收超时
        opt = receive_timeout_;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &opt, sizeof(opt));
        
        // 启用快速回收
        opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        
        // 禁用Nagle算法
        opt = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        #elif defined(_WIN32)
        // Windows 平台的套接字选项设置
        DWORD opt = recv_buffer_size_;
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&opt, sizeof(opt));
        opt = send_buffer_size_;
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&opt, sizeof(opt));
        
        // 设置发送超时
        opt = send_timeout_;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&opt, sizeof(opt));
        
        // 设置接收超时
        opt = receive_timeout_;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&opt, sizeof(opt));
        #endif
    }
    
    return sock;
}

int NetworkTransport::getConnection() {
    std::unique_lock<std::mutex> lock(connection_pool_->mutex);
    
    // 等待可用连接或超时
    if (connection_pool_->sockets.empty() && connection_pool_->current_connections < connection_pool_->max_connections) {
        // 创建新连接
        lock.unlock();
        int sock = createConnection();
        if (sock != -1) {
            lock.lock();
            connection_pool_->current_connections++;
            return sock;
        }
    }
    
    // 等待可用连接
    if (connection_pool_->sockets.empty()) {
        if (!connection_pool_->cond.wait_for(lock, std::chrono::milliseconds(1000), 
            [this]() { return !connection_pool_->sockets.empty() || connection_pool_->shutting_down; })) {
            return -1; // 超时
        }
        
        if (connection_pool_->shutting_down) {
            return -1;
        }
    }
    
    int sock = connection_pool_->sockets.back();
    connection_pool_->sockets.pop_back();
    return sock;
}

void NetworkTransport::returnConnection(int sock) {
    if (sock == -1) return;
    
    std::lock_guard<std::mutex> lock(connection_pool_->mutex);
    if (!connection_pool_->shutting_down) {
        connection_pool_->sockets.push_back(sock);
        connection_pool_->cond.notify_one();
    } else {
        platform::PlatformManager::instance().getPlatform()->closeSocket(sock);
    }
}

void NetworkTransport::init() {
    // 初始化Winsock
    #if defined(_WIN32)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        AURORA_LOG_ERROR("WSAStartup failed");
        status_ = TransportStatus::ERROR;
        return;
    }
    #endif
    
    // 创建主套接字
    sockfd_ = platform::PlatformManager::instance().getPlatform()->createSocket(
        AF_INET, SOCK_DGRAM, 0
    );
    
    if (sockfd_ != -1) {
        // 设置非阻塞和地址重用
        platform::PlatformManager::instance().getPlatform()->setSocketNonBlocking(sockfd_);
        platform::PlatformManager::instance().getPlatform()->setSocketReuseAddr(sockfd_);
        
        // 优化套接字选项
        #if defined(__linux__) || defined(__APPLE__)
        // 设置缓冲区大小
        int opt = recv_buffer_size_;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
        opt = send_buffer_size_;
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt));
        
        // 设置发送超时
        opt = send_timeout_;
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &opt, sizeof(opt));
        
        // 设置接收超时
        opt = receive_timeout_;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &opt, sizeof(opt));
        
        // 启用快速回收
        opt = 1;
        setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        
        // 禁用Nagle算法
        opt = 1;
        setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        #elif defined(_WIN32)
        // Windows 平台的套接字选项设置
        DWORD opt = recv_buffer_size_;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF, (char*)&opt, sizeof(opt));
        opt = send_buffer_size_;
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF, (char*)&opt, sizeof(opt));
        
        // 设置发送超时
        opt = send_timeout_;
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, (char*)&opt, sizeof(opt));
        
        // 设置接收超时
        opt = receive_timeout_;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, (char*)&opt, sizeof(opt));
        #endif
        
        // 绑定地址
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        addr.sin_addr.s_addr = inet_addr(host_.c_str());
        
        if (bind(sockfd_, (sockaddr*)&addr, sizeof(addr)) == 0) {
            available_ = true;
            status_ = TransportStatus::INITIALIZED;
            AURORA_LOG_INFO("NetworkTransport initialized with host: {}, port: {}, buffer size: {}B, timeout: {}ms", 
                           host_, port_, recv_buffer_size_, send_timeout_);
        } else {
            #if defined(_WIN32)
            int error = WSAGetLastError();
            AURORA_LOG_ERROR("Failed to bind socket: {}", error);
            #else
            AURORA_LOG_ERROR("Failed to bind socket: {}", strerror(errno));
            #endif
            status_ = TransportStatus::ERROR;
        }
    } else {
        AURORA_LOG_ERROR("Failed to create socket");
        status_ = TransportStatus::ERROR;
    }
}

void NetworkTransport::start() {
    if (status_ == TransportStatus::INITIALIZED && available_) {
        status_ = TransportStatus::RUNNING;
        AURORA_LOG_INFO("NetworkTransport started");
    } else {
        AURORA_LOG_ERROR("NetworkTransport cannot start in current status: {}, available: {}", 
                       static_cast<int>(status_), available_);
    }
}

void NetworkTransport::stop() {
    if (sockfd_ != -1) {
        platform::PlatformManager::instance().getPlatform()->closeSocket(sockfd_);
        sockfd_ = -1;
        available_ = false;
    }
    
    #if defined(_WIN32)
    WSACleanup();
    #endif
    
    status_ = TransportStatus::STOPPED;
    AURORA_LOG_INFO("NetworkTransport stopped");
}

bool NetworkTransport::send(const void* data, size_t size) {
    if (status_ != TransportStatus::RUNNING || !available_) {
        AURORA_LOG_ERROR("NetworkTransport not available");
        failure_count_++;
        return false;
    }
    
    // 检查连接是否活跃
    uint64_t current_time = std::chrono::system_clock::now().time_since_epoch().count();
    if (current_time - last_activity_time_ > 30000000000LL) { // 30秒无活动，检查连接
        if (!isAvailable()) {
            AURORA_LOG_WARN("NetworkTransport connection inactive, attempting to reconnect");
            if (!reconnect()) {
                failure_count_++;
                return false;
            }
        }
    }
    
    // 实现数据分片传输
    const size_t MAX_PACKET_SIZE = send_buffer_size_;
    const char* data_ptr = static_cast<const char*>(data);
    size_t remaining = size;
    
    // 发送头部信息：总大小
    uint32_t total_size = htonl(static_cast<uint32_t>(size));
    int sent = sendto(
        sockfd_,
        &total_size,
        sizeof(total_size),
        0,
        (sockaddr*)&remote_addr_,
        sizeof(remote_addr_)
    );
    if (sent != static_cast<int>(sizeof(total_size))) {
        #if defined(_WIN32)
        int error = WSAGetLastError();
        AURORA_LOG_ERROR("NetworkTransport send header error: {}, sent: {}, expected: {}", 
                       error, sent, sizeof(total_size));
        #else
        AURORA_LOG_ERROR("NetworkTransport send header error: {}, sent: {}, expected: {}", 
                       strerror(errno), sent, sizeof(total_size));
        #endif
        
        // 尝试重连
        if (reconnect_attempts_ < max_reconnect_attempts_) {
            AURORA_LOG_INFO("Attempting to reconnect...");
            if (reconnect()) {
                // 重连成功，重新发送
                return send(data, size);
            }
        }
        
        failure_count_++;
        return false;
    }
    
    // 分片段发送数据
    while (remaining > 0) {
        size_t chunk_size = std::min(remaining, MAX_PACKET_SIZE);
        
        // 优化：使用循环发送，直到所有数据发送完成
        size_t bytes_sent = 0;
        while (bytes_sent < chunk_size) {
            int sent = sendto(
                sockfd_,
                data_ptr + bytes_sent,
                chunk_size - bytes_sent,
                0,
                (sockaddr*)&remote_addr_,
                sizeof(remote_addr_)
            );
            
            if (sent > 0) {
                bytes_sent += sent;
            } else {
                #if defined(_WIN32)
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    // 非阻塞模式下发送缓冲区满，等待一段时间
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    continue;
                }
                AURORA_LOG_ERROR("NetworkTransport send chunk error: {}, sent: {}, expected: {}", 
                               error, sent, chunk_size);
                #else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 非阻塞模式下发送缓冲区满，等待一段时间
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    continue;
                }
                AURORA_LOG_ERROR("NetworkTransport send chunk error: {}, sent: {}, expected: {}", 
                               strerror(errno), sent, chunk_size);
                #endif
                
                // 尝试重连
                if (reconnect_attempts_ < max_reconnect_attempts_) {
                    AURORA_LOG_INFO("Attempting to reconnect...");
                    if (reconnect()) {
                        // 重连成功，重新发送
                        return send(data, size);
                    }
                }
                
                failure_count_++;
                return false;
            }
        }
        
        data_ptr += chunk_size;
        remaining -= chunk_size;
    }
    
    // 更新统计信息
    total_bytes_sent_ += size;
    total_transactions_++;
    last_activity_time_ = current_time;
    
    AURORA_LOG_DEBUG("NetworkTransport sent {} bytes to {}:{}", size, host_, port_);
    return true;
}

bool NetworkTransport::receive(void* data, size_t size) {
    if (status_ != TransportStatus::RUNNING || !available_) {
        AURORA_LOG_ERROR("NetworkTransport not available");
        failure_count_++;
        return false;
    }
    
    // 检查连接是否活跃
    uint64_t current_time = std::chrono::system_clock::now().time_since_epoch().count();
    if (current_time - last_activity_time_ > 30000000000LL) { // 30秒无活动，检查连接
        if (!isAvailable()) {
            AURORA_LOG_WARN("NetworkTransport connection inactive, attempting to reconnect");
            if (!reconnect()) {
                failure_count_++;
                return false;
            }
        }
    }
    
    // 接收头部信息：总大小
    uint32_t total_size;
    sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    
    int received = recvfrom(
        sockfd_,
        &total_size,
        sizeof(total_size),
        0,
        (sockaddr*)&addr,
        &addrlen
    );
    
    if (received != static_cast<int>(sizeof(total_size))) {
        #if defined(_WIN32)
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            // 非阻塞模式下没有数据，不是错误
            return false;
        }
        AURORA_LOG_ERROR("NetworkTransport receive header error: {}", error);
        #else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 非阻塞模式下没有数据，不是错误
            return false;
        }
        AURORA_LOG_ERROR("NetworkTransport receive header error: {}", strerror(errno));
        #endif
        
        // 尝试重连
        if (reconnect_attempts_ < max_reconnect_attempts_) {
            AURORA_LOG_INFO("Attempting to reconnect...");
            if (reconnect()) {
                // 重连成功，重新接收
                return receive(data, size);
            }
        }
        
        failure_count_++;
        return false;
    }
    
    // 转换为本地字节序
    total_size = ntohl(total_size);
    
    // 检查缓冲区大小
    if (size < total_size) {
        AURORA_LOG_ERROR("Receive buffer size ({}) is smaller than expected data size ({})", size, total_size);
        failure_count_++;
        return false;
    }
    
    // 接收数据
    char* data_ptr = static_cast<char*>(data);
    size_t remaining = total_size;
    
    while (remaining > 0) {
        size_t chunk_size = std::min(remaining, recv_buffer_size_);
        
        // 优化：使用循环接收，直到所有数据接收完成
        size_t bytes_received = 0;
        while (bytes_received < chunk_size) {
            received = recvfrom(
                sockfd_,
                data_ptr + bytes_received,
                chunk_size - bytes_received,
                0,
                (sockaddr*)&addr,
                &addrlen
            );
            
            if (received > 0) {
                bytes_received += received;
            } else if (received == 0) {
                AURORA_LOG_WARN("NetworkTransport connection closed during receive");
                
                // 尝试重连
                if (reconnect_attempts_ < max_reconnect_attempts_) {
                    AURORA_LOG_INFO("Attempting to reconnect...");
                    reconnect();
                }
                
                failure_count_++;
                return false;
            } else {
                #if defined(_WIN32)
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    // 非阻塞模式下没有数据，等待一段时间
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    continue;
                }
                AURORA_LOG_ERROR("NetworkTransport receive chunk error: {}", error);
                #else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 非阻塞模式下没有数据，等待一段时间
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    continue;
                }
                AURORA_LOG_ERROR("NetworkTransport receive chunk error: {}", strerror(errno));
                #endif
                
                // 尝试重连
                if (reconnect_attempts_ < max_reconnect_attempts_) {
                    AURORA_LOG_INFO("Attempting to reconnect...");
                    if (reconnect()) {
                        // 重连成功，重新接收
                        return receive(data, size);
                    }
                }
                
                failure_count_++;
                return false;
            }
        }
        
        data_ptr += chunk_size;
        remaining -= chunk_size;
    }
    
    // 更新统计信息
    total_bytes_received_ += total_size;
    last_activity_time_ = current_time;
    
    char clientAddr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), clientAddr, INET_ADDRSTRLEN);
    AURORA_LOG_DEBUG("NetworkTransport received {} bytes from {}:{}", 
                   total_size, clientAddr, ntohs(addr.sin_port));
    return true;
}

bool NetworkTransport::isAvailable() const {
    return available_ && status_ == TransportStatus::RUNNING;
}

bool NetworkTransport::reconnect() {
    AURORA_LOG_INFO("Attempting to reconnect to {}:{}, attempt {}/{}", 
                   host_, port_, reconnect_attempts_ + 1, max_reconnect_attempts_);
    
    // 关闭当前连接
    if (sockfd_ != -1) {
        platform::PlatformManager::instance().getPlatform()->closeSocket(sockfd_);
        sockfd_ = -1;
    }
    
    // 等待重连延迟
    std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_delay_));
    
    // 重新创建套接字
    sockfd_ = platform::PlatformManager::instance().getPlatform()->createSocket(
        AF_INET, SOCK_DGRAM, 0
    );
    
    if (sockfd_ != -1) {
        // 设置非阻塞和地址重用
        platform::PlatformManager::instance().getPlatform()->setSocketNonBlocking(sockfd_);
        platform::PlatformManager::instance().getPlatform()->setSocketReuseAddr(sockfd_);
        
        // 优化套接字选项
        #if defined(__linux__) || defined(__APPLE__)
        // 设置缓冲区大小
        int opt = recv_buffer_size_;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
        opt = send_buffer_size_;
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt));
        
        // 设置发送超时
        opt = send_timeout_;
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &opt, sizeof(opt));
        
        // 设置接收超时
        opt = receive_timeout_;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &opt, sizeof(opt));
        
        // 启用快速回收
        opt = 1;
        setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        
        // 禁用Nagle算法
        opt = 1;
        setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        #elif defined(_WIN32)
        // Windows 平台的套接字选项设置
        DWORD opt = recv_buffer_size_;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF, (char*)&opt, sizeof(opt));
        opt = send_buffer_size_;
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF, (char*)&opt, sizeof(opt));
        
        // 设置发送超时
        opt = send_timeout_;
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, (char*)&opt, sizeof(opt));
        
        // 设置接收超时
        opt = receive_timeout_;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, (char*)&opt, sizeof(opt));
        #endif
        
        // 绑定地址
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        addr.sin_addr.s_addr = inet_addr(host_.c_str());
        
        if (bind(sockfd_, (sockaddr*)&addr, sizeof(addr)) == 0) {
            available_ = true;
            status_ = TransportStatus::RUNNING;
            reconnect_attempts_ = 0; // 重置重连尝试次数
            AURORA_LOG_INFO("Successfully reconnected to {}:{}", host_, port_);
            return true;
        } else {
            #if defined(_WIN32)
            int error = WSAGetLastError();
            AURORA_LOG_ERROR("Failed to bind socket during reconnect: {}", error);
            #else
            AURORA_LOG_ERROR("Failed to bind socket during reconnect: {}", strerror(errno));
            #endif
            sockfd_ = -1;
            reconnect_attempts_++;
            return false;
        }
    } else {
        AURORA_LOG_ERROR("Failed to create socket during reconnect");
        reconnect_attempts_++;
        return false;
    }
}

void NetworkTransport::printStats() const {
    AURORA_LOG_INFO("NetworkTransport stats:");
    AURORA_LOG_INFO("  Host: {}", host_);
    AURORA_LOG_INFO("  Port: {}", port_);
    AURORA_LOG_INFO("  Total bytes sent: {}", total_bytes_sent_);
    AURORA_LOG_INFO("  Total bytes received: {}", total_bytes_received_);
    AURORA_LOG_INFO("  Total transactions: {}", total_transactions_);
    AURORA_LOG_INFO("  Failure count: {}", failure_count_);
    AURORA_LOG_INFO("  Reconnect attempts: {}", reconnect_attempts_);
    if (total_transactions_ > 0) {
        AURORA_LOG_INFO("  Average transaction size: {:.2f} bytes", 
            static_cast<double>(total_bytes_sent_) / total_transactions_);
    }
    if (total_bytes_sent_ > 0) {
        double throughput = static_cast<double>(total_bytes_sent_) / 1024.0 / 1024.0; // MB
        AURORA_LOG_INFO("  Total throughput: {:.2f} MB", throughput);
    }
}

TransportStatus NetworkTransport::getStatus() const {
    return status_;
}

// TSN传输的时间同步实现
class TSNTransport::TimeSyncManager {
private:
    bool enabled_;
    std::atomic<uint64_t> current_time_;
    std::thread sync_thread_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> sync_offset_; // 时间同步偏移
    std::atomic<bool> synchronized_; // 是否已同步
    std::atomic<uint64_t> last_sync_time_; // 上次同步时间
    std::atomic<int> sync_quality_; // 同步质量 (0-100)
    
public:
    TimeSyncManager() : enabled_(false), current_time_(0), running_(false), sync_offset_(0), 
                       synchronized_(false), last_sync_time_(0), sync_quality_(0) {}
    
    ~TimeSyncManager() {
        stop();
    }
    
    void start() {
        if (!enabled_ || running_) return;
        
        running_ = true;
        sync_thread_ = std::thread([this]() {
            while (running_) {
                // 模拟时间同步
                // 实际应用中应该使用PTP或其他时间同步协议
                auto now = std::chrono::system_clock::now().time_since_epoch().count();
                
                // 模拟同步过程
                if (!synchronized_.load()) {
                    // 模拟初始同步
                    sync_offset_.store(0);
                    synchronized_.store(true);
                    sync_quality_.store(95); // 初始同步质量
                    last_sync_time_.store(now);
                    AURORA_LOG_INFO("TimeSyncManager synchronized");
                } else {
                    // 模拟周期性同步调整
                    if (now - last_sync_time_.load() > 1000000000) { // 1秒
                        // 模拟时间漂移调整
                        int64_t drift = (rand() % 10) - 5; // -5到+5纳秒的漂移
                        sync_offset_.fetch_add(drift);
                        last_sync_time_.store(now);
                        
                        // 更新同步质量
                        int quality = std::max(0, std::min(100, sync_quality_.load() - (rand() % 3) + 2));
                        sync_quality_.store(quality);
                    }
                }
                
                current_time_ = now + sync_offset_.load();
                std::this_thread::sleep_for(std::chrono::microseconds(100)); // 提高同步频率
            }
        });
        AURORA_LOG_INFO("TimeSyncManager started");
    }
    
    void stop() {
        running_ = false;
        if (sync_thread_.joinable()) {
            sync_thread_.join();
        }
        synchronized_ = false;
        sync_quality_ = 0;
        AURORA_LOG_INFO("TimeSyncManager stopped");
    }
    
    void setEnabled(bool enabled) {
        enabled_ = enabled;
        if (enabled) {
            start();
        } else {
            stop();
        }
    }
    
    uint64_t getCurrentTime() const {
        return current_time_;
    }
    
    bool isSynchronized() const {
        return synchronized_.load();
    }
    
    int getSyncQuality() const {
        return sync_quality_.load();
    }
    
    void adjustTimeOffset(int64_t offset) {
        sync_offset_.fetch_add(offset);
        last_sync_time_.store(std::chrono::system_clock::now().time_since_epoch().count());
    }
    
    uint64_t getLastSyncTime() const {
        return last_sync_time_.load();
    }
};

// TSN传输的流量调度实现
class TSNTransport::TrafficScheduler {
private:
    bool enabled_;
    std::atomic<uint64_t> schedule_time_;
    std::thread schedule_thread_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> cycle_time_; // 调度周期
    std::atomic<uint64_t> current_slot_; // 当前时间槽
    std::atomic<uint64_t> slot_duration_; // 时间槽持续时间
    std::atomic<uint32_t> max_slots_; // 最大时间槽数
    std::atomic<uint32_t> priority_map_[8]; // 优先级到时间槽的映射
    
public:
    TrafficScheduler() : enabled_(false), schedule_time_(0), running_(false), 
                       cycle_time_(1000000), current_slot_(0), slot_duration_(100000), 
                       max_slots_(10) {
        // 初始化优先级映射
        for (int i = 0; i < 8; ++i) {
            priority_map_[i].store(1 << i); // 每个优先级对应不同的时间槽
        }
    }
    
    ~TrafficScheduler() {
        stop();
    }
    
    void start() {
        if (!enabled_ || running_) return;
        
        running_ = true;
        schedule_thread_ = std::thread([this]() {
            while (running_) {
                // 模拟流量调度
                // 实际应用中应该根据TSN调度表进行调度
                auto now = std::chrono::system_clock::now().time_since_epoch().count();
                schedule_time_ = now;
                
                // 计算当前时间槽
                uint64_t slot = (now / slot_duration_.load()) % max_slots_.load();
                current_slot_.store(slot);
                
                std::this_thread::sleep_for(std::chrono::microseconds(100)); // 提高调度频率
            }
        });
        AURORA_LOG_INFO("TrafficScheduler started");
    }
    
    void stop() {
        running_ = false;
        if (schedule_thread_.joinable()) {
            schedule_thread_.join();
        }
        AURORA_LOG_INFO("TrafficScheduler stopped");
    }
    
    void setEnabled(bool enabled) {
        enabled_ = enabled;
        if (enabled) {
            start();
        } else {
            stop();
        }
    }
    
    uint64_t getScheduleTime() const {
        return schedule_time_;
    }
    
    uint64_t getCurrentSlot() const {
        return current_slot_.load();
    }
    
    void setCycleTime(uint64_t cycle_time) {
        cycle_time_.store(cycle_time);
        // 自动调整时间槽持续时间
        slot_duration_.store(cycle_time / max_slots_.load());
    }
    
    void setMaxSlots(uint32_t max_slots) {
        max_slots_.store(max_slots);
        slot_duration_.store(cycle_time_.load() / max_slots);
    }
    
    void setSlotDuration(uint64_t duration) {
        slot_duration_.store(duration);
        cycle_time_.store(duration * max_slots_.load());
    }
    
    bool canTransmit(uint64_t priority) const {
        if (priority >= 8) priority = 7;
        
        // 高优先级流量可以在任何时间槽传输
        if (priority >= 6) {
            return true;
        }
        
        // 中优先级流量可以在多个时间槽传输
        if (priority >= 4) {
            uint64_t slot = current_slot_.load();
            return (slot % 2) == 0;
        }
        
        // 低优先级流量只能在特定时间槽传输
        uint64_t slot = current_slot_.load();
        uint32_t mask = priority_map_[priority].load();
        return (mask & (1 << (slot % 8))) != 0;
    }
    
    void setPriorityMap(uint32_t priority, uint32_t slot_mask) {
        if (priority < 8) {
            priority_map_[priority].store(slot_mask);
        }
    }
    
    uint32_t getPriorityMap(uint32_t priority) const {
        if (priority < 8) {
            return priority_map_[priority].load();
        }
        return 0;
    }
};

// TSNTransport implementation

TSNTransport::TSNTransport(const std::string& interface, int port, int priority) 
    : interface_(interface), port_(port), priority_(priority), sockfd_(-1), 
      available_(false), timeSyncEnabled_(false), scheduleEnabled_(false),
      tx_queue_size_(1024), rx_queue_size_(1024),
      time_sync_manager_(std::make_unique<TimeSyncManager>()),
      traffic_scheduler_(std::make_unique<TrafficScheduler>()),
      total_bytes_sent_(0), total_bytes_received_(0),
      total_transactions_(0), failure_count_(0),
      total_latency_(0) {
}

void TSNTransport::init() {
    // 初始化Winsock
    #if defined(_WIN32)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        AURORA_LOG_ERROR("WSAStartup failed");
        status_ = TransportStatus::ERROR;
        return;
    }
    #endif
    
    // 创建套接字
    sockfd_ = platform::PlatformManager::instance().getPlatform()->createSocket(
        AF_INET, SOCK_DGRAM, 0
    );
    
    if (sockfd_ != -1) {
        // 设置非阻塞和地址重用
        platform::PlatformManager::instance().getPlatform()->setSocketNonBlocking(sockfd_);
        platform::PlatformManager::instance().getPlatform()->setSocketReuseAddr(sockfd_);
        
        // 优化套接字选项以提高实时性能
        #if defined(__linux__)
        // 设置缓冲区大小
        int opt = rx_queue_size_;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
        opt = tx_queue_size_;
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt));
        
        // 设置发送超时
        opt = 100; // 100ms超时
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &opt, sizeof(opt));
        
        // 设置接收超时
        opt = 100; // 100ms超时
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &opt, sizeof(opt));
        #endif
        
        // 设置TSN相关参数
        // 1. 设置流量类（优先级）
        #if defined(__linux__)
        int tos = priority_ << 5; // 将优先级映射到TOS字段
        if (setsockopt(sockfd_, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
            AURORA_LOG_WARN("Failed to set IP_TOS: {}", strerror(errno));
        }
        
        // 2. 设置DSCP值（Differentiated Services Code Point）
        int dscp = priority_ << 2; // 将优先级映射到DSCP值
        if (setsockopt(sockfd_, IPPROTO_IP, IP_TOS, &dscp, sizeof(dscp)) < 0) {
            AURORA_LOG_WARN("Failed to set DSCP: {}", strerror(errno));
        }
        #endif
        
        // 3. 绑定地址
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        addr.sin_addr.s_addr = INADDR_ANY; // 绑定到所有接口
        
        if (bind(sockfd_, (sockaddr*)&addr, sizeof(addr)) == 0) {
            available_ = true;
            status_ = TransportStatus::INITIALIZED;
            AURORA_LOG_INFO("TSNTransport initialized with interface: {}, port: {}, priority: {}, queue size: {}/{}", 
                           interface_, port_, priority_, tx_queue_size_, rx_queue_size_);
        } else {
            #if defined(_WIN32)
            int error = WSAGetLastError();
            AURORA_LOG_ERROR("Failed to bind socket: {}", error);
            #else
            AURORA_LOG_ERROR("Failed to bind socket: {}", strerror(errno));
            #endif
            status_ = TransportStatus::ERROR;
        }
    } else {
        AURORA_LOG_ERROR("Failed to create socket");
        status_ = TransportStatus::ERROR;
    }
}

void TSNTransport::start() {
    if (status_ == TransportStatus::INITIALIZED && available_) {
        // 启动时间同步（如果启用）
        if (timeSyncEnabled_) {
            AURORA_LOG_INFO("Starting TSN time synchronization");
            time_sync_manager_->setEnabled(true);
        }
        
        // 启动调度（如果启用）
        if (scheduleEnabled_) {
            AURORA_LOG_INFO("Starting TSN traffic scheduling");
            traffic_scheduler_->setEnabled(true);
        }
        
        status_ = TransportStatus::RUNNING;
        AURORA_LOG_INFO("TSNTransport started");
    } else {
        AURORA_LOG_ERROR("TSNTransport cannot start in current status: {}, available: {}", 
                       static_cast<int>(status_), available_);
    }
}

void TSNTransport::stop() {
    // 停止时间同步和流量调度
    time_sync_manager_->setEnabled(false);
    traffic_scheduler_->setEnabled(false);
    
    if (sockfd_ != -1) {
        platform::PlatformManager::instance().getPlatform()->closeSocket(sockfd_);
        sockfd_ = -1;
        available_ = false;
    }
    
    #if defined(_WIN32)
    WSACleanup();
    #endif
    
    status_ = TransportStatus::STOPPED;
    AURORA_LOG_INFO("TSNTransport stopped");
}

bool TSNTransport::send(const void* data, size_t size) {
    if (status_ != TransportStatus::RUNNING || !available_) {
        AURORA_LOG_ERROR("TSNTransport not available");
        failure_count_++;
        return false;
    }
    
    // 检查时间同步状态
    if (timeSyncEnabled_ && !time_sync_manager_->isSynchronized()) {
        AURORA_LOG_WARN("TSNTransport time not synchronized, delaying send");
        // 等待时间同步完成
        int retry = 0;
        while (!time_sync_manager_->isSynchronized() && retry < 10) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            retry++;
        }
        if (!time_sync_manager_->isSynchronized()) {
            AURORA_LOG_ERROR("TSNTransport time synchronization failed");
            failure_count_++;
            return false;
        }
    }
    
    // 检查调度时间（如果启用了调度）
    if (scheduleEnabled_) {
        // 根据优先级和时间槽决定是否发送
        int retry = 0;
        while (!traffic_scheduler_->canTransmit(priority_) && retry < 100) {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            retry++;
        }
        if (!traffic_scheduler_->canTransmit(priority_)) {
            AURORA_LOG_WARN("TSNTransport scheduling failed, forcing send");
        }
    }
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // 本地测试
    
    // 记录发送时间
    uint64_t send_time = time_sync_manager_->getCurrentTime();
    
    // 实现数据分片传输
    const size_t MAX_PACKET_SIZE = tx_queue_size_;
    const char* data_ptr = static_cast<const char*>(data);
    size_t remaining = size;
    
    // 发送头部信息：总大小
    uint32_t total_size = htonl(static_cast<uint32_t>(size));
    int sent = sendto(
        sockfd_,
        &total_size,
        sizeof(total_size),
        0,
        (sockaddr*)&addr,
        sizeof(addr)
    );
    if (sent != static_cast<int>(sizeof(total_size))) {
        #if defined(_WIN32)
        int error = WSAGetLastError();
        AURORA_LOG_ERROR("TSNTransport send header error: {}, sent: {}, expected: {}", 
                       error, sent, sizeof(total_size));
        #else
        AURORA_LOG_ERROR("TSNTransport send header error: {}, sent: {}, expected: {}", 
                       strerror(errno), sent, sizeof(total_size));
        #endif
        failure_count_++;
        return false;
    }
    
    // 分片段发送数据
    while (remaining > 0) {
        size_t chunk_size = std::min(remaining, MAX_PACKET_SIZE);
        
        // 优化：使用循环发送，直到所有数据发送完成
        size_t bytes_sent = 0;
        while (bytes_sent < chunk_size) {
            sent = sendto(
                sockfd_,
                data_ptr + bytes_sent,
                chunk_size - bytes_sent,
                0,
                (sockaddr*)&addr,
                sizeof(addr)
            );
            
            if (sent > 0) {
                bytes_sent += sent;
            } else {
                #if defined(_WIN32)
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    // 非阻塞模式下发送缓冲区满，等待一段时间
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    continue;
                }
                AURORA_LOG_ERROR("TSNTransport send chunk error: {}, sent: {}, expected: {}", 
                               error, sent, chunk_size);
                #else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 非阻塞模式下发送缓冲区满，等待一段时间
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    continue;
                }
                AURORA_LOG_ERROR("TSNTransport send chunk error: {}, sent: {}, expected: {}", 
                               strerror(errno), sent, chunk_size);
                #endif
                failure_count_++;
                return false;
            }
        }
        
        data_ptr += chunk_size;
        remaining -= chunk_size;
    }
    
    uint64_t end_time = time_sync_manager_->getCurrentTime();
    uint64_t latency = end_time - send_time;
    
    // 更新统计信息
    total_bytes_sent_ += size;
    total_transactions_++;
    total_latency_ += latency;
    
    AURORA_LOG_DEBUG("TSNTransport sent {} bytes with priority {}, time: {}, duration: {}ns, slot: {}, sync quality: {}", 
                   size, priority_, send_time, latency, 
                   traffic_scheduler_->getCurrentSlot(), time_sync_manager_->getSyncQuality());
    return true;
}

bool TSNTransport::receive(void* data, size_t size) {
    if (status_ != TransportStatus::RUNNING || !available_) {
        AURORA_LOG_ERROR("TSNTransport not available");
        failure_count_++;
        return false;
    }
    
    sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    
    // 记录接收开始时间
    uint64_t start_time = time_sync_manager_->getCurrentTime();
    
    // 接收头部信息：总大小
    uint32_t total_size;
    int received = recvfrom(
        sockfd_,
        &total_size,
        sizeof(total_size),
        0,
        (sockaddr*)&addr,
        &addrlen
    );
    
    if (received != static_cast<int>(sizeof(total_size))) {
        #if defined(_WIN32)
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            // 非阻塞模式下没有数据，不是错误
            return false;
        }
        AURORA_LOG_ERROR("TSNTransport receive header error: {}", error);
        #else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 非阻塞模式下没有数据，不是错误
            return false;
        }
        AURORA_LOG_ERROR("TSNTransport receive header error: {}", strerror(errno));
        #endif
        failure_count_++;
        return false;
    }
    
    // 转换为本地字节序
    total_size = ntohl(total_size);
    
    // 检查缓冲区大小
    if (size < total_size) {
        AURORA_LOG_ERROR("Receive buffer size ({}) is smaller than expected data size ({})", size, total_size);
        failure_count_++;
        return false;
    }
    
    // 接收数据
    char* data_ptr = static_cast<char*>(data);
    size_t remaining = total_size;
    
    while (remaining > 0) {
        size_t chunk_size = std::min(remaining, rx_queue_size_);
        
        // 优化：使用循环接收，直到所有数据接收完成
        size_t bytes_received = 0;
        while (bytes_received < chunk_size) {
            received = recvfrom(
                sockfd_,
                data_ptr + bytes_received,
                chunk_size - bytes_received,
                0,
                (sockaddr*)&addr,
                &addrlen
            );
            
            if (received > 0) {
                bytes_received += received;
            } else if (received == 0) {
                AURORA_LOG_WARN("TSNTransport connection closed during receive");
                failure_count_++;
                return false;
            } else {
                #if defined(_WIN32)
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    // 非阻塞模式下没有数据，等待一段时间
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    continue;
                }
                AURORA_LOG_ERROR("TSNTransport receive chunk error: {}", error);
                #else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 非阻塞模式下没有数据，等待一段时间
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    continue;
                }
                AURORA_LOG_ERROR("TSNTransport receive chunk error: {}", strerror(errno));
                #endif
                failure_count_++;
                return false;
            }
        }
        
        data_ptr += chunk_size;
        remaining -= chunk_size;
    }
    
    uint64_t end_time = time_sync_manager_->getCurrentTime();
    uint64_t latency = end_time - start_time;
    
    // 更新统计信息
    total_bytes_received_ += total_size;
    total_latency_ += latency;
    
    char clientAddr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), clientAddr, INET_ADDRSTRLEN);
    AURORA_LOG_DEBUG("TSNTransport received {} bytes from {}:{}, time: {}, duration: {}ns, slot: {}, sync quality: {}", 
                   total_size, clientAddr, ntohs(addr.sin_port), 
                   start_time, latency, 
                   traffic_scheduler_->getCurrentSlot(), time_sync_manager_->getSyncQuality());
    return true;
}

bool TSNTransport::isAvailable() const {
    return available_ && status_ == TransportStatus::RUNNING;
}

TransportStatus TSNTransport::getStatus() const {
    return status_;
}

bool TSNTransport::setTrafficClass(int priority) {
    if (priority < 0 || priority > 7) {
        AURORA_LOG_ERROR("Invalid priority: {}. Must be between 0-7", priority);
        return false;
    }
    
    priority_ = priority;
    
    if (sockfd_ != -1) {
        #if defined(__linux__)
        int tos = priority_ << 5; // 将优先级映射到TOS字段
        if (setsockopt(sockfd_, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
            AURORA_LOG_ERROR("Failed to set IP_TOS: {}", strerror(errno));
            return false;
        }
        #endif
    }
    
    AURORA_LOG_INFO("TSNTransport traffic class set to: {}", priority_);
    return true;
}

bool TSNTransport::setTimeSyncEnabled(bool enabled) {
    timeSyncEnabled_ = enabled;
    time_sync_manager_->setEnabled(enabled);
    AURORA_LOG_INFO("TSNTransport time synchronization {}", enabled ? "enabled" : "disabled");
    return true;
}

bool TSNTransport::setScheduleEnabled(bool enabled) {
    scheduleEnabled_ = enabled;
    traffic_scheduler_->setEnabled(enabled);
    AURORA_LOG_INFO("TSNTransport traffic scheduling {}", enabled ? "enabled" : "disabled");
    return true;
}

bool TSNTransport::setQueueSize(size_t tx_size, size_t rx_size) {
    tx_queue_size_ = tx_size;
    rx_queue_size_ = rx_size;
    
    if (sockfd_ != -1) {
        #if defined(__linux__)
        int opt = rx_queue_size_;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
        opt = tx_queue_size_;
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt));
        #endif
    }
    
    AURORA_LOG_INFO("TSNTransport queue size set to: {}/{} (tx/rx)", tx_queue_size_, rx_queue_size_);
    return true;
}

uint64_t TSNTransport::getCurrentTime() const {
    return time_sync_manager_->getCurrentTime();
}

void TSNTransport::printStats() const {
    AURORA_LOG_INFO("TSNTransport stats:");
    AURORA_LOG_INFO("  Interface: {}", interface_);
    AURORA_LOG_INFO("  Port: {}", port_);
    AURORA_LOG_INFO("  Priority: {}", priority_);
    AURORA_LOG_INFO("  Total bytes sent: {}", total_bytes_sent_);
    AURORA_LOG_INFO("  Total bytes received: {}", total_bytes_received_);
    AURORA_LOG_INFO("  Total transactions: {}", total_transactions_);
    AURORA_LOG_INFO("  Failure count: {}", failure_count_);
    AURORA_LOG_INFO("  Queue size: {}/{}", tx_queue_size_, rx_queue_size_);
    AURORA_LOG_INFO("  Time sync enabled: {}", timeSyncEnabled_);
    AURORA_LOG_INFO("  Schedule enabled: {}", scheduleEnabled_);
    AURORA_LOG_INFO("  Sync quality: {}", time_sync_manager_->getSyncQuality());
    AURORA_LOG_INFO("  Last sync time: {}", time_sync_manager_->getLastSyncTime());
    if (total_transactions_ > 0) {
        double avg_latency = static_cast<double>(total_latency_) / total_transactions_;
        AURORA_LOG_INFO("  Average latency: {:.2f}ns", avg_latency);
        double avg_size = static_cast<double>(total_bytes_sent_) / total_transactions_;
        AURORA_LOG_INFO("  Average transaction size: {:.2f} bytes", avg_size);
    }
}

// 传输性能指标结构
struct TransportPerformance {
    std::atomic<uint64_t> total_bytes_sent;
    std::atomic<uint64_t> total_bytes_received;
    std::atomic<uint64_t> total_transactions;
    std::atomic<uint64_t> total_latency;
    std::atomic<uint64_t> last_used_time;
    std::atomic<int> failure_count;
    
    TransportPerformance() 
        : total_bytes_sent(0), total_bytes_received(0), total_transactions(0),
          total_latency(0), last_used_time(0), failure_count(0) {}
    
    double getAverageLatency() const {
        uint64_t transactions = total_transactions.load();
        return transactions > 0 ? static_cast<double>(total_latency.load()) / transactions : 0.0;
    }
    
    double getThroughput() const {
        uint64_t time_diff = std::chrono::system_clock::now().time_since_epoch().count() - last_used_time.load();
        if (time_diff == 0) return 0.0;
        return static_cast<double>(total_bytes_sent.load() + total_bytes_received.load()) / time_diff * 1e9;
    }
    
    void updateStats(size_t bytes, uint64_t latency, bool success) {
        total_bytes_sent.fetch_add(bytes);
        total_transactions.fetch_add(1);
        total_latency.fetch_add(latency);
        last_used_time.store(std::chrono::system_clock::now().time_since_epoch().count());
        if (!success) {
            failure_count.fetch_add(1);
        }
    }
};

// TransportManager implementation

TransportManager::TransportManager() {
    // 初始化性能指标
    performance_stats_[TransportType::INTRA_PROCESS] = std::make_unique<TransportPerformance>();
    performance_stats_[TransportType::SHARED_MEMORY] = std::make_unique<TransportPerformance>();
    performance_stats_[TransportType::NETWORK] = std::make_unique<TransportPerformance>();
    performance_stats_[TransportType::TSN] = std::make_unique<TransportPerformance>();
}

TransportManager& TransportManager::instance() {
    static TransportManager instance;
    return instance;
}

void TransportManager::init() {
    // 初始化各种传输类型
    transports_[TransportType::INTRA_PROCESS] = std::make_shared<IntraProcessTransport>();
    transports_[TransportType::SHARED_MEMORY] = std::make_shared<SharedMemoryTransport>("aurorart_shm", 4096);
    transports_[TransportType::NETWORK] = std::make_shared<NetworkTransport>("127.0.0.1", 5555);
    transports_[TransportType::TSN] = std::make_shared<TSNTransport>("eth0", 5556, 7);
    
    // 初始化所有传输
    for (auto& [type, transport] : transports_) {
        transport->init();
    }
    
    AURORA_LOG_INFO("TransportManager initialized with {} transport types", transports_.size());
}

void TransportManager::start() {
    // 启动所有传输
    for (auto& [type, transport] : transports_) {
        transport->start();
    }
    
    AURORA_LOG_INFO("TransportManager started");
}

void TransportManager::stop() {
    // 停止所有传输
    for (auto& [type, transport] : transports_) {
        transport->stop();
    }
    
    AURORA_LOG_INFO("TransportManager stopped");
}

std::shared_ptr<Transport> TransportManager::getTransport(TransportType type) {
    auto it = transports_.find(type);
    if (it != transports_.end()) {
        return it->second;
    }
    AURORA_LOG_WARN("Transport type not found: {}", static_cast<int>(type));
    return nullptr;
}

std::shared_ptr<Transport> TransportManager::selectOptimalTransport(size_t dataSize, bool realTime, const std::string& destination) {
    // 网络状况评估
    NetworkStatus networkStatus = evaluateNetworkStatus();
    
    // 1. 首先检查是否是本地传输
    if (isLocalDestination(destination)) {
        // 本地传输优先使用共享内存或进程内传输
        if (dataSize < 1024) {
            // 小数据使用进程内传输
            auto intraTransport = getTransport(TransportType::INTRA_PROCESS);
            if (intraTransport && intraTransport->getStatus() == TransportStatus::RUNNING) {
                AURORA_LOG_DEBUG("Selected IntraProcessTransport for local small data: {} bytes", dataSize);
                return intraTransport;
            }
        } else if (dataSize < 1024 * 1024) {
            // 中等数据使用共享内存
            auto shmTransport = getTransport(TransportType::SHARED_MEMORY);
            if (shmTransport && shmTransport->getStatus() == TransportStatus::RUNNING) {
                AURORA_LOG_DEBUG("Selected SharedMemoryTransport for local medium data: {} bytes", dataSize);
                return shmTransport;
            }
        }
    }
    
    // 2. 实时性要求处理
    if (realTime) {
        // 实时性要求高，优先使用TSN传输
        auto tsnTransport = getTransport(TransportType::TSN);
        if (tsnTransport && tsnTransport->getStatus() == TransportStatus::RUNNING) {
            AURORA_LOG_DEBUG("Selected TSNTransport for real-time data: {} bytes", dataSize);
            return tsnTransport;
        }
        
        // TSN不可用，根据网络状况选择
        if (networkStatus == NetworkStatus::GOOD) {
            auto networkTransport = getTransport(TransportType::NETWORK);
            if (networkTransport && networkTransport->getStatus() == TransportStatus::RUNNING) {
                AURORA_LOG_DEBUG("Selected NetworkTransport for real-time data with good network: {} bytes", dataSize);
                return networkTransport;
            }
        }
    }
    
    // 3. 非实时数据根据数据大小、网络状况和历史性能选择
    if (dataSize < 1024 * 1024) {
        // 中等数据量
        if (networkStatus == NetworkStatus::POOR) {
            // 网络状况差，尝试其他传输方式
            auto shmTransport = getTransport(TransportType::SHARED_MEMORY);
            if (shmTransport && shmTransport->getStatus() == TransportStatus::RUNNING) {
                AURORA_LOG_DEBUG("Selected SharedMemoryTransport for medium data with poor network: {} bytes", dataSize);
                return shmTransport;
            }
        } else {
            // 网络状况良好，根据历史性能选择
            return selectBasedOnPerformance(dataSize, realTime);
        }
    } else {
        // 大数据量，使用网络传输
        auto networkTransport = getTransport(TransportType::NETWORK);
        if (networkTransport && networkTransport->getStatus() == TransportStatus::RUNNING) {
            AURORA_LOG_DEBUG("Selected NetworkTransport for large data: {} bytes", dataSize);
            return networkTransport;
        }
    }
    
    // 4. 基于性能的 fallback 机制
    return selectBasedOnPerformance(dataSize, realTime);
}

std::shared_ptr<Transport> TransportManager::selectBasedOnPerformance(size_t dataSize, bool realTime) {
    // 基于历史性能选择最优传输
    TransportType best_type = TransportType::INTRA_PROCESS;
    double best_score = -1.0;
    
    for (const auto& [type, transport] : transports_) {
        if (!transport || transport->getStatus() != TransportStatus::RUNNING) {
            continue;
        }
        
        auto it = performance_stats_.find(type);
        if (it == performance_stats_.end()) {
            continue;
        }
        
        auto& stats = *it->second;
        double latency = stats.getAverageLatency();
        double throughput = stats.getThroughput();
        int failures = stats.failure_count.load();
        
        // 计算综合评分
        double score = 0.0;
        if (realTime) {
            // 实时数据优先考虑延迟
            score = (1.0 / (latency + 1.0)) * 0.7 + throughput * 0.3;
        } else {
            // 非实时数据优先考虑吞吐量
            score = throughput * 0.7 + (1.0 / (latency + 1.0)) * 0.3;
        }
        
        // 失败率惩罚
        score *= (1.0 - static_cast<double>(failures) / (stats.total_transactions.load() + 1.0));
        
        if (score > best_score) {
            best_score = score;
            best_type = type;
        }
    }
    
    auto best_transport = getTransport(best_type);
    if (best_transport && best_transport->getStatus() == TransportStatus::RUNNING) {
        AURORA_LOG_DEBUG("Selected transport {} based on performance for data size: {} bytes, real-time: {}", 
                       static_cast<int>(best_type), dataSize, realTime);
        return best_transport;
    }
    
    // 所有传输都不可用，返回nullptr
    AURORA_LOG_ERROR("No available transport found for data size: {}, real-time: {}", dataSize, realTime);
    return nullptr;
}

TransportManager::NetworkStatus TransportManager::evaluateNetworkStatus() {
    // 改进的网络状况评估
    // 实际应用中应该通过网络监控获取真实的网络状况
    static int counter = 0;
    counter++;
    
    // 模拟网络状况，实际应用中应该使用真实的网络监控数据
    if (counter % 5 == 0) {
        return NetworkStatus::POOR;
    } else if (counter % 5 == 1 || counter % 5 == 2) {
        return NetworkStatus::FAIR;
    } else {
        return NetworkStatus::GOOD;
    }
}

bool TransportManager::isLocalDestination(const std::string& destination) {
    // 改进的本地目标判断
    // 实际应用中应该根据IP地址或主机名判断
    if (destination.empty() || destination == "localhost" || destination == "127.0.0.1") {
        return true;
    }
    
    // 检查是否是本地IPv4地址
    if (destination.find("192.168.") == 0 || destination.find("10.") == 0 || 
        destination.find("172.16.") == 0 || destination.find("172.31.") == 0) {
        return true;
    }
    
    return false;
}

void TransportManager::updateTransportStats(TransportType type, size_t bytes, uint64_t latency, bool success) {
    auto it = performance_stats_.find(type);
    if (it != performance_stats_.end()) {
        it->second->updateStats(bytes, latency, success);
    }
}

void TransportManager::printPerformanceStats() {
    AURORA_LOG_INFO("Transport performance statistics:");
    for (const auto& [type, stats] : performance_stats_) {
        AURORA_LOG_INFO("Transport {}: avg latency={}ns, throughput={}B/s, failures={}", 
                       static_cast<int>(type), 
                       stats->getAverageLatency(), 
                       stats->getThroughput(), 
                       stats->failure_count.load());
    }
}

void TransportManager::registerTransport(TransportType type, std::shared_ptr<Transport> transport) {
    transports_[type] = transport;
    AURORA_LOG_INFO("Registered transport type: {}", static_cast<int>(type));
}

void TransportManager::unregisterTransport(TransportType type) {
    auto it = transports_.find(type);
    if (it != transports_.end()) {
        it->second->stop();
        transports_.erase(it);
        AURORA_LOG_INFO("Unregistered transport type: {}", static_cast<int>(type));
    }
}

std::shared_ptr<Transport> TransportManager::getDecoratedTransport(TransportType type, bool enableLogging, bool enableCompression, bool enableEncryption) {
    // 生成装饰器组合的唯一键
    std::string key = std::to_string(static_cast<int>(type)) + "_" + 
                     (enableLogging ? "1" : "0") + "_" + 
                     (enableCompression ? "1" : "0") + "_" + 
                     (enableEncryption ? "1" : "0");
    
    // 检查是否已经创建过相同配置的装饰器
    auto it = decoratedTransports_.find(key);
    if (it != decoratedTransports_.end()) {
        return it->second;
    }
    
    // 获取基础传输
    auto transport = getTransport(type);
    if (!transport) {
        AURORA_LOG_ERROR("Failed to get base transport for decoration");
        return nullptr;
    }
    
    // 应用装饰器
    std::shared_ptr<Transport> decoratedTransport = transport;
    
    if (enableLogging) {
        decoratedTransport = std::make_shared<LoggingTransportDecorator>(decoratedTransport);
    }
    
    if (enableCompression) {
        decoratedTransport = std::make_shared<CompressionTransportDecorator>(decoratedTransport);
    }
    
    if (enableEncryption) {
        decoratedTransport = std::make_shared<EncryptionTransportDecorator>(decoratedTransport);
    }
    
    // 缓存装饰后的传输
    decoratedTransports_[key] = decoratedTransport;
    
    AURORA_LOG_INFO("Created decorated transport with logging: {}, compression: {}, encryption: {}", 
                   enableLogging, enableCompression, enableEncryption);
    
    return decoratedTransport;
}

// 装饰器性能统计结构
struct DecoratorPerformance {
    std::atomic<uint64_t> total_calls;
    std::atomic<uint64_t> total_latency;
    std::atomic<uint64_t> total_bytes_processed;
    
    DecoratorPerformance() 
        : total_calls(0), total_latency(0), total_bytes_processed(0) {}
    
    void update(uint64_t latency, size_t bytes) {
        total_calls.fetch_add(1);
        total_latency.fetch_add(latency);
        total_bytes_processed.fetch_add(bytes);
    }
    
    double getAverageLatency() const {
        uint64_t calls = total_calls.load();
        return calls > 0 ? static_cast<double>(total_latency.load()) / calls : 0.0;
    }
};

// 装饰器基类优化
class TransportDecorator : public Transport {
public:
    TransportDecorator(std::shared_ptr<Transport> transport)
        : transport_(std::move(transport)) {}
    
    void init() override { transport_->init(); }
    void start() override { transport_->start(); }
    void stop() override { transport_->stop(); }
    TransportStatus getStatus() const override { return transport_->getStatus(); }
    TransportType getType() const override { return transport_->getType(); }
    
protected:
    std::shared_ptr<Transport> transport_;
};

// LoggingTransportDecorator implementation

bool LoggingTransportDecorator::send(const void* data, size_t size) {
    // 只在调试模式下输出日志
    #ifdef DEBUG
    AURORA_LOG_DEBUG("LoggingTransportDecorator sending {} bytes", size);
    #endif
    
    auto start = std::chrono::high_resolution_clock::now();
    bool result = transport_->send(data, size);
    auto end = std::chrono::high_resolution_clock::now();
    
    // 只在调试模式下输出日志
    #ifdef DEBUG
    AURORA_LOG_DEBUG("LoggingTransportDecorator send result: {}", result);
    #endif
    
    return result;
}

bool LoggingTransportDecorator::receive(void* data, size_t size) {
    // 只在调试模式下输出日志
    #ifdef DEBUG
    AURORA_LOG_DEBUG("LoggingTransportDecorator receiving {} bytes", size);
    #endif
    
    auto start = std::chrono::high_resolution_clock::now();
    bool result = transport_->receive(data, size);
    auto end = std::chrono::high_resolution_clock::now();
    
    // 只在调试模式下输出日志
    #ifdef DEBUG
    AURORA_LOG_DEBUG("LoggingTransportDecorator receive result: {}", result);
    #endif
    
    return result;
}

// CompressionTransportDecorator implementation

bool CompressionTransportDecorator::send(const void* data, size_t size) {
    // 只在调试模式下输出日志
    #ifdef DEBUG
    AURORA_LOG_DEBUG("CompressionTransportDecorator compressing {} bytes", size);
    #endif
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 这里实现简单的压缩逻辑
    // 实际应用中应该使用更高效的压缩算法
    // 模拟压缩（实际应用中应该使用zlib等库）
    bool result = transport_->send(data, size);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    // 只在调试模式下输出日志
    #ifdef DEBUG
    AURORA_LOG_DEBUG("CompressionTransportDecorator send result: {}", result);
    #endif
    
    return result;
}

bool CompressionTransportDecorator::receive(void* data, size_t size) {
    // 只在调试模式下输出日志
    #ifdef DEBUG
    AURORA_LOG_DEBUG("CompressionTransportDecorator decompressing {} bytes", size);
    #endif
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 这里实现简单的解压缩逻辑
    bool result = transport_->receive(data, size);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    // 只在调试模式下输出日志
    #ifdef DEBUG
    AURORA_LOG_DEBUG("CompressionTransportDecorator receive result: {}", result);
    #endif
    
    return result;
}

// EncryptionTransportDecorator implementation

bool EncryptionTransportDecorator::send(const void* data, size_t size) {
    // 只在调试模式下输出日志
    #ifdef DEBUG
    AURORA_LOG_DEBUG("EncryptionTransportDecorator encrypting {} bytes", size);
    #endif
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 这里实现简单的加密逻辑
    // 实际应用中应该使用更安全的加密算法
    // 模拟加密（实际应用中应该使用AES等库）
    bool result = transport_->send(data, size);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    // 只在调试模式下输出日志
    #ifdef DEBUG
    AURORA_LOG_DEBUG("EncryptionTransportDecorator send result: {}", result);
    #endif
    
    return result;
}

bool EncryptionTransportDecorator::receive(void* data, size_t size) {
    // 只在调试模式下输出日志
    #ifdef DEBUG
    AURORA_LOG_DEBUG("EncryptionTransportDecorator decrypting {} bytes", size);
    #endif
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 这里实现简单的解密逻辑
    bool result = transport_->receive(data, size);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    // 只在调试模式下输出日志
    #ifdef DEBUG
    AURORA_LOG_DEBUG("EncryptionTransportDecorator receive result: {}", result);
    #endif
    
    return result;
}

} // namespace transport
} // namespace aurorart