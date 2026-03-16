#include "aurorart/scheduler/scheduler.h"
#include "aurorart/utils/logger.h"
#include "aurorart/utils/performance.h"
#include "aurorart/platform/platform_abstraction.h"
#include <algorithm>
#include <queue>
#include <thread>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <tuple>
#include <deque>

namespace aurorart {
namespace scheduler {

// 任务类实现
Task::Task(std::function<void()> func, Priority priority, bool isRealTime)
    : func_(func), priority_(priority), isRealTime_(isRealTime), id_(nextId_++) {
}

TaskId Task::getId() const {
    return id_;
}

Priority Task::getPriority() const {
    return priority_;
}

bool Task::isRealTime() const {
    return isRealTime_;
}

void Task::run() {
    if (func_) {
        func_();
    }
}

std::atomic<TaskId> Task::nextId_(0);

// 调度器基类实现
Scheduler::Scheduler() : running_(false) {
}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    running_ = true;
}

void Scheduler::stop() {
    running_ = false;
}

bool Scheduler::isRunning() const {
    return running_;
}

// 协程上下文结构
typedef struct {
    void* sp; // 栈指针
    void* ip; // 指令指针
} CoroutineContext;

// 协程状态
enum class CoroutineStatus {
    READY,
    RUNNING,
    SUSPENDED,
    FINISHED
};

// 协程类
class Coroutine {
public:
    Coroutine(std::function<void()> func, size_t stackSize = 128 * 1024)
        : func_(func), stackSize_(stackSize), stack_(nullptr), status_(CoroutineStatus::READY) {
        // 分配栈空间
        stack_ = malloc(stackSize_);
        if (!stack_) {
            AURORA_LOG_ERROR("Failed to allocate coroutine stack of size {}", stackSize_);
            status_ = CoroutineStatus::FINISHED;
            return;
        }
        
        try {
            // 初始化协程上下文
            initContext();
        } catch (const std::exception& e) {
            AURORA_LOG_ERROR("Failed to initialize coroutine context: {}", e.what());
            if (stack_) {
                free(stack_);
                stack_ = nullptr;
            }
            status_ = CoroutineStatus::FINISHED;
        }
    }
    
    ~Coroutine() {
        if (stack_) {
            free(stack_);
            stack_ = nullptr;
        }
    }
    
    bool resume() {
        if (status_ == CoroutineStatus::READY || status_ == CoroutineStatus::SUSPENDED) {
            try {
                CoroutineContext oldContext;
                swapContext(&oldContext, &context_);
                status_ = CoroutineStatus::RUNNING;
                return true;
            } catch (const std::exception& e) {
                AURORA_LOG_ERROR("Failed to resume coroutine: {}", e.what());
                status_ = CoroutineStatus::FINISHED;
                return false;
            }
        }
        return false;
    }
    
    bool suspend() {
        if (status_ == CoroutineStatus::RUNNING) {
            try {
                CoroutineContext oldContext = context_;
                swapContext(&context_, &mainContext_);
                status_ = CoroutineStatus::SUSPENDED;
                return true;
            } catch (const std::exception& e) {
                AURORA_LOG_ERROR("Failed to suspend coroutine: {}", e.what());
                status_ = CoroutineStatus::FINISHED;
                return false;
            }
        }
        return false;
    }
    
    CoroutineStatus getStatus() const {
        return status_;
    }
    
    bool isValid() const {
        return stack_ != nullptr && (status_ == CoroutineStatus::READY || status_ == CoroutineStatus::RUNNING || status_ == CoroutineStatus::SUSPENDED);
    }
    
private:
    void initContext();
    static void coroutineEntry(void* arg);
    
    std::function<void()> func_;
    size_t stackSize_;
    void* stack_;
    CoroutineContext context_;
    CoroutineContext mainContext_;
    CoroutineStatus status_;
};

// 协程通道类，用于协程间通信
template<typename T>
class Channel {
public:
    Channel(size_t capacity = 1024) : capacity_(capacity), closed_(false) {}
    
    bool send(const T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待通道有空间
        not_full_.wait(lock, [this]() { return closed_ || queue_.size() < capacity_; });
        
        if (closed_) {
            return false;
        }
        
        queue_.push_back(value);
        lock.unlock();
        not_empty_.notify_one();
        return true;
    }
    
