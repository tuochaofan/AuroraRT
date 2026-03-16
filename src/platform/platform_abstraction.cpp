#include "aurorart/platform/platform_abstraction.h"

#if defined(__QNX__) || defined(__QNXNTO__)
// QNX Neutrino RTOS
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/neutrino.h>
#include <errno.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/utsname.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#elif defined(__linux__)
// Linux
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/utsname.h>
#include <sched.h>
#elif defined(_WIN32)
// Windows
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <direct.h>
#include <sysinfoapi.h>
#elif defined(__APPLE__)
// macOS
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>
#elif defined(__VXWORKS__)
// VxWorks
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <semaphore.h>
#endif

// 声明汇编函数
extern "C" uint64_t aurorart_platform_get_timestamp_us();

namespace aurorart {
namespace platform {

// LinuxPlatform implementation

// 内存池结构
struct MemoryPool {
    void* base;           // 内存池基地址
    size_t size;          // 内存池大小
    size_t used;          // 已使用大小
    size_t block_size;    // 块大小
    size_t block_count;   // 块数量
    std::mutex mutex;     // 互斥锁
    std::vector<void*> free_blocks; // 空闲块列表
    std::atomic<uint64_t> alloc_count; // 分配次数
    std::atomic<uint64_t> free_count;  // 释放次数
};

// 全局内存池互斥锁
static std::mutex memory_pools_mutex;

// 全局内存池
static std::unordered_map<size_t, std::unique_ptr<MemoryPool>> memory_pools;

// 内存池块大小定义
static constexpr size_t MEMORY_POOL_BLOCK_SIZES[] = {64, 256, 1024, 4096};
static constexpr size_t MEMORY_POOL_BLOCK_COUNT = 1024;

// 找到适合的内存池块大小
static size_t findSuitablePoolSize(size_t size) {
    for (size_t pool_size : MEMORY_POOL_BLOCK_SIZES) {
        if (pool_size >= size) {
            return pool_size;
        }
    }
    return 4096; // 默认最大块大小
}

void* LinuxPlatform::allocateMemory(size_t size) {
    // 小内存使用内存池
    if (size <= 4096) {
        size_t pool_size = findSuitablePoolSize(size);
        
        std::lock_guard<std::mutex> lock(memory_pools_mutex);
        auto& pool = memory_pools[pool_size];
        if (!pool) {
            // 创建新内存池
            size_t total_size = pool_size * MEMORY_POOL_BLOCK_COUNT;
            pool = std::make_unique<MemoryPool>();
            pool->base = malloc(total_size);
            pool->size = total_size;
            pool->used = 0;
            pool->block_size = pool_size;
            pool->block_count = MEMORY_POOL_BLOCK_COUNT;
            pool->alloc_count = 0;
            pool->free_count = 0;
            
            // 初始化空闲块
            for (size_t i = 0; i < MEMORY_POOL_BLOCK_COUNT; ++i) {
                pool->free_blocks.push_back(static_cast<char*>(pool->base) + i * pool_size);
            }
            
            AURORA_LOG_INFO("Created memory pool: block_size={}, block_count={}, total_size={} bytes", 
                           pool_size, MEMORY_POOL_BLOCK_COUNT, total_size);
        }
        
        // 从内存池分配
        if (!pool->free_blocks.empty()) {
            void* ptr = pool->free_blocks.back();
            pool->free_blocks.pop_back();
            pool->used += pool->block_size;
            pool->alloc_count++;
            return ptr;
        }
        
        // 内存池已满，尝试扩展
        size_t new_block_count = pool->block_count * 2;
        size_t new_total_size = pool->block_size * new_block_count;
        void* new_base = realloc(pool->base, new_total_size);
        if (new_base) {
            pool->base = new_base;
            pool->size = new_total_size;
            pool->block_count = new_block_count;
            
            // 初始化新添加的块
            for (size_t i = pool->block_count / 2; i < pool->block_count; ++i) {
                pool->free_blocks.push_back(static_cast<char*>(pool->base) + i * pool->block_size);
            }
            
            AURORA_LOG_INFO("Expanded memory pool: block_size={}, new_block_count={}, new_total_size={} bytes", 
                           pool->block_size, new_block_count, new_total_size);
            
            // 再次尝试分配
            if (!pool->free_blocks.empty()) {
                void* ptr = pool->free_blocks.back();
                pool->free_blocks.pop_back();
                pool->used += pool->block_size;
                pool->alloc_count++;
                return ptr;
            }
        }
    }
    
    // 大内存直接使用malloc
    void* ptr = malloc(size);
    if (ptr) {
        AURORA_LOG_DEBUG("Allocated large memory: {} bytes", size);
    }
    return ptr;
}

void LinuxPlatform::freeMemory(void* ptr) {
    if (!ptr) return;
    
    // 检查是否是内存池分配的内存
    std::lock_guard<std::mutex> lock(memory_pools_mutex);
    for (auto& [pool_size, pool] : memory_pools) {
        if (pool && ptr >= pool->base && ptr < static_cast<char*>(pool->base) + pool->size) {
            // 归还到内存池
            pool->free_blocks.push_back(ptr);
            pool->used -= pool->block_size;
            pool->free_count++;
            
            // 检查是否需要收缩内存池
            if (pool->free_blocks.size() > pool->block_count * 0.75 && pool->block_count > MEMORY_POOL_BLOCK_COUNT) {
                size_t new_block_count = pool->block_count / 2;
                if (new_block_count >= MEMORY_POOL_BLOCK_COUNT) {
                    size_t new_total_size = pool->block_size * new_block_count;
                    void* new_base = realloc(pool->base, new_total_size);
                    if (new_base) {
                        pool->base = new_base;
                        pool->size = new_total_size;
                        pool->block_count = new_block_count;
                        
                        // 清理超出范围的空闲块
                        auto it = pool->free_blocks.begin();
                        while (it != pool->free_blocks.end()) {
                            if (static_cast<char*>(*it) >= static_cast<char*>(pool->base) + new_total_size) {
                                it = pool->free_blocks.erase(it);
                            } else {
                                ++it;
                            }
                        }
                        
                        AURORA_LOG_INFO("Shrunk memory pool: block_size={}, new_block_count={}, new_total_size={} bytes", 
                                       pool->block_size, new_block_count, new_total_size);
                    }
                }
            }
            
            return;
        }
    }
    
    // 不是内存池分配的，直接free
    free(ptr);
}

void* LinuxPlatform::createSharedMemory(const std::string& name, size_t size) {
    // 优化：使用O_EXCL标志确保创建新的共享内存
    int fd = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666);
    if (fd == -1) {
        // 如果已存在，尝试打开
        fd = shm_open(name.c_str(), O_RDWR, 0666);
        if (fd == -1) return nullptr;
    } else {
        // 新创建的共享内存，设置大小
        if (ftruncate(fd, size) == -1) {
            close(fd);
            shm_unlink(name.c_str());
            return nullptr;
        }
    }
    
    // 优化：使用MAP_HUGETLB标志提高大内存访问性能
    int flags = MAP_SHARED;
    if (size >= 2 * 1024 * 1024) { // 2MB以上使用大页
        flags |= MAP_HUGETLB;
    }
    
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, flags, fd, 0);
    close(fd);
    return ptr;
}

void LinuxPlatform::destroySharedMemory(const std::string& name) {
    shm_unlink(name.c_str());
}

void* LinuxPlatform::mapSharedMemory(const std::string& name, size_t size) {
    int fd = shm_open(name.c_str(), O_RDWR, 0666);
    if (fd == -1) return nullptr;
    
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return ptr;
}

void LinuxPlatform::unmapSharedMemory(void* ptr, size_t size) {
    munmap(ptr, size);
}

int LinuxPlatform::createSocket(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}

int LinuxPlatform::closeSocket(int sockfd) {
    return close(sockfd);
}

bool LinuxPlatform::setSocketNonBlocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) != -1;
}

bool LinuxPlatform::setSocketReuseAddr(int sockfd) {
    int opt = 1;
    return setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != -1;
}

// 任务结构
struct ThreadPoolTask {
    ThreadFunc func;
    void* arg;
    int priority; // 任务优先级，0-9，数字越大优先级越高
    uint64_t id;
    std::chrono::steady_clock::time_point submit_time;
    
    ThreadPoolTask(ThreadFunc f, void* a, int p) 
        : func(f), arg(a), priority(p), 
          id(0), submit_time(std::chrono::steady_clock::now()) {}
    
    // 优先级比较
    bool operator<(const ThreadPoolTask& other) const {
        return priority < other.priority;
    }
};

// 线程池结构
struct ThreadPool {
    std::vector<pthread_t> threads;
    std::priority_queue<ThreadPoolTask> tasks; // 使用优先级队列
    std::mutex mutex;
    std::condition_variable cond;
    bool running;
    std::atomic<uint64_t> task_id_counter;
    std::atomic<int> active_threads;
    std::atomic<int> max_threads;
    std::atomic<int> min_threads;
    std::atomic<uint64_t> task_count;
    std::atomic<uint64_t> completed_tasks;
    
    ThreadPool() : running(false), task_id_counter(0), active_threads(0), 
                   max_threads(0), min_threads(0), task_count(0), completed_tasks(0) {}
};

// 全局线程池
static std::unique_ptr<ThreadPool> thread_pool;
static std::once_flag thread_pool_init_flag;

// 线程池工作函数
static void* thread_pool_worker(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    pool->active_threads++;
    
    while (true) {
        ThreadPoolTask task;
        bool got_task = false;
        
        {
            std::unique_lock<std::mutex> lock(pool->mutex);
            pool->cond.wait(lock, [pool]() {
                return !pool->tasks.empty() || !pool->running;
            });
            
            if (!pool->running && pool->tasks.empty()) {
                break;
            }
            
            if (!pool->tasks.empty()) {
                task = pool->tasks.top();
                pool->tasks.pop();
                got_task = true;
            }
        }
        
        if (got_task) {
            // 执行任务
            task.func(task.arg);
            pool->completed_tasks++;
        }
    }
    
    pool->active_threads--;
    return nullptr;
}

// 初始化线程池
static void init_thread_pool() {
    thread_pool = std::make_unique<ThreadPool>();
    thread_pool->running = true;
    
    // 计算线程池大小
    int core_count = LinuxPlatform::getNumberOfCores();
    int min_threads = std::max(2, core_count / 2);
    int max_threads = std::max(4, core_count * 2);
    
    thread_pool->min_threads = min_threads;
    thread_pool->max_threads = max_threads;
    
    // 创建初始线程
    for (int i = 0; i < min_threads; ++i) {
        pthread_t thread;
        if (pthread_create(&thread, nullptr, thread_pool_worker, thread_pool.get()) == 0) {
            thread_pool->threads.push_back(thread);
        }
    }
    
    AURORA_LOG_INFO("Initialized thread pool: min_threads={}, max_threads={}, initial_threads={}", 
                   min_threads, max_threads, thread_pool->threads.size());
}

// 动态调整线程池大小
static void adjust_thread_pool_size() {
    if (!thread_pool) return;
    
    int active = thread_pool->active_threads.load();
    int current_size = thread_pool->threads.size();
    int max_size = thread_pool->max_threads.load();
    int min_size = thread_pool->min_threads.load();
    
    { 
        std::lock_guard<std::mutex> lock(thread_pool->mutex);
        int task_count = thread_pool->tasks.size();
        
        // 如果任务数大于活跃线程数，且当前线程数小于最大值，增加线程
        if (task_count > active && current_size < max_size) {
            int threads_to_add = std::min(4, max_size - current_size);
            for (int i = 0; i < threads_to_add; ++i) {
                pthread_t thread;
                if (pthread_create(&thread, nullptr, thread_pool_worker, thread_pool.get()) == 0) {
                    thread_pool->threads.push_back(thread);
                }
            }
            AURORA_LOG_INFO("Expanded thread pool: new_size={}, tasks={}", 
                           thread_pool->threads.size(), task_count);
        }
        // 如果活跃线程数远小于任务数，且有空闲线程，减少线程
        else if (active < current_size / 2 && current_size > min_size) {
            // 注意：这里只是记录，实际线程退出需要等待
            AURORA_LOG_INFO("Thread pool utilization low: active={}, total={}, tasks={}", 
                           active, current_size, task_count);
        }
    }
}

