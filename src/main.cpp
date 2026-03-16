#include "aurorart/platform/platform_abstraction.h"
#include "aurorart/memory/memory_manager.h"
#include "aurorart/transport/transport.h"
#include "aurorart/communication/communication_pattern.h"
#include "aurorart/serialization/serializer.h"
#include "aurorart/service_discovery/service_discovery.h"
#include "aurorart/qos/qos_policy.h"
#include "aurorart/scheduler/scheduler.h"
#include "aurorart/self_test/self_test.h"
#include "aurorart/node/node.h"
#include "aurorart/security/security.h"
#include "aurorart/monitoring/monitoring.h"
#include "aurorart/diagnostics/diagnostics.h"
#include "aurorart/domain/domain_manager.h"
#include "aurorart/plugin/plugin.h"
#include "aurorart/utils/config.h"
#include "aurorart/utils/logger.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // 初始化日志系统
    aurorart::utils::Logger::instance().init();
    AURORA_LOG_INFO("AuroraRT 1.0 Initialization");
    
    // 加载配置文件
    std::string configFile = "config.json";
    if (argc > 1) {
        configFile = argv[1];
    }
    
    if (!aurorart::utils::Config::instance().load(configFile)) {
        AURORA_LOG_WARN("Failed to load config file, using defaults");
        // 加载默认配置
        aurorart::utils::Config::instance().load("config.json.example");
    }
    AURORA_LOG_INFO("Configuration loaded");
    
    // 初始化平台抽象层
    aurorart::platform::PlatformManager::instance().init();
    AURORA_LOG_INFO("Platform abstraction initialized");
    
    // 初始化内存管理器
    aurorart::memory::MemoryManager::instance().init();
    AURORA_LOG_INFO("Memory manager initialized");
    
    // 初始化传输管理器
    aurorart::transport::TransportManager::instance().init();
    aurorart::transport::TransportManager::instance().start();
    AURORA_LOG_INFO("Transport manager initialized");
    
    // 初始化服务发现管理器
    std::string discoveryMode = aurorart::utils::Config::instance().get<std::string>("service_discovery.mode", "decentralized");
    aurorart::service_discovery::DiscoveryMode mode = discoveryMode == "centralized" ? 
        aurorart::service_discovery::DiscoveryMode::CENTRALIZED : 
        aurorart::service_discovery::DiscoveryMode::DECENTRALIZED;
    
    aurorart::service_discovery::ServiceDiscoveryManager::instance().init(mode);
    aurorart::service_discovery::ServiceDiscoveryManager::instance().start();
    AURORA_LOG_INFO("Service discovery manager initialized");
    
    // 初始化调度管理器
    aurorart::scheduler::SchedulerManager::instance().init();
    aurorart::scheduler::SchedulerManager::instance().start();
    AURORA_LOG_INFO("Scheduler manager initialized");
    
    // 初始化安全管理器
    aurorart::security::SecurityManager::instance().init();
    AURORA_LOG_INFO("Security manager initialized");
    
    // 初始化节点管理器
    aurorart::node::NodeManager::instance().init();
    aurorart::node::NodeManager::instance().start();
    AURORA_LOG_INFO("Node manager initialized");
    
    // 初始化监控管理器
    aurorart::monitoring::MonitorManager::instance().init();
    aurorart::monitoring::MonitorManager::instance().start();
    AURORA_LOG_INFO("Monitor manager initialized");
    
    // 初始化诊断管理器
    aurorart::diagnostics::DiagnosticsManager::instance().init();
    aurorart::diagnostics::DiagnosticsManager::instance().start();
    AURORA_LOG_INFO("Diagnostics manager initialized");
    
    // 初始化域管理器
    aurorart::domain::DomainManager::instance().init();
    AURORA_LOG_INFO("Domain manager initialized");
    
    // 初始化插件管理器
    aurorart::plugin::PluginManager::instance().init();
    aurorart::plugin::PluginManager::instance().start();
    AURORA_LOG_INFO("Plugin manager initialized");
    
    // 运行自检测试
    AURORA_LOG_INFO("Running self tests...");
    aurorart::self_test::SelfTest::instance().runAllTests();
    AURORA_LOG_INFO("Self tests completed");
    
    // 测试发布-订阅模式
    AURORA_LOG_INFO("Testing Pub-Sub Pattern:");
    auto pubSubPattern = aurorart::communication::CommunicationPatternFactory::createPattern(
        aurorart::communication::PatternType::PUB_SUB
    );
    pubSubPattern->init();
    pubSubPattern->start();
    
    // 创建发布者和订阅者
    auto publisher = dynamic_cast<aurorart::communication::PubSubPattern*>(pubSubPattern.get())->createPublisher<int>("test_topic");
    int receivedValue = 0;
    auto subscriber = dynamic_cast<aurorart::communication::PubSubPattern*>(pubSubPattern.get())->createSubscriber<int>("test_topic",
        [&](const int& value) {
            receivedValue = value;
            AURORA_LOG_INFO("Received: {}", value);
        }
    );
    
    // 发布消息
    int testValue = 42;
    publisher->publish(testValue);
    AURORA_LOG_INFO("Published: {}", testValue);
    
    // 测试请求-响应模式
    AURORA_LOG_INFO("Testing Req-Resp Pattern:");
    auto reqRespPattern = aurorart::communication::CommunicationPatternFactory::createPattern(
        aurorart::communication::PatternType::REQ_RESP
    );
    reqRespPattern->init();
    reqRespPattern->start();
    
    // 创建客户端和服务端
    auto client = dynamic_cast<aurorart::communication::ReqRespPattern*>(reqRespPattern.get())->createClient<int, int>("test_service");
    auto server = dynamic_cast<aurorart::communication::ReqRespPattern*>(reqRespPattern.get())->createServer<int, int>("test_service",
        [](const int& request) {
            return request * 2;
        }
    );
    
    // 测试事件模式
    AURORA_LOG_INFO("Testing Event Pattern:");
    auto eventPattern = aurorart::communication::CommunicationPatternFactory::createPattern(
        aurorart::communication::PatternType::EVENT
    );
    eventPattern->init();
    eventPattern->start();
    
    // 创建通知器和监听器
    auto notifier = dynamic_cast<aurorart::communication::EventPattern*>(eventPattern.get())->createNotifier("test_event");
    auto listener = dynamic_cast<aurorart::communication::EventPattern*>(eventPattern.get())->createListener("test_event",
        []() {
            AURORA_LOG_INFO("Event received!");
        }
    );
    
    // 发送事件
    notifier->notify();
    AURORA_LOG_INFO("Event notified");
    
    // 测试推-拉模式
    AURORA_LOG_INFO("Testing Push-Pull Pattern:");
    auto pushPullPattern = aurorart::communication::CommunicationPatternFactory::createPattern(
        aurorart::communication::PatternType::PUSH_PULL
    );
    pushPullPattern->init();
    pushPullPattern->start();
    
    // 创建推送器和拉取器
    auto pusher = dynamic_cast<aurorart::communication::PushPullPattern*>(pushPullPattern.get())->createPusher("test_channel");
    int receivedPullValue = 0;
    auto puller = dynamic_cast<aurorart::communication::PushPullPattern*>(pushPullPattern.get())->createPuller<int>("test_channel",
        [&](const int& value) {
            receivedPullValue = value;
            AURORA_LOG_INFO("Pulled: {}", value);
        }
    );
    
    // 推送数据
    int pushValue = 42;
    pusher->push(pushValue);
    AURORA_LOG_INFO("Pushed: {}", pushValue);
    
    // 测试TSN传输
    AURORA_LOG_INFO("Testing TSN Transport:");
    auto tsnTransport = aurorart::transport::TransportManager::instance().getTransport(aurorart::transport::TransportType::TSN);
    if (tsnTransport) {
        AURORA_LOG_INFO("TSN transport available, testing...");
        
        // 测试设置流量类
        auto tsn = dynamic_cast<aurorart::transport::TSNTransport*>(tsnTransport.get());
        if (tsn) {
            tsn->setTrafficClass(7); // 设置最高优先级
            tsn->setTimeSyncEnabled(true);
            tsn->setScheduleEnabled(true);
            
            // 测试发送数据
            int tsnTestValue = 123;
            bool sent = tsn->send(&tsnTestValue, sizeof(tsnTestValue));
            AURORA_LOG_INFO("TSN send result: {}", sent);
        }
    } else {
        AURORA_LOG_WARN("TSN transport not available");
    }
    
    // 测试序列化
    AURORA_LOG_INFO("Testing Serialization:");
    int testData = 123;
    auto serializer = aurorart::serialization::SerializerFactory::createSerializer(
        aurorart::serialization::SerializerType::CDR
    );
    auto serializedData = serializer->serialize(&testData, sizeof(testData));
    int deserializedData = 0;
    serializer->deserialize(serializedData, &deserializedData, sizeof(deserializedData));
    AURORA_LOG_INFO("Original: {}, Deserialized: {}", testData, deserializedData);
    
    // 测试QoS策略
    AURORA_LOG_INFO("Testing QoS Policies:");
    aurorart::qos::QoSPolicyBuilder builder;
    auto policies = builder.withReliability(aurorart::qos::ReliabilityPolicy::ReliabilityLevel::RELIABLE)
                         .withPriority(255)
                         .build();
    aurorart::qos::QoSManager::instance().applyPolicies(policies, &testData);
    AURORA_LOG_INFO("QoS policies applied");
    
    // 测试调度
    AURORA_LOG_INFO("Testing Scheduling:");
    auto taskId = aurorart::scheduler::SchedulerManager::instance().scheduleTask(
        []() {
            AURORA_LOG_INFO("Scheduled task executed");
        }
    );
    AURORA_LOG_INFO("Task scheduled with ID: {}", taskId);
    
    // 测试流水线
    AURORA_LOG_INFO("Testing Pipeline:");
    aurorart::pipeline::PipelineBuilder pipelineBuilder;
    auto pipeline = pipelineBuilder.addStage("Validation", [](void* data) {
        int* value = static_cast<int*>(data);
        AURORA_LOG_INFO("Validation stage: checking value {}", *value);
    })
    .addStage("Processing", [](void* data) {
        int* value = static_cast<int*>(data);
        *value *= 2;
        AURORA_LOG_INFO("Processing stage: updated value to {}", *value);
    })
    .addStage("Finalization", [](void* data) {
        int* value = static_cast<int*>(data);
        AURORA_LOG_INFO("Finalization stage: final value {}", *value);
    })
    .build();
    
    int pipelineValue = 42;
    pipeline->process(&pipelineValue);
    AURORA_LOG_INFO("Pipeline processing completed");
    
    // 测试节点管理
    AURORA_LOG_INFO("Testing Node Management:");
    auto node = aurorart::node::NodeManager::instance().createNode("test_node");
    node->start();
    AURORA_LOG_INFO("Created and started node: {} ({})\n", node->getID(), node->getName());
    
    // 测试安全管理
    AURORA_LOG_INFO("Testing Security Management:");
    bool authenticated = aurorart::security::SecurityManager::instance().authenticateNode("default_node", "default_credential");
    AURORA_LOG_INFO("Node authentication result: {}", authenticated);
    
    // 测试数据加密
    std::vector<uint8_t> encryptedData, decryptedData;
    int testEncryptData = 42;
    bool encrypted = aurorart::security::SecurityManager::instance().encryptData(&testEncryptData, sizeof(testEncryptData), encryptedData);
    bool decrypted = aurorart::security::SecurityManager::instance().decryptData(encryptedData.data(), encryptedData.size(), decryptedData);
    AURORA_LOG_INFO("Encryption test: {}, Decryption test: {}", encrypted, decrypted);
    
    // 测试监控模块
    AURORA_LOG_INFO("Testing Monitoring Module:");
    auto systemMonitor = aurorart::monitoring::MonitorManager::instance().createMonitor("system_monitor");
    systemMonitor->start();
    
    // 收集指标
    auto metrics = systemMonitor->collectMetrics();
    for (const auto& metric : metrics) {
        AURORA_LOG_INFO("Collected metric: Type={}, Value={}", static_cast<int>(metric.getType()), metric.getValue());
    }
    
    // 检查健康状态
    auto healthStatus = systemMonitor->getHealthStatus();
    AURORA_LOG_INFO("System health status: {}", static_cast<int>(healthStatus));
    
    // 检查组件健康状态
    auto componentHealth = aurorart::monitoring::MonitorManager::instance().getComponentHealth();
    for (const auto& [component, status] : componentHealth) {
        AURORA_LOG_INFO("Component {} health: {}", component, static_cast<int>(status));
    }
    
    // 测试诊断模块
    AURORA_LOG_INFO("Testing Diagnostics Module:");
    
    // 检测故障
    auto faults = aurorart::diagnostics::DiagnosticsManager::instance().detectFaults();
    AURORA_LOG_INFO("Detected {} faults", faults.size());
    
    // 分析故障
    aurorart::diagnostics::DiagnosticsManager::instance().analyzeFaults();
    auto diagnosticStatus = aurorart::diagnostics::DiagnosticsManager::instance().getDiagnosticStatus();
    auto diagnosticAnalysis = aurorart::diagnostics::DiagnosticsManager::instance().getDiagnosticAnalysis();
    AURORA_LOG_INFO("Diagnostic status: {}, Analysis: {}", static_cast<int>(diagnosticStatus), diagnosticAnalysis);
    
    // 尝试恢复故障
    for (const auto& fault : faults) {
        bool recovered = aurorart::diagnostics::DiagnosticsManager::instance().recoverFromFault(fault);
        AURORA_LOG_INFO("Recovery from fault '{}': {}", fault.getMessage(), recovered);
    }
    
    // 测试插件管理模块
    AURORA_LOG_INFO("Testing Plugin Management Module:");
    
    // 列出可用插件
    auto availablePlugins = aurorart::plugin::PluginManager::instance().listAvailablePlugins();
    AURORA_LOG_INFO("Available plugins: {}", availablePlugins.size());
    for (const auto& pluginPath : availablePlugins) {
        AURORA_LOG_INFO("Available plugin: {}", pluginPath);
    }
    
    // 列出已加载插件
    auto loadedPlugins = aurorart::plugin::PluginManager::instance().listPlugins();
    AURORA_LOG_INFO("Loaded plugins: {}", loadedPlugins.size());
    for (const auto& plugin : loadedPlugins) {
        AURORA_LOG_INFO("Loaded plugin: {} (v{})
", plugin->getName(), plugin->getVersion());
    }
    
    // 测试域管理功能
    AURORA_LOG_INFO("Testing Domain Management:");
    
    // 创建域
    bool domainCreated = aurorart::domain::DomainManager::instance().createDomain("test_domain", "Test domain for AuroraRT");
    AURORA_LOG_INFO("Domain creation result: {}", domainCreated);
    
    // 创建分区
    bool partitionCreated = aurorart::domain::DomainManager::instance().createPartition("test_domain", "test_partition", "Test partition");
    AURORA_LOG_INFO("Partition creation result: {}", partitionCreated);
    
    // 加入域
    bool joinedDomain = aurorart::domain::DomainManager::instance().joinDomain("test_domain", "test_node");
    AURORA_LOG_INFO("Join domain result: {}", joinedDomain);
    
    // 加入分区
    bool joinedPartition = aurorart::domain::DomainManager::instance().joinPartition("test_domain", "test_partition", "test_node");
    AURORA_LOG_INFO("Join partition result: {}", joinedPartition);
    
    // 列出域
    auto domains = aurorart::domain::DomainManager::instance().listDomains();
    AURORA_LOG_INFO("Available domains: {}", domains.size());
    for (const auto& domain : domains) {
        AURORA_LOG_INFO("Domain: {}", domain);
    }
    
    // 列出分区
    auto partitions = aurorart::domain::DomainManager::instance().listPartitions("test_domain");
    AURORA_LOG_INFO("Partitions in test_domain: {}", partitions.size());
    for (const auto& partition : partitions) {
        AURORA_LOG_INFO("Partition: {}", partition);
    }
    
    // 测试操作码处理
    AURORA_LOG_INFO("Testing Opcode Processing:");
    std::string response;
    
    // 测试创建域操作码
    bool opResult = aurorart::domain::DomainManager::instance().processOpcode(
        aurorart::domain::DomainOpcode::CREATE_DOMAIN, 
        "test_opcode_domain Opcode test domain", 
        response
    );
    AURORA_LOG_INFO("Create domain opcode result: {}, response: {}", opResult, response);
    
    // 测试列出域操作码
    opResult = aurorart::domain::DomainManager::instance().processOpcode(
        aurorart::domain::DomainOpcode::LIST_DOMAINS, 
        "", 
        response
    );
    AURORA_LOG_INFO("List domains opcode result: {}, response: {}", opResult, response);
    
    // 清理资源
    pubSubPattern->stop();
    reqRespPattern->stop();
    eventPattern->stop();
    pushPullPattern->stop();
    
    aurorart::node::NodeManager::instance().stop();
    aurorart::security::SecurityManager::instance().shutdown();
    aurorart::monitoring::MonitorManager::instance().stop();
    aurorart::diagnostics::DiagnosticsManager::instance().stop();
    aurorart::plugin::PluginManager::instance().stop();
    aurorart::domain::DomainManager::instance().shutdown();
    aurorart::scheduler::SchedulerManager::instance().stop();
    aurorart::service_discovery::ServiceDiscoveryManager::instance().stop();
    aurorart::transport::TransportManager::instance().stop();
    aurorart::memory::MemoryManager::instance().shutdown();
    
    AURORA_LOG_INFO("AuroraRT 1.0 Shutdown Complete");
    
    return 0;
}