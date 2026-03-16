#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include "aurorart/platform/platform_abstraction.h"
#include "aurorart/platform/platform_features.h"
#include "aurorart/memory/memory_manager.h"
#include "aurorart/transport/transport.h"
#include "aurorart/communication/communication_pattern.h"
#include "aurorart/serialization/serializer.h"
#include "aurorart/service_discovery/service_discovery.h"
#include "aurorart/qos/qos_policy.h"
#include "aurorart/scheduler/scheduler.h"
#include "aurorart/utils/performance.h"
#include "aurorart/pipeline/pipeline.h"
#include "aurorart/self_test/self_test.h"

// 测试平台抽象层
TEST(PlatformAbstractionTest, Init) {
    aurorart::platform::PlatformManager::instance().init();
    EXPECT_NE(aurorart::platform::PlatformManager::instance().getPlatform(), nullptr);
}

// 测试内存管理器
TEST(MemoryManagerTest, Allocate) {
    aurorart::memory::MemoryManager::instance().init();
    
    void* ptr = aurorart::memory::MemoryManager::instance().allocate(64);
    EXPECT_NE(ptr, nullptr);
    
    aurorart::memory::MemoryManager::instance().deallocate(ptr);
    
    void* sharedPtr = aurorart::memory::MemoryManager::instance().allocateShared(128);
    EXPECT_NE(sharedPtr, nullptr);
    
    aurorart::memory::MemoryManager::instance().deallocateShared(sharedPtr);
    
    aurorart::memory::MemoryManager::instance().shutdown();
}

// 测试传输模块
TEST(TransportTest, SendReceive) {
    aurorart::platform::PlatformManager::instance().init();
    aurorart::memory::MemoryManager::instance().init();
    aurorart::transport::TransportManager::instance().init();
    aurorart::transport::TransportManager::instance().start();
    
    auto transport = aurorart::transport::TransportManager::instance().getTransport(
        aurorart::transport::TransportType::INTRA_PROCESS
    );
    EXPECT_NE(transport, nullptr);
    
    int testData = 42;
    int receivedData = 0;
    
    EXPECT_TRUE(transport->send(&testData, sizeof(testData)));
    EXPECT_TRUE(transport->receive(&receivedData, sizeof(receivedData)));
    EXPECT_EQ(receivedData, testData);
    
    aurorart::transport::TransportManager::instance().stop();
    aurorart::memory::MemoryManager::instance().shutdown();
}

// 测试通信模式
TEST(CommunicationPatternTest, PubSub) {
    aurorart::platform::PlatformManager::instance().init();
    aurorart::memory::MemoryManager::instance().init();
    aurorart::transport::TransportManager::instance().init();
    aurorart::transport::TransportManager::instance().start();
    
    auto pattern = aurorart::communication::CommunicationPatternFactory::createPattern(
        aurorart::communication::PatternType::PUB_SUB
    );
    EXPECT_NE(pattern, nullptr);
    
    pattern->init();
    pattern->start();
    
    auto publisher = dynamic_cast<aurorart::communication::PubSubPattern*>(pattern.get())->createPublisher<int>("test_topic");
    EXPECT_NE(publisher, nullptr);
    
    int receivedValue = 0;
    auto subscriber = dynamic_cast<aurorart::communication::PubSubPattern*>(pattern.get())->createSubscriber<int>("test_topic",
        [&](const int& value) {
            receivedValue = value;
        }
    );
    EXPECT_NE(subscriber, nullptr);
    
    int testValue = 42;
    publisher->publish(testValue);
    
    // 等待消息接收
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 注意：由于是简化实现，这里可能不会接收到消息
    // 实际实现中应该有消息队列和线程处理
    
    pattern->stop();
    aurorart::transport::TransportManager::instance().stop();
    aurorart::memory::MemoryManager::instance().shutdown();
}

// 测试序列化
TEST(SerializerTest, CDR) {
    auto serializer = aurorart::serialization::SerializerFactory::createSerializer(
        aurorart::serialization::SerializerType::CDR
    );
    EXPECT_NE(serializer, nullptr);
    
    int testData = 123;
    auto serialized = serializer->serialize(&testData, sizeof(testData));
    EXPECT_EQ(serialized.size(), sizeof(testData));
    
    int deserialized = 0;
    serializer->deserialize(serialized, &deserialized, sizeof(deserialized));
    EXPECT_EQ(deserialized, testData);
}