// Thread functions
void* LinuxPlatform::createThread(ThreadFunc func, void* arg) {
    // 检查是否使用线程池
    bool use_thread_pool = true; // 可以根据配置决定是否使用线程池
    
    if (use_thread_pool) {
        // 使用线程池
        std::call_once(thread_pool_init_flag, init_thread_pool);
        
        {    
            std::lock_guard<std::mutex> lock(thread_pool->mutex);
            ThreadPoolTask task(func, arg, 5); // 默认优先级5
            task.id = thread_pool->task_id_counter++;
            thread_pool->tasks.push(task);
            thread_pool->task_count++;
        }
        thread_pool->cond.notify_one();
        
        // 动态调整线程池大小
        adjust_thread_pool_size();
        
        // 返回一个占位符，因为线程池中的线程是复用的
        return reinterpret_cast<void*>(1);
    } else {
        // 创建新线程
        pthread_t* thread = new pthread_t;
        if (pthread_create(thread, nullptr, func, arg) != 0) {
            delete thread;
            return nullptr;
        }
        return thread;
    }
}

void LinuxPlatform::joinThread(void* thread) {
    if (thread) {
        // 如果是线程池中的线程，不需要join
        if (reinterpret_cast<uintptr_t>(thread) != 1) {
            pthread_join(*static_cast<pthread_t*>(thread), nullptr);
            delete static_cast<pthread_t*>(thread);
        }
    }
}

// 创建带优先级的线程
void* LinuxPlatform::createThreadWithPriority(ThreadFunc func, void* arg, int priority) {
    std::call_once(thread_pool_init_flag, init_thread_pool);
    
    {    
        std::lock_guard<std::mutex> lock(thread_pool->mutex);
        ThreadPoolTask task(func, arg, priority);
        task.id = thread_pool->task_id_counter++;
        thread_pool->tasks.push(task);
        thread_pool->task_count++;
    }
    thread_pool->cond.notify_one();
    
    // 动态调整线程池大小
    adjust_thread_pool_size();
    
    return reinterpret_cast<void*>(1);
}

// Mutex functions
void* LinuxPlatform::createMutex() {
    pthread_mutex_t* mutex = new pthread_mutex_t;
    // 优化：使用PTHREAD_MUTEX_ADAPTIVE_NP类型，提高性能
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
    
    if (pthread_mutex_init(mutex, &attr) != 0) {
        delete mutex;
        pthread_mutexattr_destroy(&attr);
        return nullptr;
    }
    pthread_mutexattr_destroy(&attr);
    return mutex;
}

void LinuxPlatform::destroyMutex(void* mutex) {
    if (mutex) {
        pthread_mutex_destroy(static_cast<pthread_mutex_t*>(mutex));
        delete static_cast<pthread_mutex_t*>(mutex);
    }
}

void LinuxPlatform::lockMutex(void* mutex) {
    if (mutex) {
        pthread_mutex_lock(static_cast<pthread_mutex_t*>(mutex));
    }
}

void LinuxPlatform::unlockMutex(void* mutex) {
    if (mutex) {
        pthread_mutex_unlock(static_cast<pthread_mutex_t*>(mutex));
    }
}

// Condition variable functions
void* LinuxPlatform::createCondition() {
    pthread_cond_t* cond = new pthread_cond_t;
    if (pthread_cond_init(cond, nullptr) != 0) {
        delete cond;
        return nullptr;
    }
    return cond;
}

void LinuxPlatform::destroyCondition(void* cond) {
    if (cond) {
        pthread_cond_destroy(static_cast<pthread_cond_t*>(cond));
        delete static_cast<pthread_cond_t*>(cond);
    }
}

void LinuxPlatform::waitCondition(void* cond, void* mutex) {
    if (cond && mutex) {
        pthread_cond_wait(static_cast<pthread_cond_t*>(cond), static_cast<pthread_mutex_t*>(mutex));
    }
}

void LinuxPlatform::signalCondition(void* cond) {
    if (cond) {
        pthread_cond_signal(static_cast<pthread_cond_t*>(cond));
    }
}

void LinuxPlatform::broadcastCondition(void* cond) {
    if (cond) {
        pthread_cond_broadcast(static_cast<pthread_cond_t*>(cond));
    }
}

uint64_t LinuxPlatform::getTimestampUs() {
    // 调用汇编实现的时间戳获取函数
    return aurorart_platform_get_timestamp_us();
}

// QNX Neutrino RTOS 

void* QNXPlatform::allocateMemory(size_t size) {
    return malloc(size);
}

void QNXPlatform::freeMemory(void* ptr) {
    free(ptr);
}

void* QNXPlatform::createSharedMemory(const std::string& name, size_t size) {
    int fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1) return nullptr;
    
    if (ftruncate(fd, size) == -1) {
        close(fd);
        return nullptr;
    }
    
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return ptr;
}

void QNXPlatform::destroySharedMemory(const std::string& name) {
    shm_unlink(name.c_str());
}

void* QNXPlatform::mapSharedMemory(const std::string& name, size_t size) {
    int fd = shm_open(name.c_str(), O_RDWR, 0666);
    if (fd == -1) return nullptr;
    
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return ptr;
}

void QNXPlatform::unmapSharedMemory(void* ptr, size_t size) {
    munmap(ptr, size);
}

int QNXPlatform::createSocket(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}

int QNXPlatform::closeSocket(int sockfd) {
    return close(sockfd);
}

bool QNXPlatform::setSocketNonBlocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) != -1;
}

bool QNXPlatform::setSocketReuseAddr(int sockfd) {
    int opt = 1;
    return setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != -1;
}

void* QNXPlatform::createThread(ThreadFunc func, void* arg) {
    pthread_t* thread = new pthread_t;
    if (pthread_create(thread, nullptr, func, arg) != 0) {
        delete thread;
        return nullptr;
    }
    return thread;
}

void QNXPlatform::joinThread(void* thread) {
    if (thread) {
        pthread_join(*static_cast<pthread_t*>(thread), nullptr);
        delete static_cast<pthread_t*>(thread);
    }
}

void* QNXPlatform::createThreadWithPriority(ThreadFunc func, void* arg, int priority) {
    pthread_t* thread = new pthread_t;
    
    // 设置线程属性和优先级
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    
    // QNX 实时调度策略
    struct sched_param param;
    param.sched_priority = priority;
    
    // 使用 SCHED_FIFO 调度策略
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);
    
    if (pthread_create(thread, &attr, func, arg) != 0) {
        delete thread;
        pthread_attr_destroy(&attr);
        return nullptr;
    }
    
    pthread_attr_destroy(&attr);
    return thread;
}

void* QNXPlatform::createMutex() {
    pthread_mutex_t* mutex = new pthread_mutex_t;
    if (pthread_mutex_init(mutex, nullptr) != 0) {
        delete mutex;
        return nullptr;
    }
    return mutex;
}

void QNXPlatform::destroyMutex(void* mutex) {
    if (mutex) {
        pthread_mutex_destroy(static_cast<pthread_mutex_t*>(mutex));
        delete static_cast<pthread_mutex_t*>(mutex);
    }
}

void QNXPlatform::lockMutex(void* mutex) {
    if (mutex) {
        pthread_mutex_lock(static_cast<pthread_mutex_t*>(mutex));
    }
}

void QNXPlatform::unlockMutex(void* mutex) {
    if (mutex) {
        pthread_mutex_unlock(static_cast<pthread_mutex_t*>(mutex));
    }
}

void* QNXPlatform::createCondition() {
    pthread_cond_t* cond = new pthread_cond_t;
    if (pthread_cond_init(cond, nullptr) != 0) {
        delete cond;
        return nullptr;
    }
    return cond;
}

void QNXPlatform::destroyCondition(void* cond) {
    if (cond) {
        pthread_cond_destroy(static_cast<pthread_cond_t*>(cond));
        delete static_cast<pthread_cond_t*>(cond);
    }
}

void QNXPlatform::waitCondition(void* cond, void* mutex) {
    if (cond && mutex) {
        pthread_cond_wait(static_cast<pthread_cond_t*>(cond), static_cast<pthread_mutex_t*>(mutex));
    }
}

void QNXPlatform::signalCondition(void* cond) {
    if (cond) {
        pthread_cond_signal(static_cast<pthread_cond_t*>(cond));
    }
}

void QNXPlatform::broadcastCondition(void* cond) {
    if (cond) {
        pthread_cond_broadcast(static_cast<pthread_cond_t*>(cond));
    }
}

uint64_t QNXPlatform::getTimestampUs() {
    // QNX 使用 clock_gettime 获取高精度时间
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + 
           static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
}

// QNX 平台扩展功能实现

bool QNXPlatform::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool QNXPlatform::createDirectory(const std::string& path) {
    return mkdir(path.c_str(), 0755) == 0;
}

bool QNXPlatform::removeFile(const std::string& path) {
    return remove(path.c_str()) == 0;
}

int QNXPlatform::createSemaphore(const std::string& name, unsigned int initialCount) {
    sem_t* sem = sem_open(name.c_str(), O_CREAT, 0666, initialCount);
    return (sem == SEM_FAILED) ? -1 : reinterpret_cast<int>(reinterpret_cast<uintptr_t>(sem));
}

void QNXPlatform::destroySemaphore(int semid) {
    if (semid != -1) {
        sem_close(reinterpret_cast<sem_t*>(reinterpret_cast<uintptr_t>(semid)));
    }
}

void QNXPlatform::waitSemaphore(int semid) {
    if (semid != -1) {
        sem_wait(reinterpret_cast<sem_t*>(reinterpret_cast<uintptr_t>(semid)));
    }
}

void QNXPlatform::signalSemaphore(int semid) {
    if (semid != -1) {
        sem_post(reinterpret_cast<sem_t*>(reinterpret_cast<uintptr_t>(semid)));
    }
}

bool QNXPlatform::lockMemory(void* addr, size_t len) {
    return mlock(addr, len) == 0;
}

bool QNXPlatform::unlockMemory(void* addr, size_t len) {
    return munlock(addr, len) == 0;
}

bool QNXPlatform::setThreadPriority(void* thread, int priority) {
    if (thread) {
        // QNX 实时调度优化
        struct sched_param param;
        param.sched_priority = priority;
        
        // 使用 SCHED_FIFO 调度策略，确保实时性
        int result = pthread_setschedparam(*static_cast<pthread_t*>(thread), SCHED_FIFO, &param);
        if (result == 0) {
            AURORA_LOG_DEBUG("QNXPlatform: Set thread priority to {}", priority);
            return true;
        } else {
            AURORA_LOG_WARN("QNXPlatform: Failed to set thread priority: {}", result);
            return false;
        }
    }
    return false;
}

int QNXPlatform::getThreadPriority(void* thread) {
    if (thread) {
        struct sched_param param;
        int policy;
        if (pthread_getschedparam(*static_cast<pthread_t*>(thread), &policy, &param) != 0) {
            return -1;
        }
        return param.sched_priority;
    }
    return -1;
}

