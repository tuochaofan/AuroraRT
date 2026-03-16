#include "aurorart/utils/performance.h"

namespace aurorart {
namespace utils {

// 内存拷贝优化
void PerformanceUtils::memcpy_optimized(void* dst, const void* src, size_t size) {
    // 使用汇编优化的内存拷贝
    // 对于不同大小的数据使用不同的优化策略
    if (size == 0) {
        return;
    }
    
    // 对于小数据，使用普通memcpy
    if (size < 64) {
        memcpy(dst, src, size);
        return;
    }
    
    // 对于大数据，使用汇编优化
    #if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    // GCC x86/x86_64 汇编优化
    asm volatile (
        "cld\n"
        "mov %[size], %%rcx\n"
        "mov %[src], %%rsi\n"
        "mov %[dst], %%rdi\n"
        "rep movsb\n"
        :
        : [src] "r"(src), [dst] "r"(dst), [size] "r"(size)
        : "rcx", "rsi", "rdi", "memory"
    );
    #elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    // MSVC x86/x86_64 汇编优化
    __asm {
        mov ecx, size
        mov esi, src
        mov edi, dst
        rep movsb
    }
    #else
    // 其他平台使用标准memcpy
    memcpy(dst, src, size);
    #endif
}

// 内存设置优化
void PerformanceUtils::memset_optimized(void* dst, int value, size_t size) {
    // 使用汇编优化的内存设置
    if (size == 0) {
        return;
    }
    
    // 对于小数据，使用普通memset
    if (size < 64) {
        memset(dst, value, size);
        return;
    }
    
    // 对于大数据，使用汇编优化
    #if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    // GCC x86/x86_64 汇编优化
    asm volatile (
        "cld\n"
        "mov %[value], %%al\n"
        "mov %[size], %%rcx\n"
        "mov %[dst], %%rdi\n"
        "rep stosb\n"
        :
        : [dst] "r"(dst), [value] "r"(value), [size] "r"(size)
        : "rcx", "rdi", "al", "memory"
    );
    #elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    // MSVC x86/x86_64 汇编优化
    __asm {
        mov al, value
        mov ecx, size
        mov edi, dst
        rep stosb
    }
    #else
    // 其他平台使用标准memset
    memset(dst, value, size);
    #endif
}

// 内存比较优化
int PerformanceUtils::memcmp_optimized(const void* ptr1, const void* ptr2, size_t size) {
    // 使用汇编优化的内存比较
    if (size == 0) {
        return 0;
    }
    
    // 对于小数据，使用普通memcmp
    if (size < 64) {
        return memcmp(ptr1, ptr2, size);
    }
    
    // 对于大数据，使用汇编优化
    #if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    // GCC x86/x86_64 汇编优化
    int result;
    asm volatile (
        "cld\n"
        "mov %[size], %%rcx\n"
        "mov %[ptr1], %%rsi\n"
        "mov %[ptr2], %%rdi\n"
        "repe cmpsb\n"
        "setne %%al\n"
        "movzx %%al, %[result]\n"
        "jnz 1f\n"
        "xor %[result], %[result]\n"
        "1:\n"
        : [result] "=r"(result)
        : [ptr1] "r"(ptr1), [ptr2] "r"(ptr2), [size] "r"(size)
        : "rcx", "rsi", "rdi", "al", "memory"
    );
    return result;
    #elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    // MSVC x86/x86_64 汇编优化
    int result = 0;
    __asm {
        mov ecx, size
        mov esi, ptr1
        mov edi, ptr2
        repe cmpsb
        setne al
        movzx al, al
        mov result, eax
        jnz done
        xor result, result
        done:
    }
    return result;
    #else
    // 其他平台使用标准memcmp
    return memcmp(ptr1, ptr2, size);
    #endif
}

// 快速哈希计算
uint32_t PerformanceUtils::hash_optimized(const void* data, size_t size) {
    // 使用汇编优化的哈希计算
    // 实现一个简单的哈希函数，使用汇编优化
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t hash = 5381;
    
    #if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    // GCC x86/x86_64 汇编优化
    asm volatile (
        "xor %%eax, %%eax\n"
        "mov $5381, %%eax\n"
        "1:\n"
        "cmp $0, %[size]\n"
        "je 2f\n"
        "mov (%[bytes]), %%cl\n"
        "shl $5, %%eax\n"
        "add %%eax, %%eax\n"
        "xor %%cl, %%al\n"
        "inc %[bytes]\n"
        "dec %[size]\n"
        "jmp 1b\n"
        "2:\n"
        "mov %%eax, %[hash]\n"
        : [hash] "=r"(hash), [bytes] "+r"(bytes), [size] "+r"(size)
        :
        : "eax", "ecx", "memory"
    );
    #elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    // MSVC x86/x86_64 汇编优化
    __asm {
        xor eax, eax
        mov eax, 5381
        loop_start:
        cmp size, 0
        je loop_end
        mov cl, byte ptr [bytes]
        shl eax, 5
        add eax, eax
        xor al, cl
        inc bytes
        dec size
        jmp loop_start
        loop_end:
        mov hash, eax
    }
    #else
    // 其他平台使用标准哈希计算
    for (size_t i = 0; i < size; i++) {
        hash = ((hash << 5) + hash) ^ bytes[i];
    }
    #endif
    
    return hash;
}

// 快速原子操作
void PerformanceUtils::atomic_inc(volatile int* value) {
    #if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    // GCC x86/x86_64 汇编优化
    asm volatile (
        "lock inc dword ptr [%[value]]\n"
        :
        : [value] "r"(value)
        : "memory"
    );
    #elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    // MSVC x86/x86_64 汇编优化
    __asm {
        mov eax, value
        lock inc dword ptr [eax]
    }
    #else
    // 其他平台使用标准原子操作
    __sync_fetch_and_add(value, 1);
    #endif
}

void PerformanceUtils::atomic_dec(volatile int* value) {
    #if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    // GCC x86/x86_64 汇编优化
    asm volatile (
        "lock dec dword ptr [%[value]]\n"
        :
        : [value] "r"(value)
        : "memory"
    );
    #elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    // MSVC x86/x86_64 汇编优化
    __asm {
        mov eax, value
        lock dec dword ptr [eax]
    }
    #else
    // 其他平台使用标准原子操作
    __sync_fetch_and_sub(value, 1);
    #endif
}

// 快速无锁队列操作
bool PerformanceUtils::cas(volatile void** ptr, void* old_val, void* new_val) {
    #if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    // GCC x86/x86_64 汇编优化
    bool result;
    asm volatile (
        "lock cmpxchgq %[new_val], (%[ptr])\n"
        "sete %%al\n"
        "movzx %%al, %[result]\n"
        : [result] "=r"(result)
        : [ptr] "r"(ptr), [old_val] "r"(old_val), [new_val] "r"(new_val)
        : "eax", "memory"
    );
    return result;
    #elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    // MSVC x86/x86_64 汇编优化
    bool result;
    __asm {
        mov eax, old_val
        mov edx, new_val
        mov ecx, ptr
        lock cmpxchg dword ptr [ecx], edx
        sete al
        movzx al, al
        mov result, eax
    }
    return result;
    #else
    // 其他平台使用标准原子操作
    return __sync_bool_compare_and_swap(ptr, old_val, new_val);
    #endif
}

} // namespace utils
} // namespace aurorart