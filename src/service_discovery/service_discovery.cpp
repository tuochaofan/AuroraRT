#include "aurorart/service_discovery/service_discovery.h"
#include "aurorart/utils/logger.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>

// 网络相关头文件
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif

namespace aurorart {
namespace service_discovery {

// DecentralizedDiscovery implementation

DecentralizedDiscovery::DecentralizedDiscovery()
    : running_(false), heartbeat_timeout_(5), cache_timeout_(1), udp_port_(49152), multicast_socket_(-1) {
    // 初始化多播地址
    memset(&multicast_addr_, 0, sizeof(multicast_addr_));
    multicast_addr_.sin_family = AF_INET;
    multicast_addr_.sin_port = htons(udp_port_);
    inet_pton(AF_INET, "239.255.255.250", &multicast_addr_.sin_addr);
}

void DecentralizedDiscovery::init() {
    AURORA_LOG_INFO("Initializing decentralized service discovery");
    
    // 初始化网络通信
    initNetwork();
}

void DecentralizedDiscovery::start() {
    AURORA_LOG_INFO("Starting decentralized service discovery");
    running_ = true;
    heartbeat_thread_ = std::thread(&DecentralizedDiscovery::heartbeatMonitor, this);
    discovery_thread_ = std::thread(&DecentralizedDiscovery::discoveryMonitor, this);
}

void DecentralizedDiscovery::stop() {
    AURORA_LOG_INFO("Stopping decentralized service discovery");
    running_ = false;
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    if (discovery_thread_.joinable()) {
        discovery_thread_.join();
    }
    
    // 清理网络资源
    cleanupNetwork();
}

void DecentralizedDiscovery::initNetwork() {
    // 初始化网络通信
    #if defined(_WIN32)
    // Windows实现
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        AURORA_LOG_ERROR("WSAStartup failed");
        return;
    }
    #endif
    
    // 创建UDP套接字
    multicast_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (multicast_socket_ < 0) {
        AURORA_LOG_ERROR("Failed to create multicast socket");
        return;
    }
    
    // 设置套接字选项，允许端口重用
    int opt = 1;
    setsockopt(multicast_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    // 绑定到端口
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(udp_port_);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(multicast_socket_, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        AURORA_LOG_ERROR("Failed to bind multicast socket");
        #if defined(_WIN32)
        closesocket(multicast_socket_);
        #else
        close(multicast_socket_);
        #endif
        multicast_socket_ = -1;
        return;
    }
    
    // 加入多播组
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    
    if (setsockopt(multicast_socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq)) < 0) {
        AURORA_LOG_ERROR("Failed to join multicast group");
        #if defined(_WIN32)
        closesocket(multicast_socket_);
        #else
        close(multicast_socket_);
        #endif
        multicast_socket_ = -1;
        return;
    }
    
    AURORA_LOG_INFO("Multicast network initialized on port {}", udp_port_);
}

void DecentralizedDiscovery::cleanupNetwork() {
    // 清理网络资源
    if (multicast_socket_ >= 0) {
        // 离开多播组
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        setsockopt(multicast_socket_, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char*)&mreq, sizeof(mreq));
        
        // 关闭套接字
        #if defined(_WIN32)
        closesocket(multicast_socket_);
        #else
        close(multicast_socket_);
        #endif
        multicast_socket_ = -1;
    }
    
    #if defined(_WIN32)
    WSACleanup();
    #endif
    
    AURORA_LOG_INFO("Network resources cleaned up");
}

void DecentralizedDiscovery::discoveryMonitor() {
    // 启动接收线程
    receive_thread_ = std::thread(&DecentralizedDiscovery::receiveMulticast, this);
    
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // 发送多播消息
        sendMulticast();
        
        AURORA_LOG_DEBUG("Performing service discovery scan");
    }
    
    // 等待接收线程结束
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
}

void DecentralizedDiscovery::sendMulticast() {
    if (multicast_socket_ < 0) {
        return;
    }
    
    // 构建服务发现消息
    std::string message = "AURORA_DISCOVERY";
    
    // 发送多播消息
    ssize_t sent = sendto(multicast_socket_, message.c_str(), message.length(), 0,
                         (struct sockaddr*)&multicast_addr_, sizeof(multicast_addr_));
    
    if (sent < 0) {
        AURORA_LOG_ERROR("Failed to send multicast message");
    }
}