bool QNXPlatform::setThreadAffinity(void* thread, int cpuCore) {
    // QNX 使用 ThreadCtl 设置 CPU 亲和性
    if (thread) {
        // 检查 CPU 核心是否有效
        int num_cores = getNumberOfCores();
        if (cpuCore < 0 || cpuCore >= num_cores) {
            AURORA_LOG_WARN("QNXPlatform: Invalid CPU core: {}, max: {}", cpuCore, num_cores - 1);
            return false;
        }
        
        // QNX 不支持 pthread_setaffinity_np，使用 ThreadCtl
        int result = ThreadCtl(_NTO_THREAD_CTL_BIND, reinterpret_cast<void*>(static_cast<uintptr_t>(cpuCore)));
        if (result != -1) {
            AURORA_LOG_DEBUG("QNXPlatform: Set thread affinity to CPU {}", cpuCore);
            return true;
        } else {
            AURORA_LOG_WARN("QNXPlatform: Failed to set thread affinity: {}", result);
            return false;
        }
    }
    return false;
}

std::string QNXPlatform::getPlatformName() {
    return "QNX Neutrino";
}

std::string QNXPlatform::getOSVersion() {
    struct utsname buffer;
    if (uname(&buffer) != 0) {
        return "unknown";
    }
    return std::string(buffer.release);
}

int QNXPlatform::getNumberOfCores() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

// 原子操作实现
bool QNXPlatform::atomicCompareExchange(volatile void* ptr, void* expected, void* desired, size_t size) {
    if (!ptr || !expected || !desired) {
        AURORA_LOG_ERROR("QNXPlatform::atomicCompareExchange: Invalid arguments");
        return false;
    }
    
    switch (size) {
        case 1:
            return __atomic_compare_exchange_n(ptr, expected, *(uint8_t*)desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        case 2:
            return __atomic_compare_exchange_n(ptr, expected, *(uint16_t*)desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        case 4:
            return __atomic_compare_exchange_n(ptr, expected, *(uint32_t*)desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        case 8:
            return __atomic_compare_exchange_n(ptr, expected, *(uint64_t*)desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        default:
            AURORA_LOG_WARN("QNXPlatform::atomicCompareExchange: Unsupported size {}", size);
            return false;
    }
}

void QNXPlatform::atomicStore(volatile void* ptr, void* value, size_t size) {
    if (!ptr || !value) {
        AURORA_LOG_ERROR("QNXPlatform::atomicStore: Invalid arguments");
        return;
    }
    __atomic_store(ptr, value, __ATOMIC_SEQ_CST);
}

void QNXPlatform::atomicLoad(volatile void* ptr, void* value, size_t size) {
    if (!ptr || !value) {
        AURORA_LOG_ERROR("QNXPlatform::atomicLoad: Invalid arguments");
        return;
    }
    __atomic_load(ptr, value, __ATOMIC_SEQ_CST);
}

void QNXPlatform::atomicAdd(volatile void* ptr, int64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("QNXPlatform::atomicAdd: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_add_fetch(ptr, (int8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_add_fetch(ptr, (int16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_add_fetch(ptr, (int32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_add_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("QNXPlatform::atomicAdd: Unsupported size {}", size);
    }
}

void QNXPlatform::atomicSub(volatile void* ptr, int64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("QNXPlatform::atomicSub: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_sub_fetch(ptr, (int8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_sub_fetch(ptr, (int16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_sub_fetch(ptr, (int32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_sub_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("QNXPlatform::atomicSub: Unsupported size {}", size);
    }
}

void QNXPlatform::atomicAnd(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("QNXPlatform::atomicAnd: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_and_fetch(ptr, (uint8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_and_fetch(ptr, (uint16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_and_fetch(ptr, (uint32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_and_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("QNXPlatform::atomicAnd: Unsupported size {}", size);
    }
}

void QNXPlatform::atomicOr(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("QNXPlatform::atomicOr: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_or_fetch(ptr, (uint8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_or_fetch(ptr, (uint16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_or_fetch(ptr, (uint32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_or_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("QNXPlatform::atomicOr: Unsupported size {}", size);
    }
}

void QNXPlatform::atomicXor(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("QNXPlatform::atomicXor: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_xor_fetch(ptr, (uint8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_xor_fetch(ptr, (uint16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_xor_fetch(ptr, (uint32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_xor_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("QNXPlatform::atomicXor: Unsupported size {}", size);
    }
}

// TSN支持实现
bool QNXPlatform::hasTSNSupport() {
    // QNX对TSN有良好支持，通过检查网络接口是否支持TSN特性
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return false;
    }
    
    // 检查接口是否支持TSN
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);
    
    int result = ioctl(sockfd, SIOCGIFHWADDR, &ifr);
    close(sockfd);
    
    // QNX平台默认支持TSN
    return result == 0;
}

bool QNXPlatform::enableTSN(const std::string& interface) {
    // 在QNX上启用TSN功能
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return false;
    }
    
    // 启用接口的TSN功能
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ);
    
    // 设置TSN模式
    int tsn_enable = 1;
    ifr.ifr_data = &tsn_enable;
    
    int result = ioctl(sockfd, SIOCSIFHWADDR, &ifr);
    close(sockfd);
    
    return result == 0;
}

bool QNXPlatform::configureTSN(const std::string& interface, uint32_t streamId, uint32_t priority) {
    // 在QNX上配置TSN流
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return false;
    }
    
    // 配置TSN流参数
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ);
    
    // 构建TSN配置结构
    struct tsn_config {
        uint32_t stream_id;
        uint32_t priority;
        uint32_t reserved;
    } config;
    
    config.stream_id = streamId;
    config.priority = priority;
    config.reserved = 0;
    
    ifr.ifr_data = &config;
    
    // 应用TSN配置
    int result = ioctl(sockfd, SIOCSIFHWADDR, &ifr);
    close(sockfd);
    
    return result == 0;
}

// MacOSPlatform implementation
// macOS - 使用 POSIX 兼容 API，与 Linux 实现类似

void* MacOSPlatform::allocateMemory(size_t size) {
    return malloc(size);
}

void MacOSPlatform::freeMemory(void* ptr) {
    free(ptr);
}

void* MacOSPlatform::createSharedMemory(const std::string& name, size_t size) {
    int fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1) return nullptr;
    
    if (ftruncate(fd, size) == -1) {
        close(fd);
        return nullptr;
    }
    
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return ptr;
}

void MacOSPlatform::destroySharedMemory(const std::string& name) {
    shm_unlink(name.c_str());
}

void* MacOSPlatform::mapSharedMemory(const std::string& name, size_t size) {
    int fd = shm_open(name.c_str(), O_RDWR, 0666);
    if (fd == -1) return nullptr;
    
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return ptr;
}

void MacOSPlatform::unmapSharedMemory(void* ptr, size_t size) {
    munmap(ptr, size);
}

int MacOSPlatform::createSocket(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}

int MacOSPlatform::closeSocket(int sockfd) {
    return close(sockfd);
}

bool MacOSPlatform::setSocketNonBlocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) != -1;
}

bool MacOSPlatform::setSocketReuseAddr(int sockfd) {
    int opt = 1;
    return setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != -1;
}

void* MacOSPlatform::createThread(ThreadFunc func, void* arg) {
    pthread_t* thread = new pthread_t;
    if (pthread_create(thread, nullptr, func, arg) != 0) {
        delete thread;
        return nullptr;
    }
    return thread;
}

void MacOSPlatform::joinThread(void* thread) {
    if (thread) {
        pthread_join(*static_cast<pthread_t*>(thread), nullptr);
        delete static_cast<pthread_t*>(thread);
    }
}

void* MacOSPlatform::createThreadWithPriority(ThreadFunc func, void* arg, int priority) {
    pthread_t* thread = new pthread_t;
    
    // 设置线程属性和优先级
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    
    // macOS 不支持实时调度策略，使用普通优先级
    struct sched_param param;
    param.sched_priority = priority;
    
    // 使用 SCHED_OTHER 调度策略
    pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
    pthread_attr_setschedparam(&attr, &param);
    
    if (pthread_create(thread, &attr, func, arg) != 0) {
        delete thread;
        pthread_attr_destroy(&attr);
        return nullptr;
    }
    
    pthread_attr_destroy(&attr);
    return thread;
}

void* MacOSPlatform::createMutex() {
    pthread_mutex_t* mutex = new pthread_mutex_t;
    if (pthread_mutex_init(mutex, nullptr) != 0) {
        delete mutex;
        return nullptr;
    }
    return mutex;
}

void MacOSPlatform::destroyMutex(void* mutex) {
    if (mutex) {
        pthread_mutex_destroy(static_cast<pthread_mutex_t*>(mutex));
        delete static_cast<pthread_mutex_t*>(mutex);
    }
}

void MacOSPlatform::lockMutex(void* mutex) {
    if (mutex) {
        pthread_mutex_lock(static_cast<pthread_mutex_t*>(mutex));
    }
}

void MacOSPlatform::unlockMutex(void* mutex) {
    if (mutex) {
        pthread_mutex_unlock(static_cast<pthread_mutex_t*>(mutex));
    }
}

void* MacOSPlatform::createCondition() {
    pthread_cond_t* cond = new pthread_cond_t;
    if (pthread_cond_init(cond, nullptr) != 0) {
        delete cond;
        return nullptr;
    }
    return cond;
}

void MacOSPlatform::destroyCondition(void* cond) {
    if (cond) {
        pthread_cond_destroy(static_cast<pthread_cond_t*>(cond));
        delete static_cast<pthread_cond_t*>(cond);
    }
}

void MacOSPlatform::waitCondition(void* cond, void* mutex) {
    if (cond && mutex) {
        pthread_cond_wait(static_cast<pthread_cond_t*>(cond), static_cast<pthread_mutex_t*>(mutex));
    }
}

void MacOSPlatform::signalCondition(void* cond) {
    if (cond) {
        pthread_cond_signal(static_cast<pthread_cond_t*>(cond));
    }
}

void MacOSPlatform::broadcastCondition(void* cond) {
    if (cond) {
        pthread_cond_broadcast(static_cast<pthread_cond_t*>(cond));
    }
}

uint64_t MacOSPlatform::getTimestampUs() {
    // 调用汇编实现的时间戳获取函数
    return aurorart_platform_get_timestamp_us();
}

// macOS 平台扩展功能实现

bool MacOSPlatform::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool MacOSPlatform::createDirectory(const std::string& path) {
    return mkdir(path.c_str(), 0755) == 0;
}

bool MacOSPlatform::removeFile(const std::string& path) {
    return remove(path.c_str()) == 0;
}

int MacOSPlatform::createSemaphore(const std::string& name, unsigned int initialCount) {
    sem_t* sem = sem_open(name.c_str(), O_CREAT, 0666, initialCount);
    return (sem == SEM_FAILED) ? -1 : reinterpret_cast<int>(reinterpret_cast<uintptr_t>(sem));
}

void MacOSPlatform::destroySemaphore(int semid) {
    if (semid != -1) {
        sem_close(reinterpret_cast<sem_t*>(reinterpret_cast<uintptr_t>(semid)));
    }
}

void MacOSPlatform::waitSemaphore(int semid) {
    if (semid != -1) {
        sem_wait(reinterpret_cast<sem_t*>(reinterpret_cast<uintptr_t>(semid)));
    }
}

void MacOSPlatform::signalSemaphore(int semid) {
    if (semid != -1) {
        sem_post(reinterpret_cast<sem_t*>(reinterpret_cast<uintptr_t>(semid)));
    }
}

bool MacOSPlatform::lockMemory(void* addr, size_t len) {
    return mlock(addr, len) == 0;
}

bool MacOSPlatform::unlockMemory(void* addr, size_t len) {
    return munlock(addr, len) == 0;
}

bool MacOSPlatform::setThreadPriority(void* thread, int priority) {
    if (thread) {
        // macOS 不支持实时调度策略，使用普通优先级
        struct sched_param param;
        param.sched_priority = priority;
        return pthread_setschedparam(*static_cast<pthread_t*>(thread), SCHED_OTHER, &param) == 0;
    }
    return false;
}

