#include "aurorart/platform/high_res_timer.h"
#include "aurorart/utils/logger.h"
#include <algorithm>

namespace aurorart {
namespace platform {

HighResTimer::HighResTimer()
    : is_running_(false), task_id_counter_(0) {
}

HighResTimer::~HighResTimer() {
    Stop();
}

uint64_t HighResTimer::GetTimestampUs() {
    return aurorart_platform_get_timestamp_us();
}

uint64_t HighResTimer::RegisterPeriodicTask(std::function<void()> callback, uint64_t period_us) {
    if (!callback || period_us == 0) {
        return 0;
    }
    
    uint64_t task_id = task_id_counter_++;
    auto task = std::make_shared<Task>();
    task->id = task_id;
    task->callback = callback;
    task->period_us = period_us;
    task->next_run_time = GetTimestampUs() + period_us;
    task->is_periodic = true;
    task->is_cancelled = false;
    
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    tasks_.push_back(task);
    
    AURORA_LOG_DEBUG("Registered periodic task: id={}, period={}us", task_id, period_us);
    
    // 如果定时器未运行，启动它
    if (!is_running_) {
        Start();
    }
    
    return task_id;
}

uint64_t HighResTimer::RegisterOneShotTask(std::function<void()> callback, uint64_t delay_us) {
    if (!callback) {
        return 0;
    }
    
    uint64_t task_id = task_id_counter_++;
    auto task = std::make_shared<Task>();
    task->id = task_id;
    task->callback = callback;
    task->period_us = 0;
    task->next_run_time = GetTimestampUs() + delay_us;
    task->is_periodic = false;
    task->is_cancelled = false;
    
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    tasks_.push_back(task);
    
    AURORA_LOG_DEBUG("Registered one-shot task: id={}, delay={}us", task_id, delay_us);
    
    // 如果定时器未运行，启动它
    if (!is_running_) {
        Start();
    }
    
    return task_id;
}

bool HighResTimer::CancelTask(uint64_t task_id) {
    if (task_id == 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    auto it = std::find_if(tasks_.begin(), tasks_.end(), 
        [task_id](const std::shared_ptr<Task>& task) {
            return task->id == task_id && !task->is_cancelled;
        });
    
    if (it != tasks_.end()) {
        (*it)->is_cancelled = true;
        AURORA_LOG_DEBUG("Cancelled task: id={}", task_id);
        return true;
    }
    
    return false;
}

bool HighResTimer::Start() {
    if (is_running_) {
        return true;
    }
    
    is_running_ = true;
    timer_thread_ = std::thread(&HighResTimer::TimerThreadFunc, this);
    
    AURORA_LOG_INFO("HighResTimer started");
    return true;
}

bool HighResTimer::Stop() {
    if (!is_running_) {
        return true;
    }
    
    is_running_ = false;
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
    
    // 清空任务列表
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    tasks_.clear();
    
    AURORA_LOG_INFO("HighResTimer stopped");
    return true;
}

bool HighResTimer::IsRunning() const {
    return is_running_;
}

void HighResTimer::TimerThreadFunc() {
    AURORA_LOG_DEBUG("HighResTimer thread started");
    
    while (is_running_) {
        uint64_t current_time = GetTimestampUs();
        
        // 处理任务
        { 
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            
            // 遍历所有任务
            for (auto it = tasks_.begin(); it != tasks_.end();) {
                auto& task = *it;
                
                // 检查任务是否已取消
                if (task->is_cancelled) {
                    it = tasks_.erase(it);
                    continue;
                }
                
                // 检查任务是否需要执行
                if (current_time >= task->next_run_time) {
                    // 执行任务
                    try {
                        task->callback();
                    } catch (const std::exception& e) {
                        AURORA_LOG_ERROR("Task execution failed: {}", e.what());
                    }
                    
                    // 更新下次运行时间或标记为完成
                    if (task->is_periodic) {
                        // 对于周期性任务，更新下次运行时间
                        task->next_run_time += task->period_us;
                        ++it;
                    } else {
                        // 对于一次性任务，标记为完成并移除
                        it = tasks_.erase(it);
                    }
                } else {
                    ++it;
                }
            }
        }
        
        // 短暂休眠，避免忙等
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    
    AURORA_LOG_DEBUG("HighResTimer thread stopped");
}

} // namespace platform
} // namespace aurorart