void DecentralizedDiscovery::receiveMulticast() {
    if (multicast_socket_ < 0) {
        return;
    }
    
    char buffer[1024];
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);
    
    while (running_) {
        // 设置超时，避免阻塞
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(multicast_socket_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        
        // 接收多播消息
        ssize_t recv_len = recvfrom(multicast_socket_, buffer, sizeof(buffer), 0,
                                   (struct sockaddr*)&sender_addr, &sender_addr_len);
        
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            AURORA_LOG_DEBUG("Received multicast message: {}", buffer);
            
            // 处理接收到的消息
            // 这里可以添加消息解析和服务发现逻辑
        }
    }
}

void DecentralizedDiscovery::registerService(const ServiceInfo& service) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    services_[service.name] = service;
    AURORA_LOG_INFO("Service registered: {} (type: {})", service.name, service.type);
    
    // 清除缓存，因为服务列表发生变化
    clearCache();
    
    // 通知所有回调
    for (const auto& callback : service_callbacks_) {
        callback(service);
    }
}

void DecentralizedDiscovery::unregisterService(const ServiceInfo& service) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    auto it = services_.find(service.name);
    if (it != services_.end()) {
        services_.erase(it);
        AURORA_LOG_INFO("Service unregistered: {}", service.name);
        
        // 清除缓存，因为服务列表发生变化
        clearCache();
    }
}

std::vector<ServiceInfo> DecentralizedDiscovery::discoverServices(const std::string& serviceType) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    auto now = std::chrono::steady_clock::now();
    
    // 检查缓存是否有效
    auto cache_it = cache_timestamps_.find(serviceType);
    if (cache_it != cache_timestamps_.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - cache_it->second);
        if (elapsed <= cache_timeout_) {
            AURORA_LOG_DEBUG("Service discovery cache hit for type: {}", serviceType);
            return discovery_cache_[serviceType];
        }
    }
    
    // 缓存无效，重新扫描
    std::vector<ServiceInfo> result;
    for (const auto& [name, service] : services_) {
        if (service.type == serviceType && service.is_alive) {
            result.push_back(service);
        }
    }
    
    // 更新缓存
    discovery_cache_[serviceType] = result;
    cache_timestamps_[serviceType] = now;
    AURORA_LOG_DEBUG("Service discovery cache updated for type: {}, count: {}", serviceType, result.size());
    
    return result;
}

void DecentralizedDiscovery::updateHeartbeat(const std::string& serviceName) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    auto it = services_.find(serviceName);
    if (it != services_.end()) {
        it->second.last_heartbeat = std::chrono::steady_clock::now();
        it->second.is_alive = true;
    }
}

void DecentralizedDiscovery::registerServiceCallback(ServiceCallback callback) {
    service_callbacks_.push_back(callback);
}

void DecentralizedDiscovery::registerServiceLostCallback(ServiceLostCallback callback) {
    service_lost_callbacks_.push_back(callback);
}

void DecentralizedDiscovery::heartbeatMonitor() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::lock_guard<std::mutex> lock(services_mutex_);
        auto now = std::chrono::steady_clock::now();
        
        bool cache_invalidated = false;
        
        for (auto& [name, service] : services_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - service.last_heartbeat);
            if (elapsed > heartbeat_timeout_ && service.is_alive) {
                service.is_alive = false;
                AURORA_LOG_WARN("Service lost: {} (heartbeat timeout)", name);
                
                // 通知所有回调
                for (const auto& callback : service_lost_callbacks_) {
                    callback(service);
                }
                
                // 服务状态变化，需要清除缓存
                cache_invalidated = true;
            }
        }
        
        // 清除过期缓存
        if (cache_invalidated) {
            clearCache();
        }
        
        // 检查缓存是否过期
        auto cache_it = cache_timestamps_.begin();
        while (cache_it != cache_timestamps_.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - cache_it->second);
            if (elapsed > cache_timeout_) {
                discovery_cache_.erase(cache_it->first);
                cache_it = cache_timestamps_.erase(cache_it);
            } else {
                ++cache_it;
            }
        }
    }
}

void DecentralizedDiscovery::clearCache() {
    discovery_cache_.clear();
    cache_timestamps_.clear();
    AURORA_LOG_DEBUG("Service discovery cache cleared");
}

// CentralizedRouting implementation

CentralizedRouting::CentralizedRouting()
    : running_(false), heartbeat_timeout_(5), server_port_(5555), server_running_(false) {
}

void CentralizedRouting::init() {
    AURORA_LOG_INFO("Initializing centralized service discovery");
    
    // 初始化网络通信
    initServer();
}