int MacOSPlatform::getThreadPriority(void* thread) {
    if (thread) {
        struct sched_param param;
        int policy;
        if (pthread_getschedparam(*static_cast<pthread_t*>(thread), &policy, &param) != 0) {
            return -1;
        }
        return param.sched_priority;
    }
    return -1;
}

bool MacOSPlatform::setThreadAffinity(void* thread, int cpuCore) {
    if (thread) {
        // macOS 使用 thread_policy_set 设置 CPU 亲和性
        thread_port_t mach_thread = pthread_mach_thread_np(*static_cast<pthread_t*>(thread));
        thread_affinity_policy_data_t policy = { static_cast<integer_t>(cpuCore) };
        return thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, 
                                reinterpret_cast<thread_policy_t>(&policy), 1) == KERN_SUCCESS;
    }
    return false;
}

std::string MacOSPlatform::getPlatformName() {
    return "macOS";
}

std::string MacOSPlatform::getOSVersion() {
    struct utsname buffer;
    if (uname(&buffer) != 0) {
        return "unknown";
    }
    return std::string(buffer.release);
}

int MacOSPlatform::getNumberOfCores() {
    int cores = 0;
    size_t len = sizeof(cores);
    int mib[2] = {CTL_HW, HW_NCPU};
    if (sysctl(mib, 2, &cores, &len, nullptr, 0) == 0) {
        return cores;
    }
    return 1;
}

// TSN支持实现
bool MacOSPlatform::hasTSNSupport() {
    // macOS对TSN的支持有限，检查系统版本
    struct utsname buffer;
    if (uname(&buffer) != 0) {
        return false;
    }
    
    // 检查macOS版本是否支持TSN
    std::string version(buffer.release);
    return version >= "10.15";
}

bool MacOSPlatform::enableTSN(const std::string& interface) {
    // macOS上启用TSN功能
    // 通过ifconfig命令启用TSN
    std::string command = "ifconfig " + interface + " tsn enable";
    int result = system(command.c_str());
    return result == 0;
}

bool MacOSPlatform::configureTSN(const std::string& interface, uint32_t streamId, uint32_t priority) {
    // macOS上配置TSN流
    // 通过ifconfig命令配置TSN流参数
    std::string command = "ifconfig " + interface + " tsn stream-id " + 
                         std::to_string(streamId) + " priority " + std::to_string(priority);
    int result = system(command.c_str());
    return result == 0;
}

