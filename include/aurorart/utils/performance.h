#ifndef AURORART_PERFORMANCE_H
#define AURORART_PERFORMANCE_H

#include <cstddef>

namespace aurorart {
namespace utils {

class PerformanceUtils {
public:
    // 内存拷贝优化
    static void memcpy_optimized(void* dst, const void* src, size_t size);
    
    // 内存设置优化
    static void memset_optimized(void* dst, int value, size_t size);
    
    // 内存比较优化
    static int memcmp_optimized(const void* ptr1, const void* ptr2, size_t size);
    
    // 快速哈希计算
    static uint32_t hash_optimized(const void* data, size_t size);
    
    // 快速整数除法（除以2的幂）
    static inline uint32_t divide_by_power_of_two(uint32_t value, uint32_t exponent) {
        return value >> exponent;
    }
    
    // 快速整数乘法（乘以2的幂）
    static inline uint32_t multiply_by_power_of_two(uint32_t value, uint32_t exponent) {
        return value << exponent;
    }
    
    // 快速取模运算（对2的幂取模）
    static inline uint32_t mod_power_of_two(uint32_t value, uint32_t modulus) {
        return value & (modulus - 1);
    }
    
    // 快速原子操作
    static void atomic_inc(volatile int* value);
    static void atomic_dec(volatile int* value);
    
    // 快速无锁队列操作
    static bool cas(volatile void** ptr, void* old_val, void* new_val);
};

} // namespace utils
} // namespace aurorart

#endif // AURORART_PERFORMANCE_H