#ifndef AURORART_SERVICE_DISCOVERY_H
#define AURORART_SERVICE_DISCOVERY_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include <chrono>
#include <functional>

namespace aurorart {
namespace service_discovery {

enum class DiscoveryMode {
    DECENTRALIZED,
    CENTRALIZED
};

struct ServiceInfo {
    std::string name;
    std::string type;
    std::string address;
    int port;
    std::chrono::steady_clock::time_point last_heartbeat;
    bool is_alive;
    
    ServiceInfo() : port(0), is_alive(true) {}
    ServiceInfo(const std::string& n, const std::string& t, const std::string& addr, int p)
        : name(n), type(t), address(addr), port(p), is_alive(true) {
        last_heartbeat = std::chrono::steady_clock::now();
    }
};

typedef std::function<void(const ServiceInfo&)> ServiceCallback;

typedef std::function<void(const ServiceInfo&)> ServiceLostCallback;

class ServiceDiscovery {
public:
    virtual ~ServiceDiscovery() = default;
    
    virtual void init() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void registerService(const ServiceInfo& service) = 0;
    virtual void unregisterService(const ServiceInfo& service) = 0;
    virtual std::vector<ServiceInfo> discoverServices(const std::string& serviceType) = 0;
    virtual void updateHeartbeat(const std::string& serviceName) = 0;
    virtual void registerServiceCallback(ServiceCallback callback) = 0;
    virtual void registerServiceLostCallback(ServiceLostCallback callback) = 0;
};

class DecentralizedDiscovery : public ServiceDiscovery {
public:
    DecentralizedDiscovery();
    void init() override;
    void start() override;
    void stop() override;
    void registerService(const ServiceInfo& service) override;
    void unregisterService(const ServiceInfo& service) override;
    std::vector<ServiceInfo> discoverServices(const std::string& serviceType) override;
    void updateHeartbeat(const std::string& serviceName) override;
    void registerServiceCallback(ServiceCallback callback) override;
    void registerServiceLostCallback(ServiceLostCallback callback) override;
    
private:
    void heartbeatMonitor();
    void clearCache();
    void initNetwork();
    void cleanupNetwork();
    void discoveryMonitor();
    void sendMulticast();
    void receiveMulticast();
    
    std::map<std::string, ServiceInfo> services_;
    std::mutex services_mutex_;
    std::vector<ServiceCallback> service_callbacks_;
    std::vector<ServiceLostCallback> service_lost_callbacks_;
    std::thread heartbeat_thread_;
    std::thread discovery_thread_;
    std::thread receive_thread_;
    std::atomic<bool> running_;
    std::chrono::seconds heartbeat_timeout_;
    int udp_port_;
    int multicast_socket_;
    struct sockaddr_in multicast_addr_;
    
    // 服务发现缓存
    std::map<std::string, std::vector<ServiceInfo>> discovery_cache_;
    std::map<std::string, std::chrono::steady_clock::time_point> cache_timestamps_;
    std::chrono::seconds cache_timeout_;
};

class CentralizedRouting : public ServiceDiscovery {
public:
    CentralizedRouting();
    void init() override;
    void start() override;
    void stop() override;
    void registerService(const ServiceInfo& service) override;
    void unregisterService(const ServiceInfo& service) override;
    std::vector<ServiceInfo> discoverServices(const std::string& serviceType) override;
    void updateHeartbeat(const std::string& serviceName) override;
    void registerServiceCallback(ServiceCallback callback) override;
    void registerServiceLostCallback(ServiceLostCallback callback) override;
    
private:
    void heartbeatMonitor();
    void initServer();
    void cleanupServer();
    void serverLoop();
    void clientLoop();
    
    std::map<std::string, ServiceInfo> services_;
    std::mutex services_mutex_;
    std::vector<ServiceCallback> service_callbacks_;
    std::vector<ServiceLostCallback> service_lost_callbacks_;
    std::thread heartbeat_thread_;
    std::thread server_thread_;
    std::thread client_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> server_running_;
    std::chrono::seconds heartbeat_timeout_;
    int server_port_;
};

class ServiceDiscoveryManager {
public:
    static ServiceDiscoveryManager& instance();
    
    void init(DiscoveryMode mode);
    void start();
    void stop();
    
    void registerService(const ServiceInfo& service);
    void unregisterService(const ServiceInfo& service);
    std::vector<ServiceInfo> discoverServices(const std::string& serviceType);
    void updateHeartbeat(const std::string& serviceName);
    
    void registerServiceCallback(ServiceCallback callback);
    void registerServiceLostCallback(ServiceLostCallback callback);
    
    // 混合发现模式相关方法
    void enableHybridMode(bool enabled);
    void setNetworkSizeThreshold(size_t threshold);
    DiscoveryMode getCurrentMode() const;
    
private:
    ServiceDiscoveryManager() = default;
    std::shared_ptr<ServiceDiscovery> discovery_;
    std::shared_ptr<DecentralizedDiscovery> decentralizedDiscovery_;
    std::shared_ptr<CentralizedRouting> centralizedDiscovery_;
    
    bool hybridMode_;
    size_t networkSizeThreshold_;
    DiscoveryMode currentMode_;
    
    void checkAndSwitchMode();
};

} // namespace service_discovery
} // namespace aurorart

#endif // AURORART_SERVICE_DISCOVERY_H