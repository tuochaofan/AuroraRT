#ifndef AURORART_FIXED_SIZE_CONTAINER_H
#define AURORART_FIXED_SIZE_CONTAINER_H

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>

namespace aurorart {
namespace memory {

/**
 * @class FixedSizeContainer
 * @brief 固定大小容器，用于零拷贝传输
 * 
 * 基于iceoryx2的设计理念，提供固定大小的内存容器，
 * 支持零拷贝传输和高效的内存管理。
 */
template <typename T>
class FixedSizeContainer {
public:
    /**
     * @brief 构造函数
     * @param size 容器大小
     */
    explicit FixedSizeContainer(size_t size);
    
    /**
     * @brief 析构函数
     */
    ~FixedSizeContainer();
    
    /**
     * @brief 分配元素
     * @return 分配的元素指针
     */
    T* allocate();
    
    /**
     * @brief 释放元素
     * @param ptr 要释放的元素指针
     */
    void deallocate(T* ptr);
    
    /**
     * @brief 获取容器大小
     * @return 容器大小
     */
    size_t size() const;
    
    /**
     * @brief 获取已使用的元素数量
     * @return 已使用的元素数量
     */
    size_t used() const;
    
    /**
     * @brief 获取空闲的元素数量
     * @return 空闲的元素数量
     */
    size_t free() const;
    
    /**
     * @brief 检查容器是否为空
     * @return 是否为空
     */
    bool empty() const;
    
    /**
     * @brief 检查容器是否已满
     * @return 是否已满
     */
    bool full() const;
    
    /**
     * @brief 检查指针是否在容器内
     * @param ptr 要检查的指针
     * @return 是否在容器内
     */
    bool contains(const T* ptr) const;
    
    /**
     * @brief 打印容器状态
     */
    void printStats() const;
    
private:
    /**
     * @brief 元素控制块
     */
    struct ElementControlBlock {
        std::atomic<bool> is_free; // 是否空闲
        ElementControlBlock* next; // 指向下