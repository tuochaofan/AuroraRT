#include "aurorart/service_discovery/service_discovery_client.h"
#include "aurorart/utils/logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

namespace aurorart {
namespace service_discovery {

ServiceDiscoveryClient::ServiceDiscoveryClient(const std::vector<std::string>& nameserverNodes)
    : nameserverNodes_(nameserverNodes), currentNodeIndex_(0), socket_(-1), running_(false),
      heartbeatInterval_(5), registerRequests_(0), discoverRequests_(0), 
      heartbeatRequests_(0), cacheRefreshes_(0), failures_(0), failovers_(0) {
}

ServiceDiscoveryClient::~ServiceDiscoveryClient() {
    stop();
}

void ServiceDiscoveryClient::init() {
    LOG_INFO("ServiceDiscoveryClient initialized with " << nameserverNodes_.size() << " NameServer nodes");
}

void ServiceDiscoveryClient::start() {
    running_ = true;
    
    // 启动缓存刷新线程
    cacheRefreshThread_ = std::thread([this]() {
        cacheRefreshThread();
    });
    
    LOG_INFO("ServiceDiscoveryClient started");
}

void ServiceDiscoveryClient::stop() {
    running_ = false;
    
    // 停止心跳
    stopHeartbeat();
    
    // 停止缓存刷新线程
    if (cacheRefreshThread_.joinable()) {
        cacheRefreshThread_.join();
    }
    
    // 关闭socket
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
    
    LOG_INFO("ServiceDiscoveryClient stopped");
}

bool ServiceDiscoveryClient::registerService(const std::string& serviceName, const std::string& host, int port, 
                                           int weight, const std::string& version) {
    std::string request = "REGISTER " + serviceName + " " + host + " " + 
                         std::to_string(port) + " " + std::to_string(weight) + " " + version;
    std::string response;
    
    if (tryRequestWithFailover(request, response)) {
        registerRequests_++;
        
        // 保存心跳信息
        heartbeatService_ = serviceName;
        heartbeatHost_ = host;
        heartbeatPort_ = port;
        heartbeatWeight_ = weight;
        heartbeatVersion_ = version;
        
        return true;
    }
    
    failures_++;
    return false;
}

bool ServiceDiscoveryClient::unregisterService(const std::string& serviceName) {
    // 暂时不实现，NameServer不支持注销服务
    return true;
}

std::vector<ServiceInfo> ServiceDiscoveryClient::discoverServices() {
    std::string request = "DISCOVER";
    std::string response;
    
    if (tryRequestWithFailover(request, response)) {
        discoverRequests_++;
        
        // 解析响应
        std::vector<ServiceInfo> services;
        std::unordered_map<std::string, ServiceInfo> serviceMap;
        
        std::istringstream iss(response);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            
            std::istringstream lineIss(line);
            std::string serviceName, host, version;
            int port, weight;
            
            if (lineIss >> serviceName >> host >> port >> weight >> version) {
                ServiceInfo service(serviceName, host, port, weight, version);
                services.push_back(service);
                serviceMap[serviceName] = service;
            }
        }
        
        // 更新缓存
        { 
            std::lock_guard<std::mutex> lock(cacheMutex_);
            cachedServices_ = serviceMap;
            lastCacheUpdate_ = std::chrono::steady_clock::now();
        }
        
        return services;
    }
    
    failures_++;
    
    // 返回缓存的服务信息
    std::lock_guard<std::mutex> lock(cacheMutex_);
    std::vector<ServiceInfo> services;
    for (const auto& entry : cachedServices_) {
        services.push_back(entry.second);
    }
    return services;
}

ServiceInfo* ServiceDiscoveryClient::getService(const std::string& serviceName) {
    // 先检查缓存
    { 
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cachedServices_.find(serviceName);
        if (it != cachedServices_.end() && it->second.isAvailable()) {
            return &(it->second);
        }
    }
    
    // 缓存中没有，重新发现
    auto services = discoverServices();
    for (auto& service : services) {
        if (service.getName() == serviceName && service.isAvailable()) {
            return &service;
        }
    }
    
    return nullptr;
}

std::vector<ServiceInfo> ServiceDiscoveryClient::getServicesByVersion(const std::string& serviceName, const std::string& version) {
    std::vector<ServiceInfo> result;
    
    // 先检查缓存
    { 
        std::lock_guard<std::mutex> lock(cacheMutex_);
        for (const auto& entry : cachedServices_) {
            const auto& service = entry.second;
            if (service.getName() == serviceName && service.getVersion() == version && service.isAvailable()) {
                result.push_back(service);
            }
        }
        
        if (!result.empty()) {
            return result;
        }
    }
    
    // 缓存中没有，重新发现
    auto services = discoverServices();
    for (auto& service : services) {
        if (service.getName() == serviceName && service.getVersion() == version && service.isAvailable()) {
            result.push_back(service);
        }
    }
    
    return result;
}

void ServiceDiscoveryClient::startHeartbeat(const std::string& serviceName, int interval) {
    heartbeatService_ = serviceName;
    heartbeatInterval_ = interval;
    
    if (!heartbeatThread_.joinable()) {
        heartbeatThread_ = std::thread([this]() {
            heartbeatThread();
        });
    }
}

void ServiceDiscoveryClient::stopHeartbeat() {
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
}

void ServiceDiscoveryClient::heartbeatThread() {
    while (running_ && !heartbeatService_.empty()) {
        // 重新注册服务作为心跳
        registerService(heartbeatService_, heartbeatHost_, heartbeatPort_, 
                       heartbeatWeight_, heartbeatVersion_);
        heartbeatRequests_++;
        
        std::this_thread::sleep_for(std::chrono::seconds(heartbeatInterval_));
    }
}

void ServiceDiscoveryClient::cacheRefreshThread() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(10)); // 每10秒刷新一次缓存
        discoverServices();
        cacheRefreshes_++;
    }
}