    bool receive(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待通道有数据
        not_empty_.wait(lock, [this]() { return closed_ || !queue_.empty(); });
        
        if (closed_ && queue_.empty()) {
            return false;
        }
        
        value = queue_.front();
        queue_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return true;
    }
    
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    
    bool isClosed() const {
        return closed_;
    }
    
private:
    size_t capacity_;
    std::deque<T> queue_;
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::atomic<bool> closed_;
};

// 信号量类，用于协程同步
class Semaphore {
public:
    Semaphore(int count = 0) : count_(count) {}
    
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return count_ > 0; });
        count_--;
    }
    
    void signal() {
        std::lock_guard<std::mutex> lock(mutex_);
        count_++;
        cv_.notify_one();
    }
    
    int getCount() const {
        return count_;
    }
    
private:
    std::atomic<int> count_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

// 协程上下文切换（x86-64汇编实现）
#if defined(__x86_64__)
void swapContext(CoroutineContext* oldCtx, CoroutineContext* newCtx) {
    // 保存当前上下文
    asm volatile (
        "push rbp\n"
        "push rbx\n"
        "push r12\n"
        "push r13\n"
        "push r14\n"
        "push r15\n"
        "mov [rdi], rsp\n"
        "lea rax, [swapContext_return]\n"
        "mov [rdi+8], rax\n"
        // 恢复新上下文
        "mov rsp, [rsi]\n"
        "jmp [rsi+8]\n"
        "swapContext_return:\n"
        "pop r15\n"
        "pop r14\n"
        "pop r13\n"
        "pop r12\n"
        "pop rbx\n"
        "pop rbp\n"
        : : "D"(oldCtx), "S"(newCtx) : "memory", "rax"
    );
}
#elif defined(__aarch64__)
void swapContext(CoroutineContext* oldCtx, CoroutineContext* newCtx) {
    // ARM64汇编实现
    asm volatile (
        "stp x19, x20, [x0]\n"
        "stp x21, x22, [x0, #16]\n"
        "stp x23, x24, [x0, #32]\n"
        "stp x25, x26, [x0, #48]\n"
        "stp x27, x28, [x0, #64]\n"
        "stp x29, x30, [x0, #80]\n"
        "mov [x0, #96], sp\n"
        "ldp x19, x20, [x1]\n"
        "ldp x21, x22, [x1, #16]\n"
        "ldp x23, x24, [x1, #32]\n"
        "ldp x25, x26, [x1, #48]\n"
        "ldp x27, x28, [x1, #64]\n"
        "ldp x29, x30, [x1, #80]\n"
        "mov sp, [x1, #96]\n"
        : : "r"(oldCtx), "r"(newCtx) : "memory"
    );
}
#elif defined(_WIN32)
#include <windows.h>
void swapContext(CoroutineContext* oldCtx, CoroutineContext* newCtx) {
    // Windows平台实现
    CONTEXT ctx;
    ZeroMemory(&ctx, sizeof(CONTEXT));
    ctx.ContextFlags = CONTEXT_FULL;
    
    // 保存当前上下文
    if (!GetThreadContext(GetCurrentThread(), &ctx)) {
        AURORA_LOG_ERROR("Failed to get thread context: {}", GetLastError());
        return;
    }
    
    oldCtx->sp = reinterpret_cast<void*>(ctx.Rsp);
    oldCtx->ip = reinterpret_cast<void*>(ctx.Rip);
    
    // 恢复新上下文
    ctx.Rsp = reinterpret_cast<DWORD64>(newCtx->sp);
    ctx.Rip = reinterpret_cast<DWORD64>(newCtx->ip);
    
    if (!SetThreadContext(GetCurrentThread(), &ctx)) {
        AURORA_LOG_ERROR("Failed to set thread context: {}", GetLastError());
        return;
    }
}
#elif defined(__powerpc64__)
void swapContext(CoroutineContext* oldCtx, CoroutineContext* newCtx) {
    // PowerPC 64位实现
    asm volatile (
        "stdu r1, -112(r1)\n"
        "stmw r3, 8(r1)\n"
        "std r1, 0(%r3)\n"
        "mflr r4\n"
        "std r4, 8(%r3)\n"
        "ld r1, 0(%r4)\n"
        "ld r4, 8(%r4)\n"
        "mtlr r4\n"
        "lmw r3, 8(r1)\n"
        "addi r1, r1, 112\n"
        : : "r"(oldCtx), "r"(newCtx) : "memory"
    );
}
#elif defined(__riscv)
void swapContext(CoroutineContext* oldCtx, CoroutineContext* newCtx) {
    // RISC-V实现
    asm volatile (
        "addi sp, sp, -160\n"
        "sd ra, 0(sp)\n"
        "sd s0, 8(sp)\n"
        "sd s1, 16(sp)\n"
        "sd s2, 24(sp)\n"
        "sd s3, 32(sp)\n"
        "sd s4, 40(sp)\n"
        "sd s5, 48(sp)\n"
        "sd s6, 56(sp)\n"
        "sd s7, 64(sp)\n"
        "sd s8, 72(sp)\n"
        "sd s9, 80(sp)\n"
        "sd s10, 88(sp)\n"
        "sd s11, 96(sp)\n"
        "sd sp, 0(%0)\n"
        "ld sp, 0(%1)\n"
        "ld ra, 0(sp)\n"
        "ld s0, 8(sp)\n"
        "ld s1, 16(sp)\n"
        "ld s2, 24(sp)\n"
        "ld s3, 32(sp)\n"
        "ld s4, 40(sp)\n"
        "ld s5, 48(sp)\n"
        "ld s6, 56(sp)\n"
        "ld s7, 64(sp)\n"
        "ld s8, 72(sp)\n"
        "ld s9, 80(sp)\n"
        "ld s10, 88(sp)\n"
        "ld s11, 96(sp)\n"
        "addi sp, sp, 160\n"
        : : "r"(oldCtx), "r"(newCtx) : "memory"
    );
}
#else
void swapContext(CoroutineContext* oldCtx, CoroutineContext* newCtx) {
    // 其他平台的简化实现
    AURORA_LOG_DEBUG("Swapping coroutine context");
}
#endif