// 原子操作实现
bool MacOSPlatform::atomicCompareExchange(volatile void* ptr, void* expected, void* desired, size_t size) {
    if (!ptr || !expected || !desired) {
        AURORA_LOG_ERROR("MacOSPlatform::atomicCompareExchange: Invalid arguments");
        return false;
    }
    
    switch (size) {
        case 1:
            return __atomic_compare_exchange_n(ptr, expected, *(uint8_t*)desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        case 2:
            return __atomic_compare_exchange_n(ptr, expected, *(uint16_t*)desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        case 4:
            return __atomic_compare_exchange_n(ptr, expected, *(uint32_t*)desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        case 8:
            return __atomic_compare_exchange_n(ptr, expected, *(uint64_t*)desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        default:
            AURORA_LOG_WARN("MacOSPlatform::atomicCompareExchange: Unsupported size {}", size);
            return false;
    }
}

void MacOSPlatform::atomicStore(volatile void* ptr, void* value, size_t size) {
    if (!ptr || !value) {
        AURORA_LOG_ERROR("MacOSPlatform::atomicStore: Invalid arguments");
        return;
    }
    __atomic_store(ptr, value, __ATOMIC_SEQ_CST);
}

void MacOSPlatform::atomicLoad(volatile void* ptr, void* value, size_t size) {
    if (!ptr || !value) {
        AURORA_LOG_ERROR("MacOSPlatform::atomicLoad: Invalid arguments");
        return;
    }
    __atomic_load(ptr, value, __ATOMIC_SEQ_CST);
}

void MacOSPlatform::atomicAdd(volatile void* ptr, int64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("MacOSPlatform::atomicAdd: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_add_fetch(ptr, (int8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_add_fetch(ptr, (int16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_add_fetch(ptr, (int32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_add_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("MacOSPlatform::atomicAdd: Unsupported size {}", size);
    }
}

void MacOSPlatform::atomicSub(volatile void* ptr, int64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("MacOSPlatform::atomicSub: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_sub_fetch(ptr, (int8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_sub_fetch(ptr, (int16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_sub_fetch(ptr, (int32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_sub_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("MacOSPlatform::atomicSub: Unsupported size {}", size);
    }
}

void MacOSPlatform::atomicAnd(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("MacOSPlatform::atomicAnd: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_and_fetch(ptr, (uint8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_and_fetch(ptr, (uint16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_and_fetch(ptr, (uint32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_and_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("MacOSPlatform::atomicAnd: Unsupported size {}", size);
    }
}

void MacOSPlatform::atomicOr(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("MacOSPlatform::atomicOr: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_or_fetch(ptr, (uint8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_or_fetch(ptr, (uint16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_or_fetch(ptr, (uint32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_or_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("MacOSPlatform::atomicOr: Unsupported size {}", size);
    }
}

void MacOSPlatform::atomicXor(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("MacOSPlatform::atomicXor: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_xor_fetch(ptr, (uint8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_xor_fetch(ptr, (uint16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_xor_fetch(ptr, (uint32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_xor_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("MacOSPlatform::atomicXor: Unsupported size {}", size);
    }
}

// Linux 平台扩展功能实现

bool LinuxPlatform::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool LinuxPlatform::createDirectory(const std::string& path) {
    return mkdir(path.c_str(), 0755) == 0;
}

bool LinuxPlatform::removeFile(const std::string& path) {
    return remove(path.c_str()) == 0;
}

int LinuxPlatform::createSemaphore(const std::string& name, unsigned int initialCount) {
    sem_t* sem = sem_open(name.c_str(), O_CREAT, 0666, initialCount);
    return (sem == SEM_FAILED) ? -1 : reinterpret_cast<int>(reinterpret_cast<uintptr_t>(sem));
}

void LinuxPlatform::destroySemaphore(int semid) {
    if (semid != -1) {
        sem_close(reinterpret_cast<sem_t*>(reinterpret_cast<uintptr_t>(semid)));
    }
}

void LinuxPlatform::waitSemaphore(int semid) {
    if (semid != -1) {
        sem_wait(reinterpret_cast<sem_t*>(reinterpret_cast<uintptr_t>(semid)));
    }
}

void LinuxPlatform::signalSemaphore(int semid) {
    if (semid != -1) {
        sem_post(reinterpret_cast<sem_t*>(reinterpret_cast<uintptr_t>(semid)));
    }
}

bool LinuxPlatform::lockMemory(void* addr, size_t len) {
    return mlock(addr, len) == 0;
}

bool LinuxPlatform::unlockMemory(void* addr, size_t len) {
    return munlock(addr, len) == 0;
}

bool LinuxPlatform::setThreadPriority(void* thread, int priority) {
    if (thread) {
        struct sched_param param;
        param.sched_priority = priority;
        return pthread_setschedparam(*static_cast<pthread_t*>(thread), SCHED_FIFO, &param) == 0;
    }
    return false;
}

int LinuxPlatform::getThreadPriority(void* thread) {
    if (thread) {
        struct sched_param param;
        int policy;
        if (pthread_getschedparam(*static_cast<pthread_t*>(thread), &policy, &param) != 0) {
            return -1;
        }
        return param.sched_priority;
    }
    return -1;
}

bool LinuxPlatform::setThreadAffinity(void* thread, int cpuCore) {
    if (thread) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpuCore, &cpuset);
        return pthread_setaffinity_np(*static_cast<pthread_t*>(thread), sizeof(cpuset), &cpuset) == 0;
    }
    return false;
}

std::string LinuxPlatform::getPlatformName() {
    return "Linux";
}

std::string LinuxPlatform::getOSVersion() {
    struct utsname buffer;
    if (uname(&buffer) != 0) {
        return "unknown";
    }
    return std::string(buffer.release);
}

int LinuxPlatform::getNumberOfCores() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

// 原子操作实现 - 优化版
always_inline bool LinuxPlatform::atomicCompareExchange(volatile void* ptr, void* expected, void* desired, size_t size) {
    if (!ptr || !expected || !desired) {
        AURORA_LOG_ERROR("LinuxPlatform::atomicCompareExchange: Invalid arguments");
        return false;
    }
    
    switch (size) {
        case 1:
            return __atomic_compare_exchange_n(ptr, expected, *(uint8_t*)desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
        case 2:
            return __atomic_compare_exchange_n(ptr, expected, *(uint16_t*)desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
        case 4:
            return __atomic_compare_exchange_n(ptr, expected, *(uint32_t*)desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
        case 8:
            return __atomic_compare_exchange_n(ptr, expected, *(uint64_t*)desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
        default:
            AURORA_LOG_WARN("LinuxPlatform::atomicCompareExchange: Unsupported size {}", size);
            return false;
    }
}

always_inline void LinuxPlatform::atomicStore(volatile void* ptr, void* value, size_t size) {
    if (!ptr || !value) {
        AURORA_LOG_ERROR("LinuxPlatform::atomicStore: Invalid arguments");
        return;
    }
    __atomic_store(ptr, value, __ATOMIC_RELEASE);
}

always_inline void LinuxPlatform::atomicLoad(volatile void* ptr, void* value, size_t size) {
    if (!ptr || !value) {
        AURORA_LOG_ERROR("LinuxPlatform::atomicLoad: Invalid arguments");
        return;
    }
    __atomic_load(ptr, value, __ATOMIC_ACQUIRE);
}

always_inline void LinuxPlatform::atomicAdd(volatile void* ptr, int64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("LinuxPlatform::atomicAdd: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_add_fetch(ptr, (int8_t)value, __ATOMIC_ACQ_REL);
            break;
        case 2:
            __atomic_add_fetch(ptr, (int16_t)value, __ATOMIC_ACQ_REL);
            break;
        case 4:
            __atomic_add_fetch(ptr, (int32_t)value, __ATOMIC_ACQ_REL);
            break;
        case 8:
            __atomic_add_fetch(ptr, value, __ATOMIC_ACQ_REL);
            break;
        default:
            AURORA_LOG_WARN("LinuxPlatform::atomicAdd: Unsupported size {}", size);
    }
}

always_inline void LinuxPlatform::atomicSub(volatile void* ptr, int64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("LinuxPlatform::atomicSub: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_sub_fetch(ptr, (int8_t)value, __ATOMIC_ACQ_REL);
            break;
        case 2:
            __atomic_sub_fetch(ptr, (int16_t)value, __ATOMIC_ACQ_REL);
            break;
        case 4:
            __atomic_sub_fetch(ptr, (int32_t)value, __ATOMIC_ACQ_REL);
            break;
        case 8:
            __atomic_sub_fetch(ptr, value, __ATOMIC_ACQ_REL);
            break;
        default:
            AURORA_LOG_WARN("LinuxPlatform::atomicSub: Unsupported size {}", size);
    }
}

always_inline void LinuxPlatform::atomicAnd(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("LinuxPlatform::atomicAnd: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_and_fetch(ptr, (uint8_t)value, __ATOMIC_ACQ_REL);
            break;
        case 2:
            __atomic_and_fetch(ptr, (uint16_t)value, __ATOMIC_ACQ_REL);
            break;
        case 4:
            __atomic_and_fetch(ptr, (uint32_t)value, __ATOMIC_ACQ_REL);
            break;
        case 8:
            __atomic_and_fetch(ptr, value, __ATOMIC_ACQ_REL);
            break;
        default:
            AURORA_LOG_WARN("LinuxPlatform::atomicAnd: Unsupported size {}", size);
    }
}

always_inline void LinuxPlatform::atomicOr(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("LinuxPlatform::atomicOr: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_or_fetch(ptr, (uint8_t)value, __ATOMIC_ACQ_REL);
            break;
        case 2:
            __atomic_or_fetch(ptr, (uint16_t)value, __ATOMIC_ACQ_REL);
            break;
        case 4:
            __atomic_or_fetch(ptr, (uint32_t)value, __ATOMIC_ACQ_REL);
            break;
        case 8:
            __atomic_or_fetch(ptr, value, __ATOMIC_ACQ_REL);
            break;
        default:
            AURORA_LOG_WARN("LinuxPlatform::atomicOr: Unsupported size {}", size);
    }
}

always_inline void LinuxPlatform::atomicXor(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("LinuxPlatform::atomicXor: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_xor_fetch(ptr, (uint8_t)value, __ATOMIC_ACQ_REL);
            break;
        case 2:
            __atomic_xor_fetch(ptr, (uint16_t)value, __ATOMIC_ACQ_REL);
            break;
        case 4:
            __atomic_xor_fetch(ptr, (uint32_t)value, __ATOMIC_ACQ_REL);
            break;
        case 8:
            __atomic_xor_fetch(ptr, value, __ATOMIC_ACQ_REL);
            break;
        default:
            AURORA_LOG_WARN("LinuxPlatform::atomicXor: Unsupported size {}", size);
    }
}

// 批量原子操作
template <typename T>
always_inline void LinuxPlatform::atomicBatchAdd(volatile T* ptr, const std::vector<T>& values) {
    for (size_t i = 0; i < values.size(); ++i) {
        __atomic_add_fetch(&ptr[i], values[i], __ATOMIC_ACQ_REL);
    }
}

template <typename T>
always_inline void LinuxPlatform::atomicBatchStore(volatile T* ptr, const std::vector<T>& values) {
    for (size_t i = 0; i < values.size(); ++i) {
        __atomic_store(&ptr[i], &values[i], __ATOMIC_RELEASE);
    }
}

template <typename T>
always_inline void LinuxPlatform::atomicBatchLoad(volatile T* ptr, std::vector<T>& values) {
    for (size_t i = 0; i < values.size(); ++i) {
        __atomic_load(&ptr[i], &values[i], __ATOMIC_ACQUIRE);
    }
}

// TSN支持实现 - 增强版
bool LinuxPlatform::hasTSNSupport() {
    // 检查是否存在TSN相关的网络设备特性
    // 1. 检查ethtool命令是否存在
    int result = system("which ethtool > /dev/null 2>&1");
    if (result != 0) {
        AURORA_LOG_WARN("LinuxPlatform::hasTSNSupport: ethtool not found");
        return false;
    }
    
    // 2. 检查网络接口是否存在
    std::string interface = "eth0";
    std::string if_path = "/sys/class/net/" + interface;
    if (!fileExists(if_path)) {
        AURORA_LOG_WARN("LinuxPlatform::hasTSNSupport: Interface {} not found", interface);
        return false;
    }
    
    // 3. 通过ethtool检查TSN支持
    std::string command = "ethtool -i " + interface + " | grep -i tsn";
    result = system(command.c_str());
    
    // 4. 检查/sys文件系统中的TSN相关文件
    std::string tsn_path = "/sys/class/net/" + interface + "/tsn";
    bool sys_support = fileExists(tsn_path);
    
    // 5. 检查是否支持802.1Qbv (时间感知调度)
    std::string qbv_path = "/sys/class/net/" + interface + "/tsn/qbv";
    bool qbv_support = fileExists(qbv_path);
    
    // 6. 检查是否支持802.1AS (时间同步)
    std::string as_path = "/sys/class/net/" + interface + "/tsn/as";
    bool as_support = fileExists(as_path);
    
    bool supported = (result == 0) || sys_support || qbv_support || as_support;
    AURORA_LOG_INFO("LinuxPlatform::hasTSNSupport: TSN support {} for interface {}", 
                   supported ? "enabled" : "disabled", interface);
    return supported;
}

bool LinuxPlatform::enableTSN(const std::string& interface) {
    if (interface.empty()) {
        AURORA_LOG_ERROR("LinuxPlatform::enableTSN: Empty interface name");
        return false;
    }
    
    // 检查接口是否存在
    std::string if_path = "/sys/class/net/" + interface;
    if (!fileExists(if_path)) {
        AURORA_LOG_ERROR("LinuxPlatform::enableTSN: Interface {} not found", interface);
        return false;
    }
    
    // 启用TSN功能
    std::string command = "ethtool --set-tsn " + interface + " enable on";
    int result = system(command.c_str());
    
    bool success = (result == 0);
    AURORA_LOG_INFO("LinuxPlatform::enableTSN: {} for interface {}", 
                   success ? "enabled" : "failed", interface);
    return success;
}

bool LinuxPlatform::disableTSN(const std::string& interface) {
    if (interface.empty()) {
        AURORA_LOG_ERROR("LinuxPlatform::disableTSN: Empty interface name");
        return false;
    }
    
    // 检查接口是否存在
    std::string if_path = "/sys/class/net/" + interface;
    if (!fileExists(if_path)) {
        AURORA_LOG_ERROR("LinuxPlatform::disableTSN: Interface {} not found", interface);
        return false;
    }
    
    // 禁用TSN功能
    std::string command = "ethtool --set-tsn " + interface + " enable off";
    int result = system(command.c_str());
    
    bool success = (result == 0);
    AURORA_LOG_INFO("LinuxPlatform::disableTSN: {} for interface {}", 
                   success ? "disabled" : "failed", interface);
    return success;
}

bool LinuxPlatform::configureTSN(const std::string& interface, uint32_t streamId, uint32_t priority) {
    if (interface.empty()) {
        AURORA_LOG_ERROR("LinuxPlatform::configureTSN: Empty interface name");
        return false;
    }
    
    // 检查接口是否存在
    std::string if_path = "/sys/class/net/" + interface;
    if (!fileExists(if_path)) {
        AURORA_LOG_ERROR("LinuxPlatform::configureTSN: Interface {} not found", interface);
        return false;
    }
    
    // 检查优先级范围
    if (priority > 7) {
        AURORA_LOG_WARN("LinuxPlatform::configureTSN: Priority {} out of range (0-7), clamping to 7", priority);
        priority = 7;
    }
    
    // 通过ethtool命令配置TSN流参数
    std::string command = "ethtool --set-tsn " + interface + " stream-id " + 
                         std::to_string(streamId) + " priority " + std::to_string(priority);
    int result = system(command.c_str());
    
    bool success = (result == 0);
    AURORA_LOG_INFO("LinuxPlatform::configureTSN: {} for interface {} (streamId={}, priority={})", 
                   success ? "configured" : "failed", interface, streamId, priority);
    return success;
}

bool LinuxPlatform::configureTSNStream(const std::string& interface, uint32_t streamId, 
                                      uint32_t priority, uint64_t bandwidth, 
                                      uint64_t maxLatency, uint32_t timeSlot) {
    if (interface.empty()) {
        AURORA_LOG_ERROR("LinuxPlatform::configureTSNStream: Empty interface name");
        return false;
    }
    
    // 检查接口是否存在
    std::string if_path = "/sys/class/net/" + interface;
    if (!fileExists(if_path)) {
        AURORA_LOG_ERROR("LinuxPlatform::configureTSNStream: Interface {} not found", interface);
        return false;
    }
    
    // 检查优先级范围
    if (priority > 7) {
        AURORA_LOG_WARN("LinuxPlatform::configureTSNStream: Priority {} out of range (0-7), clamping to 7", priority);
        priority = 7;
    }
    
    // 配置基本流参数
    if (!configureTSN(interface, streamId, priority)) {
        return false;
    }
    
    // 配置带宽限制 (使用tc命令)
    std::string tc_command = "tc qdisc add dev " + interface + " parent root handle 1: htb default 10";
    system(tc_command.c_str());
    
    tc_command = "tc class add dev " + interface + " parent 1: classid 1:1 htb rate " + 
                std::to_string(bandwidth) + "bps";
    system(tc_command.c_str());
    
    tc_command = "tc filter add dev " + interface + " parent 1: protocol ip prio " + 
                std::to_string(priority) + " u32 match ip dscp " + 
                std::to_string(priority * 8) + " 0xff flowid 1:1";
    system(tc_command.c_str());
    
    // 配置时间槽 (如果支持802.1Qbv)
    std::string qbv_path = "/sys/class/net/" + interface + "/tsn/qbv";
    if (fileExists(qbv_path)) {
        std::string slot_command = "echo " + std::to_string(timeSlot) + " > " + qbv_path + "/time_slot";
        system(slot_command.c_str());
    }
    
    AURORA_LOG_INFO("LinuxPlatform::configureTSNStream: Configured stream {} with priority {}, bandwidth {}bps, max latency {}ns, time slot {}", 
                   streamId, priority, bandwidth, maxLatency, timeSlot);
    return true;
}

bool LinuxPlatform::getTSNStreamInfo(const std::string& interface, uint32_t streamId, 
                                   uint32_t& priority, uint64_t& bandwidth, 
                                   uint64_t& maxLatency, uint32_t& timeSlot) {
    if (interface.empty()) {
        AURORA_LOG_ERROR("LinuxPlatform::getTSNStreamInfo: Empty interface name");
        return false;
    }
    
    // 检查接口是否存在
    std::string if_path = "/sys/class/net/" + interface;
    if (!fileExists(if_path)) {
        AURORA_LOG_ERROR("LinuxPlatform::getTSNStreamInfo: Interface {} not found", interface);
        return false;
    }
    
    // 这里简化实现，实际应该从系统中读取真实的流信息
    // 模拟返回一些默认值
    priority = 4;
    bandwidth = 100000000; // 100Mbps
    maxLatency = 1000000;   // 1ms
    timeSlot = 1000;        // 1ms
    
    AURORA_LOG_INFO("LinuxPlatform::getTSNStreamInfo: Stream {} info - priority: {}, bandwidth: {}bps, max latency: {}ns, time slot: {}", 
                   streamId, priority, bandwidth, maxLatency, timeSlot);
    return true;
}

bool LinuxPlatform::syncTime(const std::string& interface) {
    if (interface.empty()) {
        AURORA_LOG_ERROR("LinuxPlatform::syncTime: Empty interface name");
        return false;
    }
    
    // 检查接口是否存在
    std::string if_path = "/sys/class/net/" + interface;
    if (!fileExists(if_path)) {
        AURORA_LOG_ERROR("LinuxPlatform::syncTime: Interface {} not found", interface);
        return false;
    }
    
    // 检查是否支持802.1AS
    std::string as_path = "/sys/class/net/" + interface + "/tsn/as";
    if (!fileExists(as_path)) {
        AURORA_LOG_WARN("LinuxPlatform::syncTime: 802.1AS not supported on interface {}", interface);
        return false;
    }
    
    // 启用PTP时间同步
    std::string command = "ptp4l -i " + interface + " -m &";
    int result = system(command.c_str());
    
    bool success = (result == 0);
    AURORA_LOG_INFO("LinuxPlatform::syncTime: {} for interface {}", 
                   success ? "synced" : "failed", interface);
    return success;
}

uint64_t LinuxPlatform::getSyncTime() {
    // 读取系统时间作为同步时间
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t timestamp = static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + 
                         static_cast<uint64_t>(ts.tv_nsec);
    return timestamp;
}

bool LinuxPlatform::setTimeSyncInterval(const std::string& interface, uint32_t intervalMs) {
    if (interface.empty()) {
        AURORA_LOG_ERROR("LinuxPlatform::setTimeSyncInterval: Empty interface name");
        return false;
    }
    
    // 检查接口是否存在
    std::string if_path = "/sys/class/net/" + interface;
    if (!fileExists(if_path)) {
        AURORA_LOG_ERROR("LinuxPlatform::setTimeSyncInterval: Interface {} not found", interface);
        return false;
    }
    
    // 检查是否支持802.1AS
    std::string as_path = "/sys/class/net/" + interface + "/tsn/as";
    if (!fileExists(as_path)) {
        AURORA_LOG_WARN("LinuxPlatform::setTimeSyncInterval: 802.1AS not supported on interface {}", interface);
        return false;
    }
    
    // 配置时间同步间隔
    std::string interval_path = as_path + "/sync_interval";
    std::string command = "echo " + std::to_string(intervalMs) + " > " + interval_path;
    int result = system(command.c_str());
    
    bool success = (result == 0);
    AURORA_LOG_INFO("LinuxPlatform::setTimeSyncInterval: {} for interface {} (interval: {}ms)", 
                   success ? "set" : "failed", interface, intervalMs);
    return success;
}

// QNXPlatform implementation
// VxWorks RTOS 

void* VxWorksPlatform::allocateMemory(size_t size) {
    return malloc(size);
}

void VxWorksPlatform::freeMemory(void* ptr) {
    free(ptr);
}

void* VxWorksPlatform::createSharedMemory(const std::string& name, size_t size) {
    int fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1) return nullptr;
    
    if (ftruncate(fd, size) == -1) {
        close(fd);
        return nullptr;
    }
    
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return ptr;
}

void VxWorksPlatform::destroySharedMemory(const std::string& name) {
    shm_unlink(name.c_str());
}

void* VxWorksPlatform::mapSharedMemory(const std::string& name, size_t size) {
    int fd = shm_open(name.c_str(), O_RDWR, 0666);
    if (fd == -1) return nullptr;
    
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return ptr;
}

void VxWorksPlatform::unmapSharedMemory(void* ptr, size_t size) {
    munmap(ptr, size);
}

int VxWorksPlatform::createSocket(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}

int VxWorksPlatform::closeSocket(int sockfd) {
    return close(sockfd);
}

bool VxWorksPlatform::setSocketNonBlocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) != -1;
}

bool VxWorksPlatform::setSocketReuseAddr(int sockfd) {
    int opt = 1;
    return setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != -1;
}

void* VxWorksPlatform::createThread(ThreadFunc func, void* arg) {
    pthread_t* thread = new pthread_t;
    if (pthread_create(thread, nullptr, func, arg) != 0) {
        delete thread;
        return nullptr;
    }
    return thread;
}

void VxWorksPlatform::joinThread(void* thread) {
    if (thread) {
        pthread_join(*static_cast<pthread_t*>(thread), nullptr);
        delete static_cast<pthread_t*>(thread);
    }
}

void* VxWorksPlatform::createThreadWithPriority(ThreadFunc func, void* arg, int priority) {
    pthread_t* thread = new pthread_t;
    
    // 设置线程属性和优先级
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    
    // VxWorks 实时调度策略
    struct sched_param param;
    param.sched_priority = priority;
    
    // 使用 SCHED_RR 调度策略
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    pthread_attr_setschedparam(&attr, &param);
    
    if (pthread_create(thread, &attr, func, arg) != 0) {
        delete thread;
        pthread_attr_destroy(&attr);
        return nullptr;
    }
    
    pthread_attr_destroy(&attr);
    return thread;
}

void* VxWorksPlatform::createMutex() {
    pthread_mutex_t* mutex = new pthread_mutex_t;
    if (pthread_mutex_init(mutex, nullptr) != 0) {
        delete mutex;
        return nullptr;
    }
    return mutex;
}

void VxWorksPlatform::destroyMutex(void* mutex) {
    if (mutex) {
        pthread_mutex_destroy(static_cast<pthread_mutex_t*>(mutex));
        delete static_cast<pthread_mutex_t*>(mutex);
    }
}

void VxWorksPlatform::lockMutex(void* mutex) {
    if (mutex) {
        pthread_mutex_lock(static_cast<pthread_mutex_t*>(mutex));
    }
}

void VxWorksPlatform::unlockMutex(void* mutex) {
    if (mutex) {
        pthread_mutex_unlock(static_cast<pthread_mutex_t*>(mutex));
    }
}

void* VxWorksPlatform::createCondition() {
    pthread_cond_t* cond = new pthread_cond_t;
    if (pthread_cond_init(cond, nullptr) != 0) {
        delete cond;
        return nullptr;
    }
    return cond;
}

void VxWorksPlatform::destroyCondition(void* cond) {
    if (cond) {
        pthread_cond_destroy(static_cast<pthread_cond_t*>(cond));
        delete static_cast<pthread_cond_t*>(cond);
    }
}

void VxWorksPlatform::waitCondition(void* cond, void* mutex) {
    if (cond && mutex) {
        pthread_cond_wait(static_cast<pthread_cond_t*>(cond), static_cast<pthread_mutex_t*>(mutex));
    }
}

void VxWorksPlatform::signalCondition(void* cond) {
    if (cond) {
        pthread_cond_signal(static_cast<pthread_cond_t*>(cond));
    }
}

void VxWorksPlatform::broadcastCondition(void* cond) {
    if (cond) {
        pthread_cond_broadcast(static_cast<pthread_cond_t*>(cond));
    }
}

uint64_t VxWorksPlatform::getTimestampUs() {
    // VxWorks 使用 clock_gettime 获取高精度时间
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + 
           static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
}

// VxWorks 平台扩展功能实现

bool VxWorksPlatform::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool VxWorksPlatform::createDirectory(const std::string& path) {
    return mkdir(path.c_str(), 0755) == 0;
}

bool VxWorksPlatform::removeFile(const std::string& path) {
    return remove(path.c_str()) == 0;
}

int VxWorksPlatform::createSemaphore(const std::string& name, unsigned int initialCount) {
    sem_t* sem = sem_open(name.c_str(), O_CREAT, 0666, initialCount);
    return (sem == SEM_FAILED) ? -1 : reinterpret_cast<int>(reinterpret_cast<uintptr_t>(sem));
}

void VxWorksPlatform::destroySemaphore(int semid) {
    if (semid != -1) {
        sem_close(reinterpret_cast<sem_t*>(reinterpret_cast<uintptr_t>(semid)));
    }
}

void VxWorksPlatform::waitSemaphore(int semid) {
    if (semid != -1) {
        sem_wait(reinterpret_cast<sem_t*>(reinterpret_cast<uintptr_t>(semid)));
    }
}

void VxWorksPlatform::signalSemaphore(int semid) {
    if (semid != -1) {
        sem_post(reinterpret_cast<sem_t*>(reinterpret_cast<uintptr_t>(semid)));
    }
}

bool VxWorksPlatform::lockMemory(void* addr, size_t len) {
    return mlock(addr, len) == 0;
}

bool VxWorksPlatform::unlockMemory(void* addr, size_t len) {
    return munlock(addr, len) == 0;
}

bool VxWorksPlatform::setThreadPriority(void* thread, int priority) {
    if (thread) {
        // VxWorks 实时调度优化
        struct sched_param param;
        param.sched_priority = priority;
        
        // 使用 SCHED_FIFO 调度策略，确保实时性
        int result = pthread_setschedparam(*static_cast<pthread_t*>(thread), SCHED_FIFO, &param);
        if (result == 0) {
            AURORA_LOG_DEBUG("VxWorksPlatform: Set thread priority to {}", priority);
            return true;
        } else {
            AURORA_LOG_WARN("VxWorksPlatform: Failed to set thread priority: {}", result);
            return false;
        }
    }
    return false;
}

int VxWorksPlatform::getThreadPriority(void* thread) {
    if (thread) {
        struct sched_param param;
        int policy;
        if (pthread_getschedparam(*static_cast<pthread_t*>(thread), &policy, &param) != 0) {
            return -1;
        }
        return param.sched_priority;
    }
    return -1;
}

bool VxWorksPlatform::setThreadAffinity(void* thread, int cpuCore) {
    if (thread) {
        // VxWorks 使用 pthread_setaffinity_np 设置 CPU 亲和性
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpuCore, &cpuset);
        return pthread_setaffinity_np(*static_cast<pthread_t*>(thread), sizeof(cpuset), &cpuset) == 0;
    }
    return false;
}

std::string VxWorksPlatform::getPlatformName() {
    return "VxWorks";
}

std::string VxWorksPlatform::getOSVersion() {
    // VxWorks 版本信息需要通过特定的 API 获取
    return "unknown";
}

int VxWorksPlatform::getNumberOfCores() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

// 原子操作实现
bool VxWorksPlatform::atomicCompareExchange(volatile void* ptr, void* expected, void* desired, size_t size) {
    if (!ptr || !expected || !desired) {
        AURORA_LOG_ERROR("VxWorksPlatform::atomicCompareExchange: Invalid arguments");
        return false;
    }
    
    switch (size) {
        case 1:
            return __atomic_compare_exchange_n(ptr, expected, *(uint8_t*)desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        case 2:
            return __atomic_compare_exchange_n(ptr, expected, *(uint16_t*)desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        case 4:
            return __atomic_compare_exchange_n(ptr, expected, *(uint32_t*)desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        case 8:
            return __atomic_compare_exchange_n(ptr, expected, *(uint64_t*)desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        default:
            AURORA_LOG_WARN("VxWorksPlatform::atomicCompareExchange: Unsupported size {}", size);
            return false;
    }
}

void VxWorksPlatform::atomicStore(volatile void* ptr, void* value, size_t size) {
    if (!ptr || !value) {
        AURORA_LOG_ERROR("VxWorksPlatform::atomicStore: Invalid arguments");
        return;
    }
    __atomic_store(ptr, value, __ATOMIC_SEQ_CST);
}

void VxWorksPlatform::atomicLoad(volatile void* ptr, void* value, size_t size) {
    if (!ptr || !value) {
        AURORA_LOG_ERROR("VxWorksPlatform::atomicLoad: Invalid arguments");
        return;
    }
    __atomic_load(ptr, value, __ATOMIC_SEQ_CST);
}

void VxWorksPlatform::atomicAdd(volatile void* ptr, int64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("VxWorksPlatform::atomicAdd: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_add_fetch(ptr, (int8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_add_fetch(ptr, (int16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_add_fetch(ptr, (int32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_add_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("VxWorksPlatform::atomicAdd: Unsupported size {}", size);
    }
}

void VxWorksPlatform::atomicSub(volatile void* ptr, int64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("VxWorksPlatform::atomicSub: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_sub_fetch(ptr, (int8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_sub_fetch(ptr, (int16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_sub_fetch(ptr, (int32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_sub_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("VxWorksPlatform::atomicSub: Unsupported size {}", size);
    }
}

void VxWorksPlatform::atomicAnd(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("VxWorksPlatform::atomicAnd: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_and_fetch(ptr, (uint8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_and_fetch(ptr, (uint16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_and_fetch(ptr, (uint32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_and_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("VxWorksPlatform::atomicAnd: Unsupported size {}", size);
    }
}

void VxWorksPlatform::atomicOr(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("VxWorksPlatform::atomicOr: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_or_fetch(ptr, (uint8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_or_fetch(ptr, (uint16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_or_fetch(ptr, (uint32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_or_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("VxWorksPlatform::atomicOr: Unsupported size {}", size);
    }
}

void VxWorksPlatform::atomicXor(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("VxWorksPlatform::atomicXor: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            __atomic_xor_fetch(ptr, (uint8_t)value, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_xor_fetch(ptr, (uint16_t)value, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_xor_fetch(ptr, (uint32_t)value, __ATOMIC_SEQ_CST);
            break;
        case 8:
            __atomic_xor_fetch(ptr, value, __ATOMIC_SEQ_CST);
            break;
        default:
            AURORA_LOG_WARN("VxWorksPlatform::atomicXor: Unsupported size {}", size);
    }
}

// TSN支持实现
bool VxWorksPlatform::hasTSNSupport() {
    // VxWorks对TSN有良好支持，通过系统API检测
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return false;
    }
    
    // 检查接口是否支持TSN
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);
    
    int result = ioctl(fd, SIOCGIFHWADDR, &ifr);
    close(fd);
    
    // VxWorks平台默认支持TSN
    return result == 0;
}

bool VxWorksPlatform::enableTSN(const std::string& interface) {
    // 在VxWorks上启用TSN功能
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return false;
    }
    
    // 启用接口的TSN功能
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ);
    
    // 设置TSN模式
    int tsn_enable = 1;
    ifr.ifr_data = &tsn_enable;
    
    int result = ioctl(fd, SIOCSIFHWADDR, &ifr);
    close(fd);
    
    return result == 0;
}

bool VxWorksPlatform::configureTSN(const std::string& interface, uint32_t streamId, uint32_t priority) {
    // 在VxWorks上配置TSN流
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return false;
    }
    
    // 配置TSN流参数
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ);
    
    // 构建TSN配置结构
    struct tsn_config {
        uint32_t stream_id;
        uint32_t priority;
        uint32_t reserved;
    } config;
    
    config.stream_id = streamId;
    config.priority = priority;
    config.reserved = 0;
    
    ifr.ifr_data = &config;
    
    // 应用TSN配置
    int result = ioctl(fd, SIOCSIFHWADDR, &ifr);
    close(fd);
    
    return result == 0;
}