int ServiceDiscoveryClient::connectToNameServer(const std::string& node) {
    // 解析节点地址和端口
    size_t colonPos = node.find(':');
    if (colonPos == std::string::npos) {
        LOG_ERROR("Invalid NameServer node format: " << node);
        return -1;
    }
    
    std::string host = node.substr(0, colonPos);
    int port = std::stoi(node.substr(colonPos + 1));
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOG_ERROR("Failed to create socket: " << strerror(errno));
        return -1;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    addr.sin_port = htons(port);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Failed to connect to NameServer at " << node << ": " << strerror(errno));
        close(sock);
        return -1;
    }
    
    LOG_INFO("Connected to NameServer at " << node);
    return sock;
}

bool ServiceDiscoveryClient::sendRequest(const std::string& node, const std::string& request, std::string& response) {
    int sock = connectToNameServer(node);
    if (sock < 0) {
        return false;
    }
    
    // 发送请求
    if (send(sock, request.c_str(), request.size(), 0) < 0) {
        LOG_ERROR("Failed to send request to NameServer at " << node << ": " << strerror(errno));
        close(sock);
        return false;
    }
    
    // 接收响应
    char buffer[4096]; // 增加缓冲区大小
    ssize_t bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead < 0) {
        LOG_ERROR("Failed to receive response from NameServer at " << node << ": " << strerror(errno));
        close(sock);
        return false;
    }
    
    buffer[bytesRead] = '\0';
    response = buffer;
    close(sock);
    return true;
}

bool ServiceDiscoveryClient::tryRequestWithFailover(const std::string& request, std::string& response) {
    if (nameserverNodes_.empty()) {
        LOG_ERROR("No NameServer nodes configured");
        return false;
    }
    
    // 尝试所有NameServer节点
    for (size_t i = 0; i < nameserverNodes_.size(); i++) {
        std::string node = nameserverNodes_[(currentNodeIndex_ + i) % nameserverNodes_.size()];
        
        if (sendRequest(node, request, response)) {
            // 更新当前节点索引
            currentNodeIndex_ = (currentNodeIndex_ + i) % nameserverNodes_.size();
            return true;
        }
        
        // 尝试下一个节点
        failovers_++;
        LOG_WARN("Failed to connect to NameServer at " << node << ", trying next node");
    }
    
    return false;
}

void ServiceDiscoveryClient::addNameServerNode(const std::string& node) {
    nameserverNodes_.push_back(node);
    LOG_INFO("Added NameServer node: " << node);
}

void ServiceDiscoveryClient::removeNameServerNode(const std::string& node) {
    auto it = std::find(nameserverNodes_.begin(), nameserverNodes_.end(), node);
    if (it != nameserverNodes_.end()) {
        nameserverNodes_.erase(it);
        LOG_INFO("Removed NameServer node: " << node);
    }
}

std::vector<std::string> ServiceDiscoveryClient::getNameServerNodes() const {
    return nameserverNodes_;
}

void ServiceDiscoveryClient::printStats() const {
    LOG_INFO("ServiceDiscoveryClient Statistics:");
    LOG_INFO("Register requests: " << registerRequests_);
    LOG_INFO("Discover requests: " << discoverRequests_);
    LOG_INFO("Heartbeat requests: " << heartbeatRequests_);
    LOG_INFO("Cache refreshes: " << cacheRefreshes_);
    LOG_INFO("Failures: " << failures_);
    LOG_INFO("Failovers: " << failovers_);
    LOG_INFO("NameServer nodes: " << nameserverNodes_.size());
    
    std::lock_guard<std::mutex> lock(cacheMutex_);
    LOG_INFO("Cached services: " << cachedServices_.size());
    for (const auto& entry : cachedServices_) {
        const auto& service = entry.second;
        LOG_INFO("  " << service.getName() << " - " << service.getHost() << ":" << service.getPort() << 
                " (v" << service.getVersion() << ", weight=" << service.getWeight() << ") - " << 
                (service.isAvailable() ? "Available" : "Unavailable"));
    }
    
    LOG_INFO("NameServer nodes:");
    for (const auto& node : nameserverNodes_) {
        LOG_INFO("  " << node);
    }
}

} // namespace service_discovery
} // namespace aurorart