void CentralizedRouting::start() {
    AURORA_LOG_INFO("Starting centralized service discovery");
    running_ = true;
    server_running_ = true;
    heartbeat_thread_ = std::thread(&CentralizedRouting::heartbeatMonitor, this);
    server_thread_ = std::thread(&CentralizedRouting::serverLoop, this);
    client_thread_ = std::thread(&CentralizedRouting::clientLoop, this);
}

void CentralizedRouting::stop() {
    AURORA_LOG_INFO("Stopping centralized service discovery");
    running_ = false;
    server_running_ = false;
    
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    if (client_thread_.joinable()) {
        client_thread_.join();
    }
    
    // 清理网络资源
    cleanupServer();
}

void CentralizedRouting::initServer() {
    // 初始化服务器
    #if defined(_WIN32)
    // Windows实现
    AURORA_LOG_INFO("Initializing server for Windows on port {}", server_port_);
    #elif defined(__linux__)
    // Linux实现
    AURORA_LOG_INFO("Initializing server for Linux on port {}", server_port_);
    #else
    // 其他平台
    AURORA_LOG_INFO("Initializing server for other platforms on port {}", server_port_);
    #endif
}

void CentralizedRouting::cleanupServer() {
    // 清理服务器资源
    AURORA_LOG_INFO("Cleaning up server resources");
}

void CentralizedRouting::serverLoop() {
    AURORA_LOG_INFO("Centralized server started on port {}", server_port_);
    
    while (server_running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // 服务器逻辑
        AURORA_LOG_DEBUG("Server loop running");
    }
    
    AURORA_LOG_INFO("Centralized server stopped");
}

void CentralizedRouting::clientLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        // 客户端逻辑
        AURORA_LOG_DEBUG("Client loop running");
    }
}

void CentralizedRouting::registerService(const ServiceInfo& service) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    services_[service.name] = service;
    AURORA_LOG_INFO("Service registered: {} (type: {})", service.name, service.type);
    
    // 通知所有回调
    for (const auto& callback : service_callbacks_) {
        callback(service);
    }
}

void CentralizedRouting::unregisterService(const ServiceInfo& service) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    auto it = services_.find(service.name);
    if (it != services_.end()) {
        services_.erase(it);
        AURORA_LOG_INFO("Service unregistered: {}", service.name);
    }
}

std::vector<ServiceInfo> CentralizedRouting::discoverServices(const std::string& serviceType) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    std::vector<ServiceInfo> result;
    for (const auto& [name, service] : services_) {
        if (service.type == serviceType && service.is_alive) {
            result.push_back(service);
        }
    }
    return result;
}

void CentralizedRouting::updateHeartbeat(const std::string& serviceName) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    auto it = services_.find(serviceName);
    if (it != services_.end()) {
        it->second.last_heartbeat = std::chrono::steady_clock::now();
        it->second.is_alive = true;
    }
}

void CentralizedRouting::registerServiceCallback(ServiceCallback callback) {
    service_callbacks_.push_back(callback);
}

void CentralizedRouting::registerServiceLostCallback(ServiceLostCallback callback) {
    service_lost_callbacks_.push_back(callback);
}

void CentralizedRouting::heartbeatMonitor() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::lock_guard<std::mutex> lock(services_mutex_);
        auto now = std::chrono::steady_clock::now();
        
        for (auto& [name, service] : services_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - service.last_heartbeat);
            if (elapsed > heartbeat_timeout_ && service.is_alive) {
                service.is_alive = false;
                AURORA_LOG_WARN("Service lost: {} (heartbeat timeout)", name);
                
                // 通知所有回调
                for (const auto& callback : service_lost_callbacks_) {
                    callback(service);
                }
            }
        }
    }
}

// ServiceDiscoveryManager implementation

ServiceDiscoveryManager::ServiceDiscoveryManager() 
    : hybridMode_(false), networkSizeThreshold_(50), currentMode_(DiscoveryMode::DECENTRALIZED) {
}

ServiceDiscoveryManager& ServiceDiscoveryManager::instance() {
    static ServiceDiscoveryManager instance;
    return instance;
}

void ServiceDiscoveryManager::init(DiscoveryMode mode) {
    // 初始化两种发现模式
    decentralizedDiscovery_ = std::make_shared<DecentralizedDiscovery>();
    centralizedDiscovery_ = std::make_shared<CentralizedRouting>();
    
    // 根据指定模式选择初始发现机制
    currentMode_ = mode;
    if (mode == DiscoveryMode::DECENTRALIZED) {
        discovery_ = decentralizedDiscovery_;
    } else {
        discovery_ = centralizedDiscovery_;
    }
    
    if (discovery_) {
        discovery_->init();
    }
    
    AURORA_LOG_INFO("Service discovery initialized with mode: {}", 
                   mode == DiscoveryMode::DECENTRALIZED ? "DECENTRALIZED" : "CENTRALIZED");
}

