#ifndef AURORART_SCHEDULER_H
#define AURORART_SCHEDULER_H

#include <functional>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <map>

namespace aurorart {
namespace scheduler {

using TaskId = int64_t;

enum class Priority {
    LOW,
    NORMAL,
    HIGH,
    CRITICAL
};

enum class SchedulerType {
    COROUTINE,
    PRIORITY,
    TIME_TRIGGERED
};

class Task {
public:
    Task(std::function<void()> func, Priority priority, bool isRealTime);
    
    TaskId getId() const;
    Priority getPriority() const;
    bool isRealTime() const;
    void run();
    
private:
    std::function<void()> func_;
    Priority priority_;
    bool isRealTime_;
    TaskId id_;
    static std::atomic<TaskId> nextId_;
};

class Scheduler {
public:
    Scheduler();
    virtual ~Scheduler();
    
    virtual void start();
    virtual void stop();
    virtual bool isRunning() const;
    
    virtual void schedule(const std::shared_ptr<Task>& task) = 0;
    virtual void cancel(TaskId taskId) = 0;
    
protected:
    std::atomic<bool> running_;
};

// 前向声明
class Coroutine;
enum class CoroutineStatus;

class CoroutineScheduler : public Scheduler {
public:
    CoroutineScheduler();
    ~CoroutineScheduler();
    
    void schedule(const std::shared_ptr<Task>& task) override;
    void cancel(TaskId taskId) override;
    
private:
    void process(int threadIndex);
    
    std::vector<std::thread> threads_;
    std::vector<std::queue<std::shared_ptr<Coroutine>>> taskQueues_;
    std::vector<std::condition_variable> cvs_;
    std::vector<size_t> queueSizes_;
    std::vector<std::shared_ptr<Coroutine>> freeCoroutines_;
    std::unordered_map<TaskId, std::shared_ptr<Coroutine>> taskCoroutineMap_;
    std::mutex mutex_;
    std::atomic<size_t> nextThreadIndex_;
    size_t coroutinePoolSize_;
};

class TaskPriorityComparator {
public:
    bool operator()(const std::shared_ptr<Task>& a, const std::shared_ptr<Task>& b) {
        // 优先级高的任务应该在队列前面
        return a->getPriority() < b->getPriority();
    }
};

class PriorityScheduler : public Scheduler {
public:
    PriorityScheduler();
    ~PriorityScheduler();
    
    void schedule(const std::shared_ptr<Task>& task) override;
    void cancel(TaskId taskId) override;
    
private:
    void process();
    
    std::priority_queue<std::shared_ptr<Task>, std::vector<std::shared_ptr<Task>>, TaskPriorityComparator> taskQueue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::thread> threads_;
    std::atomic<size_t> nextThreadIndex_;
};

class TimeTaskComparator {
public:
    bool operator()(const std::pair<std::chrono::steady_clock::time_point, std::shared_ptr<Task>>& a,
                   const std::pair<std::chrono::steady_clock::time_point, std::shared_ptr<Task>>& b) {
        // 时间早的任务应该在队列前面
        return a.first > b.first;
    }
};

// 前向声明
class PeriodicTask;

class TimeTriggeredScheduler : public Scheduler {
public:
    TimeTriggeredScheduler();
    ~TimeTriggeredScheduler();
    
    void schedule(const std::shared_ptr<Task>& task) override;
    void cancel(TaskId taskId) override;
    
    void scheduleAt(const std::shared_ptr<Task>& task, std::chrono::steady_clock::time_point time);
    void schedulePeriodic(const std::shared_ptr<Task>& task, std::chrono::duration<double> period);
    
private:
    void process();
    
    std::queue<std::shared_ptr<Task>> taskQueue_;
    std::priority_queue<std::pair<std::chrono::steady_clock::time_point, std::shared_ptr<Task>>, 
                       std::vector<std::pair<std::chrono::steady_clock::time_point, std::shared_ptr<Task>>>,
                       TimeTaskComparator> timeTaskQueue_;
    std::unordered_map<TaskId, std::shared_ptr<PeriodicTask>> periodicTasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread thread_;
};

class SchedulerManager {
public:
    static SchedulerManager& instance();
    
    void init();
    void shutdown();
    
    std::shared_ptr<Scheduler> getScheduler(SchedulerType type);
    TaskId scheduleTask(SchedulerType type, std::function<void()> func, Priority priority = Priority::NORMAL, bool isRealTime = false);
    void cancelTask(TaskId taskId);
    void schedulePeriodicTask(SchedulerType type, std::function<void()> func, std::chrono::duration<double> period, Priority priority = Priority::NORMAL, bool isRealTime = false);
    
private:
    SchedulerManager();
    
    std::map<SchedulerType, std::unique_ptr<Scheduler>> schedulers_;
};

} // namespace scheduler
} // namespace aurorart

#endif // AURORART_SCHEDULER_H