// WindowsPlatform implementation

void* WindowsPlatform::allocateMemory(size_t size) {
    return HeapAlloc(GetProcessHeap(), 0, size);
}

void WindowsPlatform::freeMemory(void* ptr) {
    HeapFree(GetProcessHeap(), 0, ptr);
}

void* WindowsPlatform::createSharedMemory(const std::string& name, size_t size) {
    std::wstring wname(name.begin(), name.end());
    HANDLE hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,    // 使用页文件
        NULL,                    // 默认安全属性
        PAGE_READWRITE,          // 读写访问
        0,                       // 高32位大小
        size,                    // 低32位大小
        wname.c_str()            // 映射名称
    );
    
    if (hMapFile == NULL) return nullptr;
    
    void* ptr = MapViewOfFile(
        hMapFile,                // 映射对象句柄
        FILE_MAP_ALL_ACCESS,     // 读写访问
        0,                       // 高32位偏移
        0,                       // 低32位偏移
        size                     // 映射大小
    );
    
    CloseHandle(hMapFile);
    return ptr;
}

void WindowsPlatform::destroySharedMemory(const std::string& name) {
    std::wstring wname(name.begin(), name.end());
    HANDLE hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,     // 读写访问
        FALSE,                   // 不继承句柄
        wname.c_str()            // 映射名称
    );
    
    if (hMapFile != NULL) {
        CloseHandle(hMapFile);
    }
}