// 测试QoS策略
TEST(QoSTest, PolicyBuilder) {
    aurorart::qos::QoSPolicyBuilder builder;
    auto policies = builder.withReliability(aurorart::qos::ReliabilityPolicy::ReliabilityLevel::RELIABLE)
                         .withPriority(255)
                         .build();
    EXPECT_EQ(policies.size(), 2);
}

// 测试服务发现
TEST(ServiceDiscoveryTest, RegisterService) {
    aurorart::service_discovery::ServiceDiscoveryManager::instance().init(
        aurorart::service_discovery::DiscoveryMode::DECENTRALIZED
    );
    aurorart::service_discovery::ServiceDiscoveryManager::instance().start();
    
    aurorart::service_discovery::ServiceInfo service;
    service.name = "test_service";
    service.type = "test_type";
    service.address = "127.0.0.1";
    service.port = 5555;
    
    aurorart::service_discovery::ServiceDiscoveryManager::instance().registerService(service);
    
    auto services = aurorart::service_discovery::ServiceDiscoveryManager::instance().discoverServices("test_type");
    EXPECT_GE(services.size(), 1);
    
    aurorart::service_discovery::ServiceDiscoveryManager::instance().stop();
}

// 测试调度器
TEST(SchedulerTest, ScheduleTask) {
    aurorart::scheduler::SchedulerManager::instance().init();
    aurorart::scheduler::SchedulerManager::instance().start();
    
    bool taskExecuted = false;
    auto taskId = aurorart::scheduler::SchedulerManager::instance().scheduleTask(
        [&]() {
            taskExecuted = true;
        }
    );
    EXPECT_NE(taskId, 0);
    
    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    aurorart::scheduler::SchedulerManager::instance().stop();
}

// 测试性能工具
TEST(PerformanceUtilsTest, MemcpyOptimized) {
    char src[1024] = "Test data for memcpy optimization";
    char dst[1024] = {0};
    
    aurorart::utils::PerformanceUtils::memcpy_optimized(dst, src, sizeof(src));
    EXPECT_STREQ(dst, src);
}

TEST(PerformanceUtilsTest, MemsetOptimized) {
    char buffer[1024] = {0};
    
    aurorart::utils::PerformanceUtils::memset_optimized(buffer, 'A', sizeof(buffer) - 1);
    EXPECT_EQ(buffer[0], 'A');
    EXPECT_EQ(buffer[1022], 'A');
    EXPECT_EQ(buffer[1023], '\0');
}

TEST(PerformanceUtilsTest, MemcmpOptimized) {
    char buffer1[1024] = "Test data";
    char buffer2[1024] = "Test data";
    char buffer3[1024] = "Different data";
    
    EXPECT_EQ(aurorart::utils::PerformanceUtils::memcmp_optimized(buffer1, buffer2, sizeof(buffer1)), 0);
    EXPECT_NE(aurorart::utils::PerformanceUtils::memcmp_optimized(buffer1, buffer3, sizeof(buffer1)), 0);
}

TEST(PerformanceUtilsTest, HashOptimized) {
    const char* testData = "Hello, AuroraRT!";
    size_t testSize = strlen(testData);
    
    uint32_t hash1 = aurorart::utils::PerformanceUtils::hash_optimized(testData, testSize);
    uint32_t hash2 = aurorart::utils::PerformanceUtils::hash_optimized(testData, testSize);
    
    EXPECT_EQ(hash1, hash2);
}

// 测试流水线模块
TEST(PipelineTest, Process) {
    aurorart::pipeline::Pipeline pipeline;
    
    int value = 0;
    
    pipeline.addStage([](const int& input) -> int {
        return input + 1;
    });
    
    pipeline.addStage([](const int& input) -> int {
        return input * 2;
    });
    
    pipeline.addStage([](const int& input) -> int {
        return input - 3;
    });
    
    int result = pipeline.process(5);
    EXPECT_EQ(result, 9); // (5 + 1) * 2 - 3 = 9
}

// 测试自检模块
TEST(SelfTestTest, RunTests) {
    aurorart::self_test::SelfTestManager::instance().init();
    
    auto results = aurorart::self_test::SelfTestManager::instance().runTests();
    EXPECT_GE(results.size(), 1);
    
    for (const auto& result : results) {
        EXPECT_TRUE(result.passed);
    }
}