void Coroutine::initContext() {
    // 初始化协程上下文
    char* stackTop = static_cast<char*>(stack_) + stackSize_;
    
    // 对齐栈指针
    stackTop = reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(stackTop) & ~0xf);
    
    // 设置栈指针
    context_.sp = stackTop;
    
    // 设置入口点
    context_.ip = reinterpret_cast<void*>(coroutineEntry);
    
    // 在栈上压入参数
    stackTop -= sizeof(void*);
    *reinterpret_cast<void**>(stackTop) = this;
    context_.sp = stackTop;
    
    AURORA_LOG_DEBUG("Initializing coroutine context");
}

void Coroutine::coroutineEntry(void* arg) {
    Coroutine* coroutine = static_cast<Coroutine*>(arg);
    if (coroutine->func_) {
        coroutine->func_();
    }
    coroutine->status_ = CoroutineStatus::FINISHED;
    
    // 协程完成后切换回主上下文
    coroutine->suspend();
}

// 协程调度器实现
CoroutineScheduler::CoroutineScheduler() : nextThreadIndex_(0), coroutinePoolSize_(0) {
    // 初始化线程池
    int threadCount = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    taskQueues_.resize(threadCount);
    cvs_.resize(threadCount);
    queueSizes_.resize(threadCount, 0);
    for (int i = 0; i < threadCount; ++i) {
        threads_.emplace_back(&CoroutineScheduler::process, this, i);
    }
    
    // 预创建协程池
    int initialCoroutines = threadCount * 4;
    for (int i = 0; i < initialCoroutines; ++i) {
        auto coroutine = std::make_shared<Coroutine>([]() {});
        if (coroutine->isValid()) {
            freeCoroutines_.emplace_back(coroutine);
            coroutinePoolSize_++;
        }
    }
    
    AURORA_LOG_INFO("CoroutineScheduler initialized with {} threads and {} pre-created coroutines", threadCount, coroutinePoolSize_);
}

CoroutineScheduler::~CoroutineScheduler() {
    stop();
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    // 清理协程池
    freeCoroutines_.clear();
    taskCoroutineMap_.clear();
}