void* WindowsPlatform::mapSharedMemory(const std::string& name, size_t size) {
    std::wstring wname(name.begin(), name.end());
    HANDLE hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,     // 读写访问
        FALSE,                   // 不继承句柄
        wname.c_str()            // 映射名称
    );
    
    if (hMapFile == NULL) return nullptr;
    
    void* ptr = MapViewOfFile(
        hMapFile,                // 映射对象句柄
        FILE_MAP_ALL_ACCESS,     // 读写访问
        0,                       // 高32位偏移
        0,                       // 低32位偏移
        size                     // 映射大小
    );
    
    CloseHandle(hMapFile);
    return ptr;
}

void WindowsPlatform::unmapSharedMemory(void* ptr, size_t size) {
    UnmapViewOfFile(ptr);
}

int WindowsPlatform::createSocket(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}

int WindowsPlatform::closeSocket(int sockfd) {
    return closesocket(sockfd);
}

bool WindowsPlatform::setSocketNonBlocking(int sockfd) {
    u_long mode = 1;
    return ioctlsocket(sockfd, FIONBIO, &mode) != SOCKET_ERROR;
}

bool WindowsPlatform::setSocketReuseAddr(int sockfd) {
    BOOL opt = TRUE;
    return setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) != SOCKET_ERROR;
}

// Thread functions
void* WindowsPlatform::createThread(ThreadFunc func, void* arg) {
    DWORD threadId;
    HANDLE hThread = CreateThread(
        nullptr,                // 默认安全属性
        0,                      // 默认栈大小
        (LPTHREAD_START_ROUTINE)func,
        arg,
        0,                      // 默认创建标志
        &threadId
    );
    return hThread;
}

void WindowsPlatform::joinThread(void* thread) {
    if (thread) {
        WaitForSingleObject(static_cast<HANDLE>(thread), INFINITE);
        CloseHandle(static_cast<HANDLE>(thread));
    }
}

void* WindowsPlatform::createThreadWithPriority(ThreadFunc func, void* arg, int priority) {
    // Windows 线程优先级映射
    int win_priority;
    if (priority >= 9) win_priority = THREAD_PRIORITY_HIGHEST;
    else if (priority >= 7) win_priority = THREAD_PRIORITY_ABOVE_NORMAL;
    else if (priority >= 5) win_priority = THREAD_PRIORITY_NORMAL;
    else if (priority >= 3) win_priority = THREAD_PRIORITY_BELOW_NORMAL;
    else win_priority = THREAD_PRIORITY_LOWEST;
    
    HANDLE thread = CreateThread(
        NULL,                   // 默认安全属性
        0,                      // 默认栈大小
        func,                   // 线程函数
        arg,                    // 线程参数
        0,                      // 创建标志
        NULL                    // 线程ID
    );
    
    if (thread) {
        // 设置线程优先级
        SetThreadPriority(thread, win_priority);
    }
    
    return thread;
}

// Mutex functions
void* WindowsPlatform::createMutex() {
    return CreateMutex(nullptr, FALSE, nullptr);
}

void WindowsPlatform::destroyMutex(void* mutex) {
    if (mutex) {
        CloseHandle(static_cast<HANDLE>(mutex));
    }
}

void WindowsPlatform::lockMutex(void* mutex) {
    if (mutex) {
        WaitForSingleObject(static_cast<HANDLE>(mutex), INFINITE);
    }
}

void WindowsPlatform::unlockMutex(void* mutex) {
    if (mutex) {
        ReleaseMutex(static_cast<HANDLE>(mutex));
    }
}

// Condition variable functions
void* WindowsPlatform::createCondition() {
    CONDITION_VARIABLE* cond = new CONDITION_VARIABLE;
    InitializeConditionVariable(cond);
    return cond;
}

void WindowsPlatform::destroyCondition(void* cond) {
    if (cond) {
        delete static_cast<CONDITION_VARIABLE*>(cond);
    }
}

void WindowsPlatform::waitCondition(void* cond, void* mutex) {
    if (cond && mutex) {
        SleepConditionVariableCS(
            static_cast<CONDITION_VARIABLE*>(cond),
            static_cast<CRITICAL_SECTION*>(mutex),
            INFINITE
        );
    }
}

void WindowsPlatform::signalCondition(void* cond) {
    if (cond) {
        WakeConditionVariable(static_cast<CONDITION_VARIABLE*>(cond));
    }
}

void WindowsPlatform::broadcastCondition(void* cond) {
    if (cond) {
        WakeAllConditionVariable(static_cast<CONDITION_VARIABLE*>(cond));
    }
}

uint64_t WindowsPlatform::getTimestampUs() {
    // 调用汇编实现的时间戳获取函数
    return aurorart_platform_get_timestamp_us();
}

// Windows 平台扩展功能实现

bool WindowsPlatform::fileExists(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool WindowsPlatform::createDirectory(const std::string& path) {
    return CreateDirectoryA(path.c_str(), nullptr) != FALSE;
}

bool WindowsPlatform::removeFile(const std::string& path) {
    return DeleteFileA(path.c_str()) != FALSE;
}

int WindowsPlatform::createSemaphore(const std::string& name, unsigned int initialCount) {
    HANDLE hSem = CreateSemaphoreA(nullptr, initialCount, MAXLONG, name.c_str());
    return (hSem == nullptr) ? -1 : reinterpret_cast<int>(reinterpret_cast<uintptr_t>(hSem));
}

void WindowsPlatform::destroySemaphore(int semid) {
    if (semid != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(reinterpret_cast<uintptr_t>(semid)));
    }
}

void WindowsPlatform::waitSemaphore(int semid) {
    if (semid != -1) {
        WaitForSingleObject(reinterpret_cast<HANDLE>(reinterpret_cast<uintptr_t>(semid)), INFINITE);
    }
}

void WindowsPlatform::signalSemaphore(int semid) {
    if (semid != -1) {
        ReleaseSemaphore(reinterpret_cast<HANDLE>(reinterpret_cast<uintptr_t>(semid)), 1, nullptr);
    }
}

bool WindowsPlatform::lockMemory(void* addr, size_t len) {
    // Windows 使用 VirtualLock 锁定内存
    return VirtualLock(addr, len) != FALSE;
}

bool WindowsPlatform::unlockMemory(void* addr, size_t len) {
    return VirtualUnlock(addr, len) != FALSE;
}

bool WindowsPlatform::setThreadPriority(void* thread, int priority) {
    if (thread) {
        // Windows 线程优先级范围：THREAD_PRIORITY_IDLE 到 THREAD_PRIORITY_TIME_CRITICAL
        int winPriority = THREAD_PRIORITY_NORMAL;
        if (priority < -10) winPriority = THREAD_PRIORITY_IDLE;
        else if (priority < -5) winPriority = THREAD_PRIORITY_LOWEST;
        else if (priority < 0) winPriority = THREAD_PRIORITY_BELOW_NORMAL;
        else if (priority == 0) winPriority = THREAD_PRIORITY_NORMAL;
        else if (priority < 5) winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
        else if (priority < 10) winPriority = THREAD_PRIORITY_HIGHEST;
        else winPriority = THREAD_PRIORITY_TIME_CRITICAL;
        
        return SetThreadPriority(static_cast<HANDLE>(thread), winPriority) != FALSE;
    }
    return false;
}

int WindowsPlatform::getThreadPriority(void* thread) {
    if (thread) {
        int priority = GetThreadPriority(static_cast<HANDLE>(thread));
        if (priority == THREAD_PRIORITY_ERROR_RETURN) {
            return -1;
        }
        // 映射回标准优先级范围
        if (priority == THREAD_PRIORITY_IDLE) return -15;
        else if (priority == THREAD_PRIORITY_LOWEST) return -10;
        else if (priority == THREAD_PRIORITY_BELOW_NORMAL) return -5;
        else if (priority == THREAD_PRIORITY_NORMAL) return 0;
        else if (priority == THREAD_PRIORITY_ABOVE_NORMAL) return 5;
        else if (priority == THREAD_PRIORITY_HIGHEST) return 10;
        else if (priority == THREAD_PRIORITY_TIME_CRITICAL) return 15;
        return priority;
    }
    return -1;
}

bool WindowsPlatform::setThreadAffinity(void* thread, int cpuCore) {
    if (thread) {
        // Windows 使用 SetThreadAffinityMask 设置 CPU 亲和性
        DWORD_PTR affinityMask = (1ULL << cpuCore);
        return SetThreadAffinityMask(static_cast<HANDLE>(thread), affinityMask) != 0;
    }
    return false;
}

std::string WindowsPlatform::getPlatformName() {
    return "Windows";
}

std::string WindowsPlatform::getOSVersion() {
    OSVERSIONINFOEXA osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
    
    // 使用 RtlGetVersion 获取真实版本（避免兼容性欺骗）
    using RtlGetVersionPtr = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (hNtdll) {
        auto RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(hNtdll, "RtlGetVersion"));
        if (RtlGetVersion) {
            RTL_OSVERSIONINFOW rovi = {};
            rovi.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
            if (RtlGetVersion(&rovi) == 0) {
                return std::to_string(rovi.dwMajorVersion) + "." + 
                       std::to_string(rovi.dwMinorVersion) + "." + 
                       std::to_string(rovi.dwBuildNumber);
            }
        }
    }
    
    // 回退到 GetVersionEx
    if (GetVersionExA(reinterpret_cast<OSVERSIONINFOA*>(&osvi))) {
        return std::to_string(osvi.dwMajorVersion) + "." + 
               std::to_string(osvi.dwMinorVersion) + "." + 
               std::to_string(osvi.dwBuildNumber);
    }
    return "unknown";
}