void ServiceDiscoveryManager::start() {
    if (discovery_) {
        discovery_->start();
    }
}

void ServiceDiscoveryManager::stop() {
    if (discovery_) {
        discovery_->stop();
    }
}

void ServiceDiscoveryManager::registerService(const ServiceInfo& service) {
    if (discovery_) {
        discovery_->registerService(service);
        
        // 在混合模式下，检查是否需要切换模式
        if (hybridMode_) {
            checkAndSwitchMode();
        }
    }
}

void ServiceDiscoveryManager::unregisterService(const ServiceInfo& service) {
    if (discovery_) {
        discovery_->unregisterService(service);
        
        // 在混合模式下，检查是否需要切换模式
        if (hybridMode_) {
            checkAndSwitchMode();
        }
    }
}

std::vector<ServiceInfo> ServiceDiscoveryManager::discoverServices(const std::string& serviceType) {
    if (discovery_) {
        return discovery_->discoverServices(serviceType);
    }
    return {};
}

void ServiceDiscoveryManager::updateHeartbeat(const std::string& serviceName) {
    if (discovery_) {
        discovery_->updateHeartbeat(serviceName);
    }
}

void ServiceDiscoveryManager::registerServiceCallback(ServiceCallback callback) {
    if (discovery_) {
        discovery_->registerServiceCallback(callback);
    }
}

void ServiceDiscoveryManager::registerServiceLostCallback(ServiceLostCallback callback) {
    if (discovery_) {
        discovery_->registerServiceLostCallback(callback);
    }
}

void ServiceDiscoveryManager::enableHybridMode(bool enabled) {
    hybridMode_ = enabled;
    AURORA_LOG_INFO("Hybrid mode {}", enabled ? "enabled" : "disabled");
    
    if (enabled) {
        // 初始化两种发现模式
        if (!decentralizedDiscovery_) {
            decentralizedDiscovery_ = std::make_shared<DecentralizedDiscovery>();
            decentralizedDiscovery_->init();
        }
        if (!centralizedDiscovery_) {
            centralizedDiscovery_ = std::make_shared<CentralizedRouting>();
            centralizedDiscovery_->init();
        }
        
        // 检查当前网络规模并选择合适的模式
        checkAndSwitchMode();
    }
}

void ServiceDiscoveryManager::setNetworkSizeThreshold(size_t threshold) {
    networkSizeThreshold_ = threshold;
    AURORA_LOG_INFO("Network size threshold set to: {}", threshold);
    
    if (hybridMode_) {
        checkAndSwitchMode();
    }
}

DiscoveryMode ServiceDiscoveryManager::getCurrentMode() const {
    return currentMode_;
}

void ServiceDiscoveryManager::checkAndSwitchMode() {
    // 估算当前网络规模（简化实现）
    size_t networkSize = 0;
    if (discovery_) {
        // 假设通过发现服务的数量来估算网络规模
        auto services = discovery_->discoverServices("");
        networkSize = services.size();
    }
    
    // 根据网络规模决定使用哪种发现模式
    DiscoveryMode newMode;
    if (networkSize < networkSizeThreshold_) {
        newMode = DiscoveryMode::DECENTRALIZED;
    } else {
        newMode = DiscoveryMode::CENTRALIZED;
    }
    
    // 如果模式需要切换
    if (newMode != currentMode_) {
        AURORA_LOG_INFO("Switching discovery mode from {} to {} (network size: {})",
                       currentMode_ == DiscoveryMode::DECENTRALIZED ? "DECENTRALIZED" : "CENTRALIZED",
                       newMode == DiscoveryMode::DECENTRALIZED ? "DECENTRALIZED" : "CENTRALIZED",
                       networkSize);
        
        // 停止当前发现服务
        if (discovery_) {
            discovery_->stop();
        }
        
        // 切换到新模式
        currentMode_ = newMode;
        if (newMode == DiscoveryMode::DECENTRALIZED) {
            discovery_ = decentralizedDiscovery_;
        } else {
            discovery_ = centralizedDiscovery_;
        }
        
        // 启动新的发现服务
        discovery_->start();
    }
}

} // namespace service_discovery
} // namespace aurorart