#include "aurorart/service_discovery/name_server.h"
#include "aurorart/utils/logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <cstring>
#include <sstream>

namespace aurorart {
namespace service_discovery {

NameServer::NameServer() : serverSocket_(-1), running_(false),
    registerRequests_(0), discoverRequests_(0), healthChecks_(0), 
    expiredServices_(0), clusterSyncs_(0) {
}

NameServer& NameServer::instance() {
    static NameServer instance;
    return instance;
}

void NameServer::init(int port, const std::vector<std::string>& clusterNodes) {
    // 创建服务器 socket
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        LOG_ERROR("Failed to create socket: " << strerror(errno));
        return;
    }
    
    // 设置地址重用
    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR("Failed to set socket options: " << strerror(errno));
        close(serverSocket_);
        serverSocket_ = -1;
        return;
    }
    
    // 绑定地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind socket: " << strerror(errno));
        close(serverSocket_);
        serverSocket_ = -1;
        return;
    }
    
    // 开始监听
    if (listen(serverSocket_, 100) < 0) { // 增加监听队列大小
        LOG_ERROR("Failed to listen on socket: " << strerror(errno));
        close(serverSocket_);
        serverSocket_ = -1;
        return;
    }
    
    // 添加集群节点
    for (const auto& node : clusterNodes) {
        addClusterNode(node);
    }
    
    LOG_INFO("NameServer initialized on port " << port << " with " << clusterNodes.size() << " cluster nodes");
}

void NameServer::start() {
    if (serverSocket_ < 0) {
        LOG_ERROR("NameServer not initialized");
        return;
    }
    
    running_ = true;
    
    // 启动服务器线程
    serverThread_ = std::thread([this]() {
        while (running_) {
            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientAddrLen);
            if (clientSocket < 0) {
                if (running_) {
                    LOG_ERROR("Failed to accept connection: " << strerror(errno));
                }
                continue;
            }
            
            // 处理客户端连接
            std::thread([this, clientSocket]() {
                handleClientConnection(clientSocket);
            }).detach();
        }
    });
    
    // 启动健康检查线程
    startHealthCheck();
    
    // 启动集群同步线程
    clusterSyncThread_ = std::thread([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            syncWithCluster();
        }
    });
    
    LOG_INFO("NameServer started");
}

void NameServer::stop() {
    running_ = false;
    
    // 停止健康检查
    stopHealthCheck();
    
    // 关闭服务器 socket
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    // 等待线程结束
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    
    if (clusterSyncThread_.joinable()) {
        clusterSyncThread_.join();
    }
    
    LOG_INFO("NameServer stopped");
}

bool NameServer::registerService(const std::string& serviceName, const std::string& host, int port, 
                               int weight, const std::string& version) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = services_.find(serviceName);
    if (it != services_.end()) {
        // 更新现有服务
        it->second->setAvailable(true);
        it->second->updateTimestamp();
        LOG_INFO("Updated service: " << serviceName << " at " << host << ":" << port << " (v" << version << ")");
    } else {
        // 注册新服务
        services_[serviceName] = std::make_unique<ServiceInfo>(serviceName, host, port, weight, version);
        LOG_INFO("Registered service: " << serviceName << " at " << host << ":" << port << " (v" << version << ")");
    }
    
    registerRequests_++;
    return true;
}

bool NameServer::unregisterService(const std::string& serviceName) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = services_.find(serviceName);
    if (it != services_.end()) {
        services_.erase(it);
        LOG_INFO("Unregistered service: " << serviceName);
        return true;
    }
    
    return false;
}

std::vector<ServiceInfo> NameServer::discoverServices() {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    std::vector<ServiceInfo> result;
    for (const auto& entry : services_) {
        if (entry.second->isAvailable() && !entry.second->isExpired()) {
            result.emplace_back(*entry.second);
        }
    }
    
    discoverRequests_++;
    return result;
}

ServiceInfo* NameServer::getService(const std::string& serviceName) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = services_.find(serviceName);
    if (it != services_.end() && it->second->isAvailable() && !it->second->isExpired()) {
        return it->second.get();
    }
    
    return nullptr;
}

std::vector<ServiceInfo> NameServer::getServicesByVersion(const std::string& serviceName, const std::string& version) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    std::vector<ServiceInfo> result;
    for (const auto& entry : services_) {
        if (entry.first == serviceName && entry.second->getVersion() == version && 
            entry.second->isAvailable() && !entry.second->isExpired()) {
            result.emplace_back(*entry.second);
        }
    }
    
    return result;
}

void NameServer::startHealthCheck() {
    healthCheckThread_ = std::thread([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            healthCheckThread();
        }
    });
}

void NameServer::stopHealthCheck() {
    if (healthCheckThread_.joinable()) {
        healthCheckThread_.join();
    }
}

void NameServer::healthCheckThread() {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    healthChecks_++;
    
    // 检查服务是否过期或需要健康检查
    for (auto it = services_.begin(); it != services_.end();) {
        if (it->second->isExpired()) {
            LOG_WARN("Service expired: " << it->first);
            it->second->setAvailable(false);
            expiredServices_++;
            it++;
        } else if (it->second->needsHealthCheck()) {
            bool healthy = checkServiceHealth(*it->second);
            it->second->setAvailable(healthy);
            it->second->updateHealthCheckTime();
            if (!healthy) {
                LOG_WARN("Service health check failed: " << it->first);
            }
            it++;
        } else {
            it++;
        }
    }
}

bool NameServer::checkServiceHealth(const ServiceInfo& service) {
    // 简单的健康检查：尝试连接服务端口
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(service.getHost().c_str());
    addr.sin_port = htons(service.getPort());
    
    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    
    return result == 0;
}

