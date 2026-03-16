#ifndef AURORART_SELF_TEST_H
#define AURORART_SELF_TEST_H

#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace aurorart {
namespace self_test {

enum class TestResult {
    PASS,
    FAIL,
    SKIPPED
};

struct TestCase {
    std::string name;
    std::function<TestResult()> testFunction;
    std::string description;
};

class SelfTest {
public:
    static SelfTest& instance();
    
    void registerTestCase(const TestCase& testCase);
    void runAllTests();
    void runTest(const std::string& testName);
    void runTestsByCategory(const std::string& category);
    
    const std::vector<TestCase>& getTestCases() const { return testCases_; }
    
private:
    SelfTest();
    std::vector<TestCase> testCases_;
    void registerDefaultTests();
};

class MemoryTest {
public:
    static TestResult testMemoryPool();
    static TestResult testSharedMemory();
    static TestResult testMemoryManager();
};

class SchedulerTest {
public:
    static TestResult testCoroutineScheduler();
    static TestResult testPriorityScheduler();
    static TestResult testIntelligentScheduler();
};

class SerializationTest {
public:
    static TestResult testCDRSerializer();
    static TestResult testProtobufSerializer();
    static TestResult testFlatBuffersSerializer();
    static TestResult testJSONSerializer();
};

class QoSTest {
public:
    static TestResult testQoSPolicies();
    static TestResult testQoSProfiles();
    static TestResult testQoSManager();
};

class TransportTest {
public:
    static TestResult testIntraProcessTransport();
    static TestResult testSharedMemoryTransport();
    static TestResult testNetworkTransport();
};

class ServiceDiscoveryTest {
public:
    static TestResult testServiceDiscovery();
    static TestResult testHeartbeatMonitor();
};

class SystemTest {
public:
    static TestResult testSystemIntegration();
    static TestResult testPerformance();
    static TestResult testStability();
};

} // namespace self_test
} // namespace aurorart

#endif // AURORART_SELF_TEST_H