void CoroutineScheduler::schedule(const std::shared_ptr<Task>& task) {
    if (!task) {
        AURORA_LOG_ERROR("CoroutineScheduler::schedule: Invalid null task");
        return;
    }
    
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 获取或创建协程
        std::shared_ptr<Coroutine> coroutine;
        if (!freeCoroutines_.empty()) {
            coroutine = freeCoroutines_.back();
            freeCoroutines_.pop_back();
            coroutinePoolSize_--;
        } else {
            // 动态创建新协程
            coroutine = std::make_shared<Coroutine>([]() {});
        }
        
        // 重置协程，设置新的任务
        coroutine = std::make_shared<Coroutine>([this, task]() {
            try {
                auto start_time = std::chrono::steady_clock::now();
                task->run();
                auto end_time = std::chrono::steady_clock::now();
                auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
                AURORA_LOG_DEBUG("Coroutine task {} executed in {}μs", task->getId(), execution_time);
            } catch (const std::exception& e) {
                AURORA_LOG_ERROR("Coroutine task execution error: {}", e.what());
            }
        });
        
        // 记录任务与协程的映射
        taskCoroutineMap_[task->getId()] = coroutine;
        
        // 选择队列长度最小的线程，实现负载均衡
        size_t threadIndex = 0;
        size_t minQueueSize = queueSizes_[0];
        for (size_t i = 1; i < queueSizes_.size(); ++i) {
            if (queueSizes_[i] < minQueueSize) {
                minQueueSize = queueSizes_[i];
                threadIndex = i;
            }
        }
        
        taskQueues_[threadIndex].push(coroutine);
        queueSizes_[threadIndex]++;
        cvs_[threadIndex].notify_one();
        
        AURORA_LOG_DEBUG("CoroutineScheduler::schedule: Scheduled task {} with priority {} to thread {}", 
                       task->getId(), static_cast<int>(task->getPriority()), threadIndex);
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("CoroutineScheduler::schedule error: {}", e.what());
    }
}

void CoroutineScheduler::cancel(TaskId taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 移除任务与协程的映射
    auto it = taskCoroutineMap_.find(taskId);
    if (it != taskCoroutineMap_.end()) {
        auto coroutine = it->second;
        taskCoroutineMap_.erase(it);
        
        // 遍历所有任务队列，移除对应协程
        for (auto& queue : taskQueues_) {
            std::queue<std::shared_ptr<Coroutine>> newQueue;
            while (!queue.empty()) {
                auto taskCoroutine = queue.front();
                queue.pop();
                if (taskCoroutine != coroutine) {
                    newQueue.push(taskCoroutine);
                }
            }
            queue = std::move(newQueue);
        }
    }
}