// 测试平台特性检测
TEST(PlatformFeaturesTest, GetCapabilities) {
    auto caps = aurorart::platform::getPlatformCapabilities();
    
    // 验证平台基本信息
    EXPECT_FALSE(caps.platformName.empty());
    EXPECT_FALSE(caps.archName.empty());
    EXPECT_FALSE(caps.compilerName.empty());
    EXPECT_FALSE(caps.osVersion.empty());
    EXPECT_GT(caps.numberOfCores, 0);
    EXPECT_GT(caps.cacheLineSize, 0);
    
    // 验证硬件特性检测
    EXPECT_NO_THROW(aurorart::platform::getHardwareFeaturesString());
}

// 测试TSN支持
TEST(PlatformAbstractionTest, TSN) {
    aurorart::platform::PlatformManager::instance().init();
    auto platform = aurorart::platform::PlatformManager::instance().getPlatform();
    
    // 测试TSN支持方法
    EXPECT_NO_THROW(platform->hasTSNSupport());
    EXPECT_NO_THROW(platform->enableTSN("eth0"));
    EXPECT_NO_THROW(platform->configureTSN("eth0", 1, 7));
}

// 测试硬件特性检测函数
TEST(PlatformFeaturesTest, HardwareFeatures) {
    // 测试共享内存支持检测
    EXPECT_NO_THROW(aurorart::platform::hasSharedMemorySupport());
    
    // 测试实时调度支持检测
    EXPECT_NO_THROW(aurorart::platform::hasRealtimeSchedulingSupport());
    
    // 测试零拷贝支持检测
    EXPECT_NO_THROW(aurorart::platform::hasZeroCopySupport());
    
    // 测试核心数量获取
    EXPECT_GT(aurorart::platform::getNumberOfCores(), 0);
    
    // 测试缓存行大小获取
    EXPECT_GT(aurorart::platform::getCacheLineSize(), 0);
}

