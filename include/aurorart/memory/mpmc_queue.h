#ifndef AURORART_MPMC_QUEUE_H
#define AURORART_MPMC_QUEUE_H

#include <atomic>
#include <vector>

namespace aurorart {
namespace memory {

/**
 * @brief 多生产者多消费者无锁队列
 * 参考 Iceoryx2 的无锁设计，使用原子操作实现无锁队列
 * 支持批量操作，提高并发性能
 */
template <typename T, size_t Capacity>
class MPMCQueue {
private:
    // 缓存行对齐，避免伪共享
    alignas(64) T buffer[Capacity];
    alignas(64) std::atomic<size_t> head;
    alignas(64) std::atomic<size_t> tail;

public:
    MPMCQueue() : head(0), tail(0) {}

    /**
     * @brief 入队操作
     * @param item 要入队的元素
     * @return 是否入队成功
     */
    bool push(const T& item) {
        size_t currentTail = tail.load(std::memory_order_relaxed);
        size_t nextTail = (currentTail + 1) % Capacity;

        // 检查队列是否已满
        if (nextTail == head.load(std::memory_order_acquire)) {
            return false;
        }

        // 写入数据
        buffer[currentTail] = item;

        // 发布新数据
        tail.store(nextTail, std::memory_order_release);
        return true;
    }

    /**
     * @brief 批量入队操作
     * 参考 ZMQ 的批量处理，减少原子操作次数
     * @param items 要入队的元素向量
     * @return 成功入队的元素数量
     */
    size_t pushBatch(const std::vector<T>& items) {
        size_t count = 0;
        for (const auto& item : items) {
            if (!push(item)) {
                break;
            }
            count++;
        }
        return count;
    }

    /**
     * @brief 出队操作
     * @param item 用于存储出队元素的引用
     * @return 是否出队成功
     */
    bool pop(T& item) {
        size_t currentHead = head.load(std::memory_order_relaxed);

        // 检查队列是否为空
        if (currentHead == tail.load(std::memory_order_acquire)) {
            return false;
        }

        // 读取数据
        item = buffer[currentHead];

        // 发布消费完成
        head.store((currentHead + 1) % Capacity, std::memory_order_release);
        return true;
    }

    /**
     * @brief 批量出队操作
     * 参考 ZMQ 的批量处理，减少原子操作次数
     * @param items 用于存储出队元素的向量
     * @param maxCount 最大出队数量
     * @return 成功出队的元素数量
     */
    size_t popBatch(std::vector<T>& items, size_t maxCount) {
        size_t count = 0;
        T item;
        while (count < maxCount && pop(item)) {
            items.push_back(item);
            count++;
        }
        return count;
    }

    /**
     * @brief 检查队列是否为空
     * @return 是否为空
     */
    bool isEmpty() const {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }

    /**
     * @brief 检查队列是否已满
     * @return 是否已满
     */
    bool isFull() const {
        size_t currentTail = tail.load(std::memory_order_acquire);
        size_t nextTail = (currentTail + 1) % Capacity;
        return nextTail == head.load(std::memory_order_acquire);
    }

    /**
     * @brief 获取队列容量
     * @return 队列容量
     */
    size_t capacity() const {
        return Capacity;
    }

    /**
     * @brief 获取队列当前元素数量
     * @return 元素数量
     */
    size_t size() const {
        size_t currentHead = head.load(std::memory_order_acquire);
        size_t currentTail = tail.load(std::memory_order_acquire);
        if (currentTail >= currentHead) {
            return currentTail - currentHead;
        } else {
            return Capacity - (currentHead - currentTail);
        }
    }
};

} // namespace memory
} // namespace aurorart

#endif // AURORART_MPMC_QUEUE_H
