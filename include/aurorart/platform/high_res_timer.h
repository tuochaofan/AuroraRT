#ifndef AURORART_HIGH_RES_TIMER_H
#define AURORART_HIGH_RES_TIMER_H

#include <functional>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

namespace aurorart {
namespace platform {

class HighResTimer {
public:
    HighResTimer();
    ~HighResTimer();
    
    // 获取微秒级时间戳
    uint64_t GetTimestampUs();
    
    // 注册周期性任务
    uint64_t RegisterPeriodicTask(std::function<void()> callback, uint64_t period_us);
    
    // 注册一次性任务
    uint64_t RegisterOneShotTask(std::function<void()> callback, uint64_t delay_us);
    
    // 取消任务
    bool CancelTask(uint64_t task_id);
    
    // 启动定时器
    bool Start();
    
    // 停止定时器
    bool Stop();
    
    // 获取定时器状态
    bool IsRunning() const;
    
private:
    // 任务结构
    struct Task {
        uint64_t id;
        std::function<void()> callback;
        uint64_t period_us;      // 周期性任务的周期
        uint64_t next_run_time;  // 下次运行时间
        bool is_periodic;        // 是否为周期性任务
        bool is_cancelled;       // 是否已取消
    };
    
    // 汇编函数声明
    extern "C" uint64_t aurorart_platform_get_timestamp_us();
    
    // 定时器线程函数
    void TimerThreadFunc();
    
    // 任务管理
    std::vector<std::shared_ptr<Task>> tasks_;
    std::mutex tasks_mutex_;
    
    // 定时器线程
    std::thread timer_thread_;
    std::atomic<bool> is_running_;
    
    // 任务ID生成
    std::atomic<uint64_t> task_id_counter_;
};

} // namespace platform
} // namespace aurorart

#endif // AURORART_HIGH_RES_TIMER_H
