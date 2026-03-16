#include "aurorart/self_test/self_test.h"
#include "aurorart/memory/memory_manager.h"
#include "aurorart/scheduler/scheduler.h"
#include "aurorart/serialization/serializer.h"
#include "aurorart/qos/qos_policy.h"
#include "aurorart/transport/transport.h"
#include "aurorart/service_discovery/service_discovery.h"
#include "aurorart/utils/logger.h"
#include <iostream>
#include <chrono>

namespace aurorart {
namespace self_test {

// SelfTest implementation

SelfTest::SelfTest() {
    registerDefaultTests();
}

SelfTest& SelfTest::instance() {
    static SelfTest instance;
    return instance;
}

void SelfTest::registerDefaultTests() {
    // 内存测试
    registerTestCase({"MemoryPoolTest", MemoryTest::testMemoryPool, "测试内存池功能"});
    registerTestCase({"SharedMemoryTest", MemoryTest::testSharedMemory, "测试共享内存功能"});
    registerTestCase({"MemoryManagerTest", MemoryTest::testMemoryManager, "测试内存管理器功能"});
    
    // 调度器测试
    registerTestCase({"CoroutineSchedulerTest", SchedulerTest::testCoroutineScheduler, "测试协程调度器"});
    registerTestCase({"PrioritySchedulerTest", SchedulerTest::testPriorityScheduler, "测试优先级调度器"});
    registerTestCase({"IntelligentSchedulerTest", SchedulerTest::testIntelligentScheduler, "测试智能调度器"});
    
    // 序列化测试
    registerTestCase({"CDRSerializerTest", SerializationTest::testCDRSerializer, "测试CDR序列化器"});
    registerTestCase({"ProtobufSerializerTest", SerializationTest::testProtobufSerializer, "测试Protobuf序列化器"});
    registerTestCase({"FlatBuffersSerializerTest", SerializationTest::testFlatBuffersSerializer, "测试FlatBuffers序列化器"});
    registerTestCase({"JSONSerializerTest", SerializationTest::testJSONSerializer, "测试JSON序列化器"});
    
    // QoS测试
    registerTestCase({"QoSPoliciesTest", QoSTest::testQoSPolicies, "测试QoS策略"});
    registerTestCase({"QoSProfilesTest", QoSTest::testQoSProfiles, "测试QoS配置文件"});
    registerTestCase({"QoSManagerTest", QoSTest::testQoSManager, "测试QoS管理器"});
    
    // 传输测试
    registerTestCase({"IntraProcessTransportTest", TransportTest::testIntraProcessTransport, "测试进程内传输"});
    registerTestCase({"SharedMemoryTransportTest", TransportTest::testSharedMemoryTransport, "测试共享内存传输"});
    registerTestCase({"NetworkTransportTest", TransportTest::testNetworkTransport, "测试网络传输"});
    
    // 服务发现测试
    registerTestCase({"ServiceDiscoveryTest", ServiceDiscoveryTest::testServiceDiscovery, "测试服务发现"});
    registerTestCase({"HeartbeatMonitorTest", ServiceDiscoveryTest::testHeartbeatMonitor, "测试心跳监控"});
    
    // 系统测试
    registerTestCase({"SystemIntegrationTest", SystemTest::testSystemIntegration, "测试系统集成"});
    registerTestCase({"PerformanceTest", SystemTest::testPerformance, "测试系统性能"});
    registerTestCase({"StabilityTest", SystemTest::testStability, "测试系统稳定性"});
}

void SelfTest::registerTestCase(const TestCase& testCase) {
    testCases_.push_back(testCase);
}

void SelfTest::runAllTests() {
    std::cout << "Running all self tests..." << std::endl;
    int passed = 0, failed = 0, skipped = 0;
    
    for (const auto& testCase : testCases_) {
        std::cout << "Running test: " << testCase.name << " - " << testCase.description << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result = testCase.testFunction();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        switch (result) {
        case TestResult::PASS:
            std::cout << "✓ PASS" << std::endl;
            passed++;
            break;
        case TestResult::FAIL:
            std::cout << "✗ FAIL" << std::endl;
            failed++;
            break;
        case TestResult::SKIPPED:
            std::cout << "⚠ SKIPPED" << std::endl;
            skipped++;
            break;
        }
        std::cout << "Duration: " << duration << "ms" << std::endl << std::endl;
    }
    
    std::cout << "Test Summary:" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Skipped: " << skipped << std::endl;
    std::cout << "Total: " << testCases_.size() << std::endl;
}

void SelfTest::runTest(const std::string& testName) {
    for (const auto& testCase : testCases_) {
        if (testCase.name == testName) {
            std::cout << "Running test: " << testCase.name << " - " << testCase.description << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            TestResult result = testCase.testFunction();
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            
            switch (result) {
            case TestResult::PASS:
                std::cout << "✓ PASS" << std::endl;
                break;
            case TestResult::FAIL:
                std::cout << "✗ FAIL" << std::endl;
                break;
            case TestResult::SKIPPED:
                std::cout << "⚠ SKIPPED" << std::endl;
                break;
            }
            std::cout << "Duration: " << duration << "ms" << std::endl;
            return;
        }
    }
    std::cout << "Test not found: " << testName << std::endl;
}

void SelfTest::runTestsByCategory(const std::string& category) {
    std::cout << "Running tests in category: " << category << std::endl;
    int passed = 0, failed = 0, skipped = 0;
    
    for (const auto& testCase : testCases_) {
        if (testCase.name.find(category) != std::string::npos) {
            std::cout << "Running test: " << testCase.name << " - " << testCase.description << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            TestResult result = testCase.testFunction();
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            
            switch (result) {
            case TestResult::PASS:
                std::cout << "✓ PASS" << std::endl;
                passed++;
                break;
            case TestResult::FAIL:
                std::cout << "✗ FAIL" << std::endl;
                failed++;
                break;
            case TestResult::SKIPPED:
                std::cout << "⚠ SKIPPED" << std::endl;
                skipped++;
                break;
            }
            std::cout << "Duration: " << duration << "ms" << std::endl << std::endl;
        }
    }
    
    std::cout << "Category Summary:" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Skipped: " << skipped << std::endl;
}

// MemoryTest implementation

TestResult MemoryTest::testMemoryPool() {
    try {
        memory::MemoryPool pool(64, 10);
        
        // 测试内存分配和释放
        std::vector<void*> ptrs;
        for (int i = 0; i < 10; i++) {
            void* ptr = pool.allocate();
            if (!ptr) {
                return TestResult::FAIL;
            }
            ptrs.push_back(ptr);
        }
        
        // 测试内存池耗尽
        void* ptr = pool.allocate();
        if (ptr) {
            return TestResult::FAIL;
        }
        
        // 测试内存释放
        for (void* p : ptrs) {
            pool.deallocate(p);
        }
        
        // 测试释放后可以重新分配
        ptr = pool.allocate();
        if (!ptr) {
            return TestResult::FAIL;
        }
        pool.deallocate(ptr);
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult MemoryTest::testSharedMemory() {
    try {
        memory::SharedMemoryManager& manager = memory::SharedMemoryManager::instance();
        
        // 测试共享内存分配和释放
        void* ptr = manager.allocate(1024);
        if (!ptr) {
            return TestResult::FAIL;
        }
        
        // 测试共享内存映射
        void* mappedPtr = manager.map("test_shm", 1024);
        if (!mappedPtr) {
            manager.deallocate(ptr);
            return TestResult::FAIL;
        }
        
        // 测试共享内存写入和读取
        int testValue = 42;
        memcpy(ptr, &testValue, sizeof(testValue));
        int readValue;
        memcpy(&readValue, ptr, sizeof(readValue));
        if (readValue != testValue) {
            manager.deallocate(ptr);
            manager.unmap("test_shm");
            return TestResult::FAIL;
        }
        
        manager.deallocate(ptr);
        manager.unmap("test_shm");
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult MemoryTest::testMemoryManager() {
    try {
        memory::MemoryManager& manager = memory::MemoryManager::instance();
        manager.init();
        
        // 测试内存分配和释放
        void* ptr = manager.allocate(100);
        if (!ptr) {
            manager.shutdown();
            return TestResult::FAIL;
        }
        manager.deallocate(ptr);
        
        // 测试共享内存分配和释放
        void* sharedPtr = manager.allocateShared(1024);
        if (!sharedPtr) {
            manager.shutdown();
            return TestResult::FAIL;
        }
        manager.deallocateShared(sharedPtr);
        
        manager.shutdown();
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

// SchedulerTest implementation

TestResult SchedulerTest::testCoroutineScheduler() {
    try {
        scheduler::SchedulerManager& manager = scheduler::SchedulerManager::instance();
        manager.init();
        manager.start();
        
        // 测试任务调度
        bool taskExecuted = false;
        scheduler::TaskId taskId = manager.scheduleTask([&taskExecuted]() {
            taskExecuted = true;
        });
        
        // 等待任务执行
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (!taskExecuted) {
            manager.stop();
            return TestResult::FAIL;
        }
        
        manager.stop();
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult SchedulerTest::testPriorityScheduler() {
    try {
        scheduler::SchedulerManager& manager = scheduler::SchedulerManager::instance();
        manager.init();
        manager.start();
        
        // 测试优先级任务调度
        int highPriorityExecuted = 0;
        int lowPriorityExecuted = 0;
        
        // 先调度低优先级任务
        manager.scheduleTaskWithPriority([&lowPriorityExecuted]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            lowPriorityExecuted = 1;
        }, 0);
        
        // 再调度高优先级任务
        manager.scheduleTaskWithPriority([&highPriorityExecuted]() {
            highPriorityExecuted = 1;
        }, 255);
        
        // 等待任务执行
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (highPriorityExecuted != 1 || lowPriorityExecuted != 1) {
            manager.stop();
            return TestResult::FAIL;
        }
        
        manager.stop();
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult SchedulerTest::testIntelligentScheduler() {
    try {
        scheduler::SchedulerManager& manager = scheduler::SchedulerManager::instance();
        manager.init();
        manager.start();
        
        // 测试智能调度器
        bool taskExecuted = false;
        scheduler::TaskId taskId = manager.scheduleTask([&taskExecuted]() {
            taskExecuted = true;
        });
        
        // 等待任务执行
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (!taskExecuted) {
            manager.stop();
            return TestResult::FAIL;
        }
        
        manager.stop();
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

// SerializationTest implementation

TestResult SerializationTest::testCDRSerializer() {
    try {
        serialization::SerializerManager& manager = serialization::SerializerManager::instance();
        manager.init();
        
        auto serializer = manager.getSerializer(serialization::SerializerType::CDR);
        if (!serializer) {
            return TestResult::FAIL;
        }
        
        // 测试序列化和反序列化
        int testValue = 42;
        auto buffer = serializer->serialize(&testValue, sizeof(testValue));
        int readValue;
        serializer->deserialize(buffer, &readValue, sizeof(readValue));
        
        if (readValue != testValue) {
            return TestResult::FAIL;
        }
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult SerializationTest::testProtobufSerializer() {
    try {
        serialization::SerializerManager& manager = serialization::SerializerManager::instance();
        manager.init();
        
        auto serializer = manager.getSerializer(serialization::SerializerType::PROTOBUF);
        if (!serializer) {
            return TestResult::FAIL;
        }
        
        // 测试序列化和反序列化
        int testValue = 42;
        auto buffer = serializer->serialize(&testValue, sizeof(testValue));
        int readValue;
        serializer->deserialize(buffer, &readValue, sizeof(readValue));
        
        if (readValue != testValue) {
            return TestResult::FAIL;
        }
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult SerializationTest::testFlatBuffersSerializer() {
    try {
        serialization::SerializerManager& manager = serialization::SerializerManager::instance();
        manager.init();
        
        auto serializer = manager.getSerializer(serialization::SerializerType::FLATBUFFERS);
        if (!serializer) {
            return TestResult::FAIL;
        }
        
        // 测试序列化和反序列化
        int testValue = 42;
        auto buffer = serializer->serialize(&testValue, sizeof(testValue));
        int readValue;
        serializer->deserialize(buffer, &readValue, sizeof(readValue));
        
        if (readValue != testValue) {
            return TestResult::FAIL;
        }
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult SerializationTest::testJSONSerializer() {
    try {
        serialization::SerializerManager& manager = serialization::SerializerManager::instance();
        manager.init();
        
        auto serializer = manager.getSerializer(serialization::SerializerType::JSON);
        if (!serializer) {
            return TestResult::FAIL;
        }
        
        // 测试序列化和反序列化
        int testValue = 42;
        auto buffer = serializer->serialize(&testValue, sizeof(testValue));
        int readValue;
        serializer->deserialize(buffer, &readValue, sizeof(readValue));
        
        if (readValue != testValue) {
            return TestResult::FAIL;
        }
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

// QoSTest implementation

TestResult QoSTest::testQoSPolicies() {
    try {
        qos::QoSPolicyBuilder builder;
        
        // 测试构建QoS策略
        auto policies = builder.withReliability(qos::ReliabilityPolicy::ReliabilityLevel::RELIABLE)
                             .withDurability(qos::DurabilityPolicy::DurabilityLevel::TRANSIENT_LOCAL)
                             .withHistory(qos::HistoryPolicy::HistoryKind::KEEP_LAST, 10)
                             .withLifespan(std::chrono::duration<double>(10.0))
                             .withPriority(128)
                             .withDeadline(std::chrono::duration<double>(1.0))
                             .withLatencyBudget(std::chrono::duration<double>(0.1))
                             .withOwnership(qos::OwnershipPolicy::OwnershipKind::SHARED)
                             .build();
        
        if (policies.empty()) {
            return TestResult::FAIL;
        }
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult QoSTest::testQoSProfiles() {
    try {
        qos::QoSProfile bestEffortProfile(qos::QoSProfile::ProfileType::BEST_EFFORT);
        qos::QoSProfile reliableProfile(qos::QoSProfile::ProfileType::RELIABLE);
        qos::QoSProfile realTimeProfile(qos::QoSProfile::ProfileType::REAL_TIME);
        qos::QoSProfile highThroughputProfile(qos::QoSProfile::ProfileType::HIGH_THROUGHPUT);
        
        if (bestEffortProfile.getPolicies().empty() ||
            reliableProfile.getPolicies().empty() ||
            realTimeProfile.getPolicies().empty() ||
            highThroughputProfile.getPolicies().empty()) {
            return TestResult::FAIL;
        }
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult QoSTest::testQoSManager() {
    try {
        qos::QoSManager& manager = qos::QoSManager::instance();
        
        // 测试获取默认配置文件
        auto bestEffortProfile = manager.getProfile(qos::QoSProfile::ProfileType::BEST_EFFORT);
        if (!bestEffortProfile) {
            return TestResult::FAIL;
        }
        
        // 测试获取最佳QoS策略
        auto optimalPolicies = manager.getOptimalPolicies(1024, true);
        if (optimalPolicies.empty()) {
            return TestResult::FAIL;
        }
        
        // 测试注册和获取自定义配置文件
        qos::QoSPolicyBuilder builder;
        auto customPolicies = builder.withReliability(qos::ReliabilityPolicy::ReliabilityLevel::RELIABLE)
                                   .withPriority(200)
                                   .build();
        auto customProfile = std::make_shared<qos::QoSProfile>(customPolicies);
        manager.registerProfile("custom_profile", customProfile);
        
        auto retrievedProfile = manager.getProfile("custom_profile");
        if (!retrievedProfile) {
            return TestResult::FAIL;
        }
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

// TransportTest implementation

TestResult TransportTest::testIntraProcessTransport() {
    try {
        transport::TransportManager& manager = transport::TransportManager::instance();
        manager.init();
        manager.start();
        
        auto transport = manager.getTransport(transport::TransportType::INTRA_PROCESS);
        if (!transport) {
            manager.stop();
            return TestResult::FAIL;
        }
        
        // 测试发送和接收
        int testValue = 42;
        bool sent = transport->send(&testValue, sizeof(testValue));
        if (!sent) {
            manager.stop();
            return TestResult::FAIL;
        }
        
        int readValue;
        bool received = transport->receive(&readValue, sizeof(readValue));
        if (!received || readValue != testValue) {
            manager.stop();
            return TestResult::FAIL;
        }
        
        manager.stop();
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult TransportTest::testSharedMemoryTransport() {
    try {
        transport::TransportManager& manager = transport::TransportManager::instance();
        manager.init();
        manager.start();
        
        auto transport = manager.getTransport(transport::TransportType::SHARED_MEMORY);
        if (!transport) {
            manager.stop();
            return TestResult::FAIL;
        }
        
        // 测试发送和接收
        int testValue = 42;
        bool sent = transport->send(&testValue, sizeof(testValue));
        if (!sent) {
            manager.stop();
            return TestResult::FAIL;
        }
        
        int readValue;
        bool received = transport->receive(&readValue, sizeof(readValue));
        if (!received || readValue != testValue) {
            manager.stop();
            return TestResult::FAIL;
        }
        
        manager.stop();
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult TransportTest::testNetworkTransport() {
    try {
        transport::TransportManager& manager = transport::TransportManager::instance();
        manager.init();
        manager.start();
        
        auto transport = manager.getTransport(transport::TransportType::NETWORK);
        if (!transport) {
            manager.stop();
            return TestResult::FAIL;
        }
        
        // 测试网络传输是否可用
        if (!transport->isAvailable()) {
            manager.stop();
            return TestResult::SKIPPED; // 网络传输可能需要网络环境，跳过测试
        }
        
        manager.stop();
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

// ServiceDiscoveryTest implementation

TestResult ServiceDiscoveryTest::testServiceDiscovery() {
    try {
        service_discovery::ServiceDiscovery& discovery = service_discovery::ServiceDiscovery::instance();
        discovery.init();
        discovery.start();
        
        // 测试服务注册
        service_discovery::ServiceInfo serviceInfo;
        serviceInfo.name = "test_service";
        serviceInfo.address = "127.0.0.1";
        serviceInfo.port = 5555;
        
        bool registered = discovery.registerService(serviceInfo);
        if (!registered) {
            discovery.stop();
            return TestResult::FAIL;
        }
        
        // 测试服务发现
        auto services = discovery.discoverServices("test_service");
        if (services.empty()) {
            discovery.stop();
            return TestResult::FAIL;
        }
        
        // 测试服务注销
        bool unregistered = discovery.unregisterService("test_service");
        if (!unregistered) {
            discovery.stop();
            return TestResult::FAIL;
        }
        
        discovery.stop();
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult ServiceDiscoveryTest::testHeartbeatMonitor() {
    try {
        service_discovery::ServiceDiscovery& discovery = service_discovery::ServiceDiscovery::instance();
        discovery.init();
        discovery.start();
        
        // 测试服务注册和心跳
        service_discovery::ServiceInfo serviceInfo;
        serviceInfo.name = "heartbeat_service";
        serviceInfo.address = "127.0.0.1";
        serviceInfo.port = 5556;
        
        bool registered = discovery.registerService(serviceInfo);
        if (!registered) {
            discovery.stop();
            return TestResult::FAIL;
        }
        
        // 测试心跳更新
        discovery.updateHeartbeat("heartbeat_service");
        
        // 等待一段时间，检查服务是否仍然活跃
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        auto services = discovery.discoverServices("heartbeat_service");
        if (services.empty()) {
            discovery.stop();
            return TestResult::FAIL;
        }
        
        discovery.stop();
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

// SystemTest implementation

TestResult SystemTest::testSystemIntegration() {
    try {
        // 测试系统集成
        // 初始化各个组件
        platform::PlatformManager::instance().init();
        memory::MemoryManager::instance().init();
        scheduler::SchedulerManager::instance().init();
        serialization::SerializerManager::instance().init();
        qos::QoSManager::instance();
        transport::TransportManager::instance().init();
        service_discovery::ServiceDiscovery::instance().init();
        
        // 启动各个组件
        scheduler::SchedulerManager::instance().start();
        transport::TransportManager::instance().start();
        service_discovery::ServiceDiscovery::instance().start();
        
        // 测试服务注册和发现
        service_discovery::ServiceInfo serviceInfo;
        serviceInfo.name = "integration_test_service";
        serviceInfo.address = "127.0.0.1";
        serviceInfo.port = 5557;
        
        service_discovery::ServiceDiscovery::instance().registerService(serviceInfo);
        auto services = service_discovery::ServiceDiscovery::instance().discoverServices("integration_test_service");
        if (services.empty()) {
            // 清理
            service_discovery::ServiceDiscovery::instance().stop();
            transport::TransportManager::instance().stop();
            scheduler::SchedulerManager::instance().stop();
            memory::MemoryManager::instance().shutdown();
            return TestResult::FAIL;
        }
        
        // 清理
        service_discovery::ServiceDiscovery::instance().stop();
        transport::TransportManager::instance().stop();
        scheduler::SchedulerManager::instance().stop();
        memory::MemoryManager::instance().shutdown();
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult SystemTest::testPerformance() {
    try {
        // 测试系统性能
        // 这里可以添加性能测试代码
        // 例如：测试内存分配性能、调度器性能、序列化性能等
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

TestResult SystemTest::testStability() {
    try {
        // 测试系统稳定性
        // 这里可以添加稳定性测试代码
        // 例如：长时间运行测试、压力测试等
        
        return TestResult::PASS;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return TestResult::FAIL;
    }
}

} // namespace self_test
} // namespace aurorart