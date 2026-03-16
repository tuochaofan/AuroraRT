#ifndef AURORART_SERVICE_DISCOVERY_CLIENT_H
#define AURORART_SERVICE_DISCOVERY_CLIENT_H

#include <string>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <unordered_map>
#include "aurorart/service_discovery/name_server.h"

namespace aurorart {
namespace service_discovery {

class ServiceDiscoveryClient {
public:
    ServiceDiscoveryClient(const std::vector<std::string>& nameserverNodes);
    ~ServiceDiscoveryClient();
    
    void init();
    void start();
    void stop();
    
    bool registerService(const std::string& serviceName, const std::string& host, int port, 
                        int weight = 1, const std::string& version = "1.0.0");
    bool unregisterService(const std::string& serviceName);
    std::vector<ServiceInfo> discoverServices();
    ServiceInfo* getService(const std::string& serviceName);
    std::vector<ServiceInfo> getServicesByVersion(const std::string& serviceName, const std::string& version);
    
    // 心跳机制
    void startHeartbeat(const std::string& serviceName, int interval = 5); // 5秒心跳
    void stopHeartbeat();
    
    // 集群管理
    void addNameServerNode(const std::string& node);
    void removeNameServerNode(const std::string& node);
    std::vector<std::string> getNameServerNodes() const;
    
    // 统计信息
    void printStats() const;
    
private:
    void heartbeatThread();
    void cacheRefreshThread();
    int connectToNameServer(const std::string& node);
    bool sendRequest(const std::string& node, const std::string& request, std::string& response);
    bool tryRequestWithFailover(const std::string& request, std::string& response);
    
    std::vector<std::string> nameserverNodes_;
    int currentNodeIndex_;
    int socket_;
    std::atomic<bool> running_;
    std::thread heartbeatThread_;
    std::thread cacheRefreshThread_;
    
    // 心跳相关
    std::string heartbeatService_;
    int heartbeatInterval_;
    std::string heartbeatHost_;
    int heartbeatPort_;
    int heartbeatWeight_;
    std::string heartbeatVersion_;
    
    // 缓存服务信息
    std::unordered_map<std::string, ServiceInfo> cachedServices_; // 使用unordered_map提高性能
    mutable std::mutex cacheMutex_;
    std::chrono::steady_clock::time_point lastCacheUpdate_;
    
    // 统计信息
    std::atomic<uint64_t> registerRequests_;
    std::atomic<uint64_t> discoverRequests_;
    std::atomic<uint64_t> heartbeatRequests_;
    std::atomic<uint64_t> cacheRefreshes_;
    std::atomic<uint64_t> failures_;
    std::atomic<uint64_t> failovers_;
};

} // namespace service_discovery
} // namespace aurorart

#endif // AURORART_SERVICE_DISCOVERY_CLIENT_H