void CoroutineScheduler::process(int threadIndex) {
    while (isRunning()) {
        std::shared_ptr<Coroutine> coroutine;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cvs_[threadIndex].wait(lock, [this, threadIndex]() { 
                return !taskQueues_[threadIndex].empty() || !isRunning(); 
            });
            
            if (!isRunning() && taskQueues_[threadIndex].empty()) {
                break;
            }
            
            if (!taskQueues_[threadIndex].empty()) {
                coroutine = taskQueues_[threadIndex].front();
                taskQueues_[threadIndex].pop();
                queueSizes_[threadIndex]--;
            }
        }
        
        if (coroutine) {
            try {
                // 运行协程
                if (coroutine->isValid()) {
                    coroutine->resume();
                } else {
                    AURORA_LOG_WARN("Coroutine is invalid, skipping execution");
                }
                
                // 协程完成后回收
                if (coroutine->getStatus() == CoroutineStatus::FINISHED) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    // 限制协程池大小，防止内存泄漏
                    if (freeCoroutines_.size() < threads_.size() * 8) {
                        freeCoroutines_.push_back(coroutine);
                        coroutinePoolSize_++;
                    }
                    
                    // 清理任务与协程的映射
                    for (auto it = taskCoroutineMap_.begin(); it != taskCoroutineMap_.end();) {
                        if (it->second == coroutine) {
                            it = taskCoroutineMap_.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            } catch (const std::exception& e) {
                AURORA_LOG_ERROR("Coroutine execution error: {}", e.what());
                // 确保协程被标记为完成
                std::lock_guard<std::mutex> lock(mutex_);
                // 限制协程池大小，防止内存泄漏
                if (freeCoroutines_.size() < threads_.size() * 8) {
                    freeCoroutines_.push_back(coroutine);
                    coroutinePoolSize_++;
                }
                
                // 清理任务与协程的映射
                for (auto it = taskCoroutineMap_.begin(); it != taskCoroutineMap_.end();) {
                    if (it->second == coroutine) {
                        it = taskCoroutineMap_.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
    }
}

// 优先级调度器实现
PriorityScheduler::PriorityScheduler() : nextThreadIndex_(0) {
    // 初始化线程池
    int threadCount = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    for (int i = 0; i < threadCount; ++i) {
        threads_.emplace_back(&PriorityScheduler::process, this);
    }
    
    AURORA_LOG_INFO("PriorityScheduler initialized with {} threads", threadCount);
}

PriorityScheduler::~PriorityScheduler() {
    stop();
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void PriorityScheduler::schedule(const std::shared_ptr<Task>& task) {
    std::lock_guard<std::mutex> lock(mutex_);
    taskQueue_.push(task);
    cv_.notify_one();
}

void PriorityScheduler::cancel(TaskId taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 遍历任务队列，移除指定ID的任务
    std::priority_queue<std::shared_ptr<Task>, std::vector<std::shared_ptr<Task>>, TaskPriorityComparator> newQueue;
    while (!taskQueue_.empty()) {
        auto task = taskQueue_.top();
        taskQueue_.pop();
        if (task->getId() != taskId) {
            newQueue.push(task);
        }
    }
    taskQueue_ = std::move(newQueue);
}

void PriorityScheduler::process() {
    while (isRunning()) {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return !taskQueue_.empty() || !isRunning(); });
            
            if (!isRunning() && taskQueue_.empty()) {
                break;
            }
            
            if (!taskQueue_.empty()) {
                task = taskQueue_.top();
                taskQueue_.pop();
            }
        }
        
        if (task) {
            // 对于实时任务，设置线程优先级
            if (task->isRealTime()) {
                auto platform = platform::PlatformAbstraction::create();
                if (platform) {
                    platform->setThreadPriority(std::this_thread::get_id(), platform::ThreadPriority::REALTIME);
                }
            }
            
            task->run();
        }
    }
}

// 周期性任务结构
class PeriodicTask {
public:
    PeriodicTask(std::shared_ptr<Task> task, std::chrono::duration<double> period)
        : task_(task), period_(period) {
    }
    
    std::shared_ptr<Task> getTask() const {
        return task_;
    }
    
    std::chrono::duration<double> getPeriod() const {
        return period_;
    }
    
private:
    std::shared_ptr<Task> task_;
    std::chrono::duration<double> period_;
};

// 时间触发调度器实现
TimeTriggeredScheduler::TimeTriggeredScheduler() : thread_(&TimeTriggeredScheduler::process, this) {
    AURORA_LOG_INFO("TimeTriggeredScheduler initialized");
}

TimeTriggeredScheduler::~TimeTriggeredScheduler() {
    stop();
    if (thread_.joinable()) {
        thread_.join();
    }
    
    // 清理任务
    periodicTasks_.clear();
    while (!taskQueue_.empty()) {
        taskQueue_.pop();
    }
    while (!timeTaskQueue_.empty()) {
        timeTaskQueue_.pop();
    }
}

void TimeTriggeredScheduler::schedule(const std::shared_ptr<Task>& task) {
    if (!task) {
        AURORA_LOG_ERROR("TimeTriggeredScheduler::schedule: Invalid null task");
        return;
    }
    
    // 时间触发调度器需要指定执行时间
    // 这里简化实现，默认立即执行
    std::lock_guard<std::mutex> lock(mutex_);
    taskQueue_.push(task);
    cv_.notify_one();
    
    AURORA_LOG_DEBUG("TimeTriggeredScheduler::schedule: Scheduled task {} for immediate execution", task->getId());
}

void TimeTriggeredScheduler::scheduleAt(const std::shared_ptr<Task>& task, std::chrono::steady_clock::time_point time) {
    if (!task) {
        AURORA_LOG_ERROR("TimeTriggeredScheduler::scheduleAt: Invalid null task");
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    timeTaskQueue_.emplace(time, task);
    cv_.notify_one();
    
    auto now = std::chrono::steady_clock::now();
    auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(time - now).count();
    AURORA_LOG_DEBUG("TimeTriggeredScheduler::scheduleAt: Scheduled task {} for execution in {}ms", 
                   task->getId(), delay);
}

void TimeTriggeredScheduler::schedulePeriodic(const std::shared_ptr<Task>& task, std::chrono::duration<double> period) {
    if (!task) {
        AURORA_LOG_ERROR("TimeTriggeredScheduler::schedulePeriodic: Invalid null task");
        return;
    }
    
    // 实现周期性任务调度
    auto now = std::chrono::steady_clock::now();
    scheduleAt(task, now);
    
    // 记录周期性任务
    std::lock_guard<std::mutex> lock(mutex_);
    periodicTasks_[task->getId()] = std::make_shared<PeriodicTask>(task, period);
    
    AURORA_LOG_DEBUG("Scheduled periodic task {} with period {}ms", task->getId(), 
                     std::chrono::duration_cast<std::chrono::milliseconds>(period).count());
}

void TimeTriggeredScheduler::cancel(TaskId taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 移除普通任务
    std::queue<std::shared_ptr<Task>> newQueue;
    while (!taskQueue_.empty()) {
        auto task = taskQueue_.front();
        taskQueue_.pop();
        if (task->getId() != taskId) {
            newQueue.push(task);
        }
    }
    taskQueue_ = std::move(newQueue);
    
    // 移除时间触发任务
    std::priority_queue<std::pair<std::chrono::steady_clock::time_point, std::shared_ptr<Task>>, 
                       std::vector<std::pair<std::chrono::steady_clock::time_point, std::shared_ptr<Task>>>,
                       TimeTaskComparator> newTimeQueue;
    while (!timeTaskQueue_.empty()) {
        auto taskPair = timeTaskQueue_.top();
        timeTaskQueue_.pop();
        if (taskPair.second->getId() != taskId) {
            newTimeQueue.push(taskPair);
        }
    }
    timeTaskQueue_ = std::move(newTimeQueue);
    
    // 移除周期性任务
    auto it = periodicTasks_.find(taskId);
    if (it != periodicTasks_.end()) {
        periodicTasks_.erase(it);
        AURORA_LOG_DEBUG("Cancelled periodic task {}", taskId);
    } else {
        AURORA_LOG_DEBUG("Cancelled task {}", taskId);
    }
}

void TimeTriggeredScheduler::process() {
    while (isRunning()) {
        std::shared_ptr<Task> task;
        bool isPeriodic = false;
        TaskId taskId = -1;
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // 检查时间触发任务
            if (!timeTaskQueue_.empty()) {
                auto now = std::chrono::steady_clock::now();
                auto nextTaskTime = timeTaskQueue_.top().first;
                
                if (now >= nextTaskTime) {
                    task = timeTaskQueue_.top().second;
                    taskId = task->getId();
                    timeTaskQueue_.pop();
                    
                    // 检查是否是周期性任务
                    isPeriodic = (periodicTasks_.find(taskId) != periodicTasks_.end());
                } else {
                    // 等待到下一个任务执行时间
                    cv_.wait_until(lock, nextTaskTime);
                    continue;
                }
            } else {
                // 等待普通任务
                cv_.wait(lock, [this]() { return !taskQueue_.empty() || !isRunning(); });
                
                if (!isRunning() && taskQueue_.empty()) {
                    break;
                }
                
                if (!taskQueue_.empty()) {
                    task = taskQueue_.front();
                    taskQueue_.pop();
                }
            }
        }
        
        if (task) {
            try {
                // 对于实时任务，设置线程优先级
                if (task->isRealTime()) {
                    auto platform = platform::PlatformAbstraction::create();
                    if (platform) {
                        platform->setThreadPriority(std::this_thread::get_id(), platform::ThreadPriority::REALTIME);
                    }
                }
                
                // 记录任务执行开始时间
                auto start_time = std::chrono::steady_clock::now();
                
                // 执行任务
                task->run();
                
                // 记录任务执行结束时间
                auto end_time = std::chrono::steady_clock::now();
                auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
                
                AURORA_LOG_DEBUG("Task {} executed in {}μs", task->getId(), execution_time);
                
                // 重新调度周期性任务
                if (isPeriodic) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = periodicTasks_.find(taskId);
                    if (it != periodicTasks_.end()) {
                        auto periodicTask = it->second;
                        auto now = std::chrono::steady_clock::now();
                        auto period = periodicTask->getPeriod();
                        auto nextTime = now + period;
                        timeTaskQueue_.emplace(nextTime, periodicTask->getTask());
                        cv_.notify_one();
                        
                        AURORA_LOG_DEBUG("Rescheduled periodic task {} for next execution at {}", 
                                         taskId, std::chrono::duration_cast<std::chrono::milliseconds>(nextTime.time_since_epoch()).count());
                    }
                }
            } catch (const std::exception& e) {
                AURORA_LOG_ERROR("TimeTriggeredScheduler task execution error: {}", e.what());
                
                // 即使任务执行失败，也要重新调度周期性任务
                if (isPeriodic) {
                    try {
                        std::lock_guard<std::mutex> lock(mutex_);
                        auto it = periodicTasks_.find(taskId);
                        if (it != periodicTasks_.end()) {
                            auto periodicTask = it->second;
                            auto now = std::chrono::steady_clock::now();
                            auto period = periodicTask->getPeriod();
                            auto nextTime = now + period;
                            timeTaskQueue_.emplace(nextTime, periodicTask->getTask());
                            cv_.notify_one();
                            
                            AURORA_LOG_DEBUG("Rescheduled periodic task {} after execution error", taskId);
                        }
                    } catch (const std::exception& inner_e) {
                        AURORA_LOG_ERROR("Failed to reschedule periodic task: {}", inner_e.what());
                    }
                }
            }
        }
    }
}

// 调度器管理器实现
SchedulerManager::SchedulerManager() {
    // 初始化各种调度器
    schedulers_[SchedulerType::COROUTINE] = std::make_unique<CoroutineScheduler>();
    schedulers_[SchedulerType::PRIORITY] = std::make_unique<PriorityScheduler>();
    schedulers_[SchedulerType::TIME_TRIGGERED] = std::make_unique<TimeTriggeredScheduler>();
    
    AURORA_LOG_INFO("SchedulerManager initialized with {} schedulers", schedulers_.size());
}

SchedulerManager& SchedulerManager::instance() {
    static SchedulerManager instance;
    return instance;
}

void SchedulerManager::init() {
    for (auto& [type, scheduler] : schedulers_) {
        scheduler->start();
    }
    AURORA_LOG_INFO("SchedulerManager started");
}

void SchedulerManager::shutdown() {
    for (auto& [type, scheduler] : schedulers_) {
        scheduler->stop();
    }
    AURORA_LOG_INFO("SchedulerManager shutdown");
}

std::shared_ptr<Scheduler> SchedulerManager::getScheduler(SchedulerType type) {
    auto it = schedulers_.find(type);
    if (it != schedulers_.end()) {
        return it->second;
    }
    return nullptr;
}

TaskId SchedulerManager::scheduleTask(SchedulerType type, std::function<void()> func, Priority priority, bool isRealTime) {
    auto scheduler = getScheduler(type);
    if (!scheduler) {
        AURORA_LOG_ERROR("Scheduler type {} not found", static_cast<int>(type));
        return -1;
    }
    
    auto task = std::make_shared<Task>(func, priority, isRealTime);
    scheduler->schedule(task);
    return task->getId();
}

void SchedulerManager::cancelTask(TaskId taskId) {
    for (auto& [type, scheduler] : schedulers_) {
        scheduler->cancel(taskId);
    }
}

void SchedulerManager::schedulePeriodicTask(SchedulerType type, std::function<void()> func, std::chrono::duration<double> period, Priority priority, bool isRealTime) {
    if (type == SchedulerType::TIME_TRIGGERED) {
        auto timeScheduler = dynamic_cast<TimeTriggeredScheduler*>(schedulers_[type].get());
        if (timeScheduler) {
            auto task = std::make_shared<Task>(func, priority, isRealTime);
            timeScheduler->schedulePeriodic(task, period);
        }
    } else {
        AURORA_LOG_ERROR("Periodic task scheduling only supported for TIME_TRIGGERED scheduler");
    }
}

} // namespace scheduler
} // namespace aurorart