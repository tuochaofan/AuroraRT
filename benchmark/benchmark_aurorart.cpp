#include <benchmark/benchmark.h>
#include <thread>
#include <vector>
#include "aurorart/platform/platform_abstraction.h"
#include "aurorart/memory/memory_manager.h"
#include "aurorart/transport/transport.h"
#include "aurorart/communication/communication_pattern.h"
#include "aurorart/scheduler/scheduler.h"
#include "aurorart/utils/performance.h"

// 性能测试基类
class AuroraRTBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) {
        // 初始化平台和内存管理器
        aurorart::platform::PlatformManager::instance().init();
        aurorart::memory::MemoryManager::instance().init();
        aurorart::transport::TransportManager::instance().init();
        aurorart::transport::TransportManager::instance().start();
        aurorart::scheduler::SchedulerManager::instance().init();
        aurorart::scheduler::SchedulerManager::instance().start();
    }
    
    void TearDown(const ::benchmark::State& state) {
        // 清理资源
        aurorart::scheduler::SchedulerManager::instance().stop();
        aurorart::transport::TransportManager::instance().stop();
        aurorart::memory::MemoryManager::instance().shutdown();
    }
};

// 内存分配性能测试
BENCHMARK_DEFINE_F(AuroraRTBenchmark, MemoryAllocation)(benchmark::State& state) {
    for (auto _ : state) {
        void* ptr = aurorart::memory::MemoryManager::instance().allocate(64);
        benchmark::DoNotOptimize(ptr);
        aurorart::memory::MemoryManager::instance().deallocate(ptr);
    }
}

// 共享内存分配性能测试
BENCHMARK_DEFINE_F(AuroraRTBenchmark, SharedMemoryAllocation)(benchmark::State& state) {
    for (auto _ : state) {
        void* ptr = aurorart::memory::MemoryManager::instance().allocateShared(1024);
        benchmark::DoNotOptimize(ptr);
        aurorart::memory::MemoryManager::instance().deallocateShared(ptr);
    }
}

// 进程内传输性能测试
BENCHMARK_DEFINE_F(AuroraRTBenchmark, IntraProcessTransport)(benchmark::State& state) {
    auto transport = aurorart::transport::TransportManager::instance().getTransport(
        aurorart::transport::TransportType::INTRA_PROCESS
    );
    
    int testData = 42;
    int receivedData = 0;
    
    for (auto _ : state) {
        transport->send(&testData, sizeof(testData));
        transport->receive(&receivedData, sizeof(receivedData));
        benchmark::DoNotOptimize(receivedData);
    }
}

// 发布-订阅模式性能测试
BENCHMARK_DEFINE_F(AuroraRTBenchmark, PubSubPattern)(benchmark::State& state) {
    auto pattern = aurorart::communication::CommunicationPatternFactory::createPattern(
        aurorart::communication::PatternType::PUB_SUB
    );
    pattern->init();
    pattern->start();
    
    auto publisher = dynamic_cast<aurorart::communication::PubSubPattern*>(pattern.get())->createPublisher<int>("test_topic");
    
    int receivedValue = 0;
    auto subscriber = dynamic_cast<aurorart::communication::PubSubPattern*>(pattern.get())->createSubscriber<int>("test_topic",
        [&](const int& value) {
            receivedValue = value;
        }
    );
    
    int testValue = 42;
    
    for (auto _ : state) {
        publisher->publish(testValue);
        // 等待消息处理
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        benchmark::DoNotOptimize(receivedValue);
    }
    
    pattern->stop();
}

// 调度器性能测试
BENCHMARK_DEFINE_F(AuroraRTBenchmark, SchedulerPerformance)(benchmark::State& state) {
    bool taskExecuted = false;
    
    for (auto _ : state) {
        auto taskId = aurorart::scheduler::SchedulerManager::instance().scheduleTask(
            [&]() {
                taskExecuted = true;
            }
        );
        // 等待任务执行
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        benchmark::DoNotOptimize(taskExecuted);
    }
}

// 内存拷贝性能测试
BENCHMARK_DEFINE_F(AuroraRTBenchmark, MemcpyOptimized)(benchmark::State& state) {
    const size_t size = 1024;
    char src[size] = "Test data for memcpy optimization";
    char dst[size] = {0};
    
    for (auto _ : state) {
        aurorart::utils::PerformanceUtils::memcpy_optimized(dst, src, size);
        benchmark::DoNotOptimize(dst);
    }
}

// 内存设置性能测试
BENCHMARK_DEFINE_F(AuroraRTBenchmark, MemsetOptimized)(benchmark::State& state) {
    const size_t size = 1024;
    char buffer[size] = {0};
    
    for (auto _ : state) {
        aurorart::utils::PerformanceUtils::memset_optimized(buffer, 'A', size - 1);
        benchmark::DoNotOptimize(buffer);
    }
}

// 哈希函数性能测试
BENCHMARK_DEFINE_F(AuroraRTBenchmark, HashOptimized)(benchmark::State& state) {
    const char* testData = "Hello, AuroraRT! This is a test string for hash performance.";
    size_t testSize = strlen(testData);
    
    for (auto _ : state) {
        uint32_t hash = aurorart::utils::PerformanceUtils::hash_optimized(testData, testSize);
        benchmark::DoNotOptimize(hash);
    }
}

// 并发性能测试
BENCHMARK_DEFINE_F(AuroraRTBenchmark, ConcurrentTasks)(benchmark::State& state) {
    const int numThreads = state.range(0);
    std::vector<std::thread> threads;
    std::atomic<int> counter(0);
    
    for (auto _ : state) {
        threads.clear();
        counter = 0;
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < 1000; ++j) {
                    counter.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        benchmark::DoNotOptimize(counter);
    }
}

// 注册性能测试
BENCHMARK_REGISTER_F(AuroraRTBenchmark, MemoryAllocation)->Iterations(10000);
BENCHMARK_REGISTER_F(AuroraRTBenchmark, SharedMemoryAllocation)->Iterations(1000);
BENCHMARK_REGISTER_F(AuroraRTBenchmark, IntraProcessTransport)->Iterations(1000);
BENCHMARK_REGISTER_F(AuroraRTBenchmark, PubSubPattern)->Iterations(100);
BENCHMARK_REGISTER_F(AuroraRTBenchmark, SchedulerPerformance)->Iterations(1000);
BENCHMARK_REGISTER_F(AuroraRTBenchmark, MemcpyOptimized)->Iterations(10000);
BENCHMARK_REGISTER_F(AuroraRTBenchmark, MemsetOptimized)->Iterations(10000);
BENCHMARK_REGISTER_F(AuroraRTBenchmark, HashOptimized)->Iterations(10000);
BENCHMARK_REGISTER_F(AuroraRTBenchmark, ConcurrentTasks)->Range(1, 16);

// 主函数
int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    ::benchmark::RunSpecifiedBenchmarks();
    return 0;
}
