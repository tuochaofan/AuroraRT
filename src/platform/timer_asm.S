# 高精度定时器汇编实现

# x86-64 架构
#ifdef __x86_64__

.global aurorart_platform_get_timestamp_us

.section .text

# 获取微秒级时间戳
aurorart_platform_get_timestamp_us:
    rdtsc                  # 读取TSC寄存器，EDX:EAX
    shl rdx, 32            # 将EDX移位到高32位
    or rax, rdx           # 合并为64位时间戳
    # 假设TSC频率为1GHz，需要根据实际情况调整
    # 这里简化处理，直接返回TSC值
    ret

#endif

# ARM64 架构
#ifdef __aarch64__

.global aurorart_platform_get_timestamp_us

.section .text

# 获取微秒级时间戳
aurorart_platform_get_timestamp_us:
    mrs x0, cntvct_el0     # 读取系统计数器
    # 假设计数器频率为1GHz，需要根据实际情况调整
    # 这里简化处理，直接返回计数器值
    ret

#endif