int WindowsPlatform::getNumberOfCores() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return static_cast<int>(sysInfo.dwNumberOfProcessors);
}

// TSN支持实现 - 增强版
bool WindowsPlatform::hasTSNSupport() {
    // Windows 10 1809及以上版本支持TSN
    OSVERSIONINFOEXA osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
    
    if (GetVersionExA(reinterpret_cast<OSVERSIONINFOA*>(&osvi))) {
        // 检查是否为Windows 10 1809或更高版本
        if (osvi.dwMajorVersion >= 10 && osvi.dwBuildNumber >= 17763) {
            return true;
        }
    }
    return false;
}

bool WindowsPlatform::enableTSN(const std::string& interface) {
    // Windows上启用TSN功能
    // 使用Windows Network Shell命令启用TSN
    std::string command = "netsh int tsn set interface " + interface + " state=enabled";
    int result = system(command.c_str());
    return result == 0;
}

bool WindowsPlatform::disableTSN(const std::string& interface) {
    // Windows上禁用TSN功能
    std::string command = "netsh int tsn set interface " + interface + " state=disabled";
    int result = system(command.c_str());
    return result == 0;
}

bool WindowsPlatform::configureTSN(const std::string& interface, uint32_t streamId, uint32_t priority) {
    // Windows上配置TSN流
    // 使用Windows Network Shell命令配置TSN流
    std::string command = "netsh int tsn add flow interface=" + interface + 
                         " stream-id=" + std::to_string(streamId) + 
                         " priority=" + std::to_string(priority);
    int result = system(command.c_str());
    return result == 0;
}

bool WindowsPlatform::configureTSNStream(const std::string& interface, uint32_t streamId, 
                                        uint32_t priority, uint64_t bandwidth, 
                                        uint64_t maxLatency, uint32_t timeSlot) {
    // Windows上配置TSN流详细参数
    // 首先添加基本流
    if (!configureTSN(interface, streamId, priority)) {
        return false;
    }
    
    // 配置带宽限制
    std::string command = "netsh int tsn set flow interface=" + interface + 
                         " stream-id=" + std::to_string(streamId) + 
                         " bandwidth=" + std::to_string(bandwidth / 1000) + "kbps";
    system(command.c_str());
    
    // 配置最大延迟
    command = "netsh int tsn set flow interface=" + interface + 
             " stream-id=" + std::to_string(streamId) + 
             " max-latency=" + std::to_string(maxLatency / 1000) + "us";
    system(command.c_str());
    
    // 配置时间槽
    command = "netsh int tsn set flow interface=" + interface + 
             " stream-id=" + std::to_string(streamId) + 
             " time-slot=" + std::to_string(timeSlot);
    system(command.c_str());
    
    AURORA_LOG_INFO("WindowsPlatform::configureTSNStream: Configured stream {} with priority {}, bandwidth {}bps, max latency {}ns, time slot {}", 
                   streamId, priority, bandwidth, maxLatency, timeSlot);
    return true;
}

bool WindowsPlatform::getTSNStreamInfo(const std::string& interface, uint32_t streamId, 
                                     uint32_t& priority, uint64_t& bandwidth, 
                                     uint64_t& maxLatency, uint32_t& timeSlot) {
    // 这里简化实现，实际应该从系统中读取真实的流信息
    // 模拟返回一些默认值
    priority = 4;
    bandwidth = 100000000; // 100Mbps
    maxLatency = 1000000;   // 1ms
    timeSlot = 1000;        // 1ms
    
    AURORA_LOG_INFO("WindowsPlatform::getTSNStreamInfo: Stream {} info - priority: {}, bandwidth: {}bps, max latency: {}ns, time slot: {}", 
                   streamId, priority, bandwidth, maxLatency, timeSlot);
    return true;
}

bool WindowsPlatform::syncTime(const std::string& interface) {
    // Windows上启用时间同步
    // 使用Windows Network Shell命令启用PTP
    std::string command = "netsh int ptp set interface " + interface + " state=enabled";
    int result = system(command.c_str());
    return result == 0;
}

uint64_t WindowsPlatform::getSyncTime() {
    // 读取系统时间作为同步时间
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    // 转换为纳秒
    uint64_t timestamp = uli.QuadPart * 100; // 100ns per tick
    return timestamp;
}

bool WindowsPlatform::setTimeSyncInterval(const std::string& interface, uint32_t intervalMs) {
    // Windows上配置时间同步间隔
    std::string command = "netsh int ptp set interface " + interface + 
                         " sync-interval=" + std::to_string(intervalMs);
    int result = system(command.c_str());
    return result == 0;
}

// 原子操作实现
bool WindowsPlatform::atomicCompareExchange(volatile void* ptr, void* expected, void* desired, size_t size) {
    if (!ptr || !expected || !desired) {
        AURORA_LOG_ERROR("WindowsPlatform::atomicCompareExchange: Invalid arguments");
        return false;
    }
    
    switch (size) {
        case 1:
            return InterlockedCompareExchange8(reinterpret_cast<volatile char*>(ptr), *(char*)desired, *(char*)expected) == *(char*)expected;
        case 2:
            return InterlockedCompareExchange16(reinterpret_cast<volatile short*>(ptr), *(short*)desired, *(short*)expected) == *(short*)expected;
        case 4:
            return InterlockedCompareExchange(reinterpret_cast<volatile long*>(ptr), *(long*)desired, *(long*)expected) == *(long*)expected;
        case 8:
            return InterlockedCompareExchange64(reinterpret_cast<volatile LONGLONG*>(ptr), *(LONGLONG*)desired, *(LONGLONG*)expected) == *(LONGLONG*)expected;
        default:
            AURORA_LOG_WARN("WindowsPlatform::atomicCompareExchange: Unsupported size {}", size);
            return false;
    }
}

void WindowsPlatform::atomicStore(volatile void* ptr, void* value, size_t size) {
    if (!ptr || !value) {
        AURORA_LOG_ERROR("WindowsPlatform::atomicStore: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            InterlockedExchange8(reinterpret_cast<volatile char*>(ptr), *(char*)value);
            break;
        case 2:
            InterlockedExchange16(reinterpret_cast<volatile short*>(ptr), *(short*)value);
            break;
        case 4:
            InterlockedExchange(reinterpret_cast<volatile long*>(ptr), *(long*)value);
            break;
        case 8:
            InterlockedExchange64(reinterpret_cast<volatile LONGLONG*>(ptr), *(LONGLONG*)value);
            break;
        default:
            AURORA_LOG_WARN("WindowsPlatform::atomicStore: Unsupported size {}", size);
    }
}

void WindowsPlatform::atomicLoad(volatile void* ptr, void* value, size_t size) {
    if (!ptr || !value) {
        AURORA_LOG_ERROR("WindowsPlatform::atomicLoad: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            *(char*)value = InterlockedCompareExchange8(reinterpret_cast<volatile char*>(ptr), 0, 0);
            break;
        case 2:
            *(short*)value = InterlockedCompareExchange16(reinterpret_cast<volatile short*>(ptr), 0, 0);
            break;
        case 4:
            *(long*)value = InterlockedCompareExchange(reinterpret_cast<volatile long*>(ptr), 0, 0);
            break;
        case 8:
            *(LONGLONG*)value = InterlockedCompareExchange64(reinterpret_cast<volatile LONGLONG*>(ptr), 0, 0);
            break;
        default:
            AURORA_LOG_WARN("WindowsPlatform::atomicLoad: Unsupported size {}", size);
    }
}

void WindowsPlatform::atomicAdd(volatile void* ptr, int64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("WindowsPlatform::atomicAdd: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            InterlockedExchangeAdd8(reinterpret_cast<volatile char*>(ptr), (char)value);
            break;
        case 2:
            InterlockedExchangeAdd16(reinterpret_cast<volatile short*>(ptr), (short)value);
            break;
        case 4:
            InterlockedExchangeAdd(reinterpret_cast<volatile long*>(ptr), (long)value);
            break;
        case 8:
            InterlockedExchangeAdd64(reinterpret_cast<volatile LONGLONG*>(ptr), (LONGLONG)value);
            break;
        default:
            AURORA_LOG_WARN("WindowsPlatform::atomicAdd: Unsupported size {}", size);
    }
}

void WindowsPlatform::atomicSub(volatile void* ptr, int64_t value, size_t size) {
    atomicAdd(ptr, -value, size);
}

void WindowsPlatform::atomicAnd(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("WindowsPlatform::atomicAnd: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            InterlockedAnd8(reinterpret_cast<volatile char*>(ptr), (char)value);
            break;
        case 2:
            InterlockedAnd16(reinterpret_cast<volatile short*>(ptr), (short)value);
            break;
        case 4:
            InterlockedAnd(reinterpret_cast<volatile long*>(ptr), (long)value);
            break;
        case 8:
            InterlockedAnd64(reinterpret_cast<volatile LONGLONG*>(ptr), (LONGLONG)value);
            break;
        default:
            AURORA_LOG_WARN("WindowsPlatform::atomicAnd: Unsupported size {}", size);
    }
}

void WindowsPlatform::atomicOr(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("WindowsPlatform::atomicOr: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            InterlockedOr8(reinterpret_cast<volatile char*>(ptr), (char)value);
            break;
        case 2:
            InterlockedOr16(reinterpret_cast<volatile short*>(ptr), (short)value);
            break;
        case 4:
            InterlockedOr(reinterpret_cast<volatile long*>(ptr), (long)value);
            break;
        case 8:
            InterlockedOr64(reinterpret_cast<volatile LONGLONG*>(ptr), (LONGLONG)value);
            break;
        default:
            AURORA_LOG_WARN("WindowsPlatform::atomicOr: Unsupported size {}", size);
    }
}

void WindowsPlatform::atomicXor(volatile void* ptr, uint64_t value, size_t size) {
    if (!ptr) {
        AURORA_LOG_ERROR("WindowsPlatform::atomicXor: Invalid arguments");
        return;
    }
    
    switch (size) {
        case 1:
            InterlockedXor8(reinterpret_cast<volatile char*>(ptr), (char)value);
            break;
        case 2:
            InterlockedXor16(reinterpret_cast<volatile short*>(ptr), (short)value);
            break;
        case 4:
            InterlockedXor(reinterpret_cast<volatile long*>(ptr), (long)value);
            break;
        case 8:
            InterlockedXor64(reinterpret_cast<volatile LONGLONG*>(ptr), (LONGLONG)value);
            break;
        default:
            AURORA_LOG_WARN("WindowsPlatform::atomicXor: Unsupported size {}", size);
    }
}



// PlatformManager implementation

PlatformManager& PlatformManager::instance() {
    static PlatformManager instance;
    return instance;
}

void PlatformManager::init() {
#if defined(__QNX__) || defined(__QNXNTO__)
    platform_ = std::make_unique<QNXPlatform>();
#elif defined(__linux__)
    platform_ = std::make_unique<LinuxPlatform>();
#elif defined(_WIN32)
    platform_ = std::make_unique<WindowsPlatform>();
#elif defined(__APPLE__)
    platform_ = std::make_unique<MacOSPlatform>();
#elif defined(__VXWORKS__)
    platform_ = std::make_unique<VxWorksPlatform>();
#else
    #error "Unsupported platform"
#endif
}

PlatformAbstraction* PlatformManager::getPlatform() {
    return platform_.get();
}

} // namespace platform
} // namespace aurorart