void NameServer::handleClientConnection(int clientSocket) {
    char buffer[4096]; // 增加缓冲区大小
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead < 0) {
        LOG_ERROR("Failed to receive data: " << strerror(errno));
        close(clientSocket);
        return;
    }
    
    buffer[bytesRead] = '\0';
    std::string request(buffer);
    
    // 处理请求
    if (request.substr(0, 8) == "REGISTER ") {
        processRegisterRequest(request.substr(8));
    } else if (request == "DISCOVER") {
        processDiscoverRequest(clientSocket);
    } else if (request.substr(0, 12) == "CLUSTER_SYNC ") {
        processClusterSyncRequest(clientSocket);
    }
    
    close(clientSocket);
}

void NameServer::processRegisterRequest(const std::string& request) {
    // 格式: serviceName host port weight version
    size_t firstSpace = request.find(' ');
    size_t secondSpace = request.find(' ', firstSpace + 1);
    size_t thirdSpace = request.find(' ', secondSpace + 1);
    size_t fourthSpace = request.find(' ', thirdSpace + 1);
    
    if (firstSpace == std::string::npos || secondSpace == std::string::npos ||
        thirdSpace == std::string::npos || fourthSpace == std::string::npos) {
        LOG_ERROR("Invalid register request format: " << request);
        return;
    }
    
    std::string serviceName = request.substr(0, firstSpace);
    std::string host = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    int port = std::stoi(request.substr(secondSpace + 1, thirdSpace - secondSpace - 1));
    int weight = std::stoi(request.substr(thirdSpace + 1, fourthSpace - thirdSpace - 1));
    std::string version = request.substr(fourthSpace + 1);
    
    registerService(serviceName, host, port, weight, version);
}

void NameServer::processDiscoverRequest(int clientSocket) {
    auto services = discoverServices();
    
    std::string response;
    for (const auto& service : services) {
        response += service.getName() + " " + service.getHost() + " " + 
                   std::to_string(service.getPort()) + " " + 
                   std::to_string(service.getWeight()) + " " + 
                   service.getVersion() + "\n";
    }
    
    send(clientSocket, response.c_str(), response.size(), 0);
}

void NameServer::processClusterSyncRequest(int clientSocket) {
    // 发送当前服务列表给集群节点
    auto services = discoverServices();
    
    std::string response;
    for (const auto& service : services) {
        response += "SERVICE " + service.getName() + " " + service.getHost() + " " + 
                   std::to_string(service.getPort()) + " " + 
                   std::to_string(service.getWeight()) + " " + 
                   service.getVersion() + "\n";
    }
    
    send(clientSocket, response.c_str(), response.size(), 0);
}

void NameServer::syncWithCluster() {
    std::lock_guard<std::mutex> lock(clusterMutex_);
    
    for (const auto& node : clusterNodes_) {
        // 解析节点地址和端口
        size_t colonPos = node.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }
        
        std::string host = node.substr(0, colonPos);
        int port = std::stoi(node.substr(colonPos + 1));
        
        // 连接到集群节点
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            continue;
        }
        
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(host.c_str());
        addr.sin_port = htons(port);
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            continue;
        }
        
        // 发送集群同步请求
        std::string request = "CLUSTER_SYNC";
        send(sock, request.c_str(), request.size(), 0);
        
        // 接收响应
        char buffer[4096];
        ssize_t bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string response(buffer);
            
            // 解析响应，更新服务列表
            std::istringstream iss(response);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.substr(0, 8) == "SERVICE ") {
                    std::istringstream lineIss(line.substr(8));
                    std::string serviceName, host, version;
                    int port, weight;
                    lineIss >> serviceName >> host >> port >> weight >> version;
                    
                    // 注册服务
                    registerService(serviceName, host, port, weight, version);
                }
            }
        }
        
        close(sock);
    }
    
    clusterSyncs_++;
}

void NameServer::addClusterNode(const std::string& node) {
    std::lock_guard<std::mutex> lock(clusterMutex_);
    clusterNodes_.insert(node);
    LOG_INFO("Added cluster node: " << node);
}

void NameServer::removeClusterNode(const std::string& node) {
    std::lock_guard<std::mutex> lock(clusterMutex_);
    clusterNodes_.erase(node);
    LOG_INFO("Removed cluster node: " << node);
}

std::vector<std::string> NameServer::getClusterNodes() const {
    std::lock_guard<std::mutex> lock(clusterMutex_);
    return std::vector<std::string>(clusterNodes_.begin(), clusterNodes_.end());
}

void NameServer::printStats() const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    std::lock_guard<std::mutex> clusterLock(clusterMutex_);
    
    LOG_INFO("NameServer Statistics:");
    LOG_INFO("Registered services: " << services_.size());
    LOG_INFO("Register requests: " << registerRequests_);
    LOG_INFO("Discover requests: " << discoverRequests_);
    LOG_INFO("Health checks: " << healthChecks_);
    LOG_INFO("Expired services: " << expiredServices_);
    LOG_INFO("Cluster nodes: " << clusterNodes_.size());
    LOG_INFO("Cluster syncs: " << clusterSyncs_);
    
    LOG_INFO("Service list:");
    for (const auto& entry : services_) {
        const auto& service = entry.second;
        LOG_INFO("  " << service->getName() << " - " << service->getHost() << ":" << service->getPort() << 
                " (v" << service->getVersion() << ", weight=" << service->getWeight() << ") - " << 
                (service->isAvailable() ? "Available" : "Unavailable"));
    }
    
    LOG_INFO("Cluster nodes:");
    for (const auto& node : clusterNodes_) {
        LOG_INFO("  " << node);
    }
}

} // namespace service_discovery
} // namespace aurorart