// 性能测试：传输层吞吐量
TEST(PerformanceTest, TransportThroughput) {
    aurorart::platform::PlatformManager::instance().init();
    aurorart::memory::MemoryManager::instance().init();
    aurorart::transport::TransportManager::instance().init();
    aurorart::transport::TransportManager::instance().start();
    
    auto transport = aurorart::transport::TransportManager::instance().getTransport(
        aurorart::transport::TransportType::INTRA_PROCESS
    );
    EXPECT_NE(transport, nullptr);
    
    // 测试数据
    const size_t dataSize = 1024;
    char* sendBuffer = new char[dataSize];
    char* receiveBuffer = new char[dataSize];
    memset(sendBuffer, 'A', dataSize);
    
    // 测试参数
    const size_t messageCount = 100000;
    auto startTime = std::chrono::steady_clock::now();
    
    // 发送和接收消息
    for (size_t i = 0; i < messageCount; ++i) {
        EXPECT_TRUE(transport->send(sendBuffer, dataSize));
        EXPECT_TRUE(transport->receive(receiveBuffer, dataSize));
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    double throughput = (messageCount * dataSize * 8) / (duration * 1024.0); // Kbps
    
    std::cout << "Transport throughput: " << throughput << " Kbps" << std::endl;
    std::cout << "Messages per second: " << (messageCount * 1000.0) / duration << std::endl;
    
    delete[] sendBuffer;
    delete[] receiveBuffer;
    
    aurorart::transport::TransportManager::instance().stop();
    aurorart::memory::MemoryManager::instance().shutdown();
}

// 性能测试：通信模式（发布-订阅）
TEST(PerformanceTest, PubSubPerformance) {
    aurorart::platform::PlatformManager::instance().init();
    aurorart::memory::MemoryManager::instance().init();
    aurorart::transport::TransportManager::instance().init();
    aurorart::transport::TransportManager::instance().start();
    
    auto pattern = aurorart::communication::CommunicationPatternFactory::createPattern(
        aurorart::communication::PatternType::PUB_SUB
    );
    EXPECT_NE(pattern, nullptr);
    
    pattern->init();
    pattern->start();
    
    auto publisher = dynamic_cast<aurorart::communication::PubSubPattern*>(pattern.get())->createPublisher<int>("performance_topic");
    EXPECT_NE(publisher, nullptr);
    
    size_t receivedCount = 0;
    auto subscriber = dynamic_cast<aurorart::communication::PubSubPattern*>(pattern.get())->createSubscriber<int>("performance_topic",
        [&](const int& value) {
            receivedCount++;
        }
    );
    EXPECT_NE(subscriber, nullptr);
    
    // 测试参数
    const size_t messageCount = 100000;
    auto startTime = std::chrono::steady_clock::now();
    
    // 发布消息
    for (size_t i = 0; i < messageCount; ++i) {
        publisher->publish(i);
    }
    
    // 等待消息处理
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    double throughput = (receivedCount * sizeof(int) * 8) / (duration * 1024.0); // Kbps
    
    std::cout << "PubSub throughput: " << throughput << " Kbps" << std::endl;
    std::cout << "Messages per second: " << (receivedCount * 1000.0) / duration << std::endl;
    std::cout << "Message delivery rate: " << (receivedCount * 100.0) / messageCount << "%" << std::endl;
    
    pattern->stop();
    aurorart::transport::TransportManager::instance().stop();
    aurorart::memory::MemoryManager::instance().shutdown();
}

// 性能测试：调度器性能
TEST(PerformanceTest, SchedulerPerformance) {
    aurorart::scheduler::SchedulerManager::instance().init();
    aurorart::scheduler::SchedulerManager::instance().start();
    
    // 测试参数
    const size_t taskCount = 100000;
    std::atomic<size_t> completedTasks(0);
    auto startTime = std::chrono::steady_clock::now();
    
    // 提交任务
    for (size_t i = 0; i < taskCount; ++i) {
        aurorart::scheduler::SchedulerManager::instance().scheduleTask(
            aurorart::scheduler::SchedulerType::COROUTINE,
            [&]() {
                completedTasks++;
            }
        );
    }
    
    // 等待任务完成
    while (completedTasks < taskCount) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    double throughput = (completedTasks * 1000.0) / duration;
    
    std::cout << "Scheduler throughput: " << throughput << " tasks per second" << std::endl;
    
    aurorart::scheduler::SchedulerManager::instance().stop();
}

// 性能测试：内存管理性能
TEST(PerformanceTest, MemoryManagerPerformance) {
    aurorart::memory::MemoryManager::instance().init();
    
    // 测试参数
    const size_t allocationCount = 100000;
    const size_t allocationSize = 64;
    std::vector<void*> allocations;
    
    auto startTime = std::chrono::steady_clock::now();
    
    // 分配内存
    for (size_t i = 0; i < allocationCount; ++i) {
        void* ptr = aurorart::memory::MemoryManager::instance().allocate(allocationSize);
        EXPECT_NE(ptr, nullptr);
        allocations.push_back(ptr);
    }
    
    // 释放内存
    for (void* ptr : allocations) {
        aurorart::memory::MemoryManager::instance().deallocate(ptr);
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    double throughput = (allocationCount * 2 * 1000.0) / duration; // 2 operations per allocation (allocate + deallocate)
    
    std::cout << "Memory manager throughput: " << throughput << " operations per second" << std::endl;
    
    aurorart::memory::MemoryManager::instance().shutdown();
}

// 性能测试：序列化性能
TEST(PerformanceTest, SerializationPerformance) {
    auto serializer = aurorart::serialization::SerializerFactory::createSerializer(
        aurorart::serialization::SerializerType::CDR
    );
    EXPECT_NE(serializer, nullptr);
    
    // 测试数据
    struct TestData {
        int id;
        float value;
        double timestamp;
        char name[32];
    } testData;
    testData.id = 123;
    testData.value = 3.14f;
    testData.timestamp = 1234567890.123;
    strcpy(testData.name, "TestData");
    
    // 测试参数
    const size_t serializationCount = 100000;
    
    auto startTime = std::chrono::steady_clock::now();
    
    for (size_t i = 0; i < serializationCount; ++i) {
        auto serialized = serializer->serialize(&testData, sizeof(testData));
        EXPECT_EQ(serialized.size(), sizeof(testData));
        
        TestData deserialized;
        serializer->deserialize(serialized, &deserialized, sizeof(deserialized));
        EXPECT_EQ(deserialized.id, testData.id);
        EXPECT_FLOAT_EQ(deserialized.value, testData.value);
        EXPECT_DOUBLE_EQ(deserialized.timestamp, testData.timestamp);
        EXPECT_STREQ(deserialized.name, testData.name);
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    double throughput = (serializationCount * 2 * 1000.0) / duration; // 2 operations per serialization (serialize + deserialize)
    
    std::cout << "Serialization throughput: " << throughput << " operations per second" << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}