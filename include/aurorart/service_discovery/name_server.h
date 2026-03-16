#ifndef AURORART_NAME_SERVER_H
#define AURORART_NAME_SERVER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <set>

namespace aurorart {
namespace service_discovery {

class ServiceInfo {
public:
    ServiceInfo(const std::string& name, const std::string& host, int port, 
                int weight = 1, const std::string& version = "1.0.0", bool available = true)
        : name_(name), host_(host), port_(port), weight_(weight), 
          version_(version), available_(available) {
        lastUpdateTime_ = std::chrono::steady_clock::now();
        healthCheckTime_ = std::chrono::steady_clock::now();
    }
    
    std::string getName() const { return name_; }
    std::string getHost() const { return host_; }
    int getPort() const { return port_; }
    int getWeight() const { return weight_; }
    std::string getVersion() const { return version_; }
    bool isAvailable() const { return available_; }
    
    void setAvailable(bool available) { available_ = available; }
    void setWeight(int weight) { weight_ = weight; }
    void updateTimestamp() {
        lastUpdateTime_ = std::chrono::steady_clock::now();
    }
    void updateHealthCheckTime() {
        healthCheckTime_ = std::chrono::steady_clock::now();
    }
    
    bool isExpired() const {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdateTime_);
        return diff.count() > 30; // 30秒过期
    }
    
    bool needsHealthCheck() const {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - healthCheckTime_);
        return diff.count() > 10; // 10秒检查一次
    }
    
private:
    std::string name_;
    std::string host_;
    int port_;
    int weight_; // 服务权重
    std::string version_; // 服务版本
    std::atomic<bool> available_;
    std::chrono::steady_clock::time_point lastUpdateTime_;
    std::chrono::steady_clock::time_point healthCheckTime_;
};

class NameServer {
public:
    static NameServer& instance();
    
    void init(int port = 9876, const std::vector<std::string>& clusterNodes = {});
    void start();
    void stop();
    
    bool registerService(const std::string& serviceName, const std::string& host, int port, 
                        int weight = 1, const std::string& version = "1.0.0");
    bool unregisterService(const std::string& serviceName);
    std::vector<ServiceInfo> discoverServices();
    ServiceInfo* getService(const std::string& serviceName);
    std::vector<ServiceInfo> getServicesByVersion(const std::string& serviceName, const std::string& version);
    
    // 健康检查
    void startHealthCheck();
    void stopHealthCheck();
    
    // 集群管理
    void addClusterNode(const std::string& node);
    void removeClusterNode(const std::string& node);
    std::vector<std::string> getClusterNodes() const;
    
    // 统计信息
    void printStats() const;
    
private:
    NameServer();
    void healthCheckThread();
    void clusterSyncThread();
    void handleClientConnection(int clientSocket);
    void processRegisterRequest(const std::string& request);
    void processDiscoverRequest(int clientSocket);
    void processClusterSyncRequest(int clientSocket);
    bool checkServiceHealth(const ServiceInfo& service);
    void syncWithCluster();
    
    int serverSocket_;
    std::atomic<bool> running_;
    std::thread healthCheckThread_;
    std::thread serverThread_;
    std::thread clusterSyncThread_;
    
    std::unordered_map<std::string, std::unique_ptr<ServiceInfo>> services_; // 使用unordered_map提高性能
    mutable std::mutex servicesMutex_;
    
    std::set<std::string> clusterNodes_; // 集群节点
    mutable std::mutex clusterMutex_;
    
    // 统计信息
    std::atomic<uint64_t> registerRequests_;
    std::atomic<uint64_t> discoverRequests_;
    std::atomic<uint64_t> healthChecks_;
    std::atomic<uint64_t> expiredServices_;
    std::atomic<uint64_t> clusterSyncs_;
};

} // namespace service_discovery
} // namespace aurorart

#endif // AURORART_NAME_SERVER_H