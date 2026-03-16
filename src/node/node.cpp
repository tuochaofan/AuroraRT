#include "aurorart/node/node.h"
#include "aurorart/utils/logger.h"
#include "aurorart/utils/config.h"
#include "aurorart/transport/transport.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <unordered_map>

namespace aurorart {
namespace node {

// 节点事件监听器接口
class NodeEventListener {
public:
    virtual ~NodeEventListener() = default;
    virtual void onNodeAdded(const NodeId& nodeId, const std::string& name) = 0;
    virtual void onNodeRemoved(const NodeId& nodeId) = 0;
    virtual void onNodeStatusChanged(const NodeId& nodeId, NodeState oldState, NodeState newState) = 0;
    virtual void onNodeHealthChanged(const NodeId& nodeId, bool isHealthy) = 0;
    virtual void onNodeMetadataChanged(const NodeId& nodeId, const std::string& key, const std::string& value) = 0;
    virtual void onNodeLoadChanged(const NodeId& nodeId, double load) = 0;
};

// DefaultNode implementation

DefaultNode::DefaultNode(const std::string& name)
    : name_(name), state_(NodeState::UNINITIALIZED), 
      cpu_load_(0.0), memory_usage_(0.0), uptime_(0) {
    // 生成唯一的节点ID（使用时间戳和随机数）
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    // 添加时间戳前缀，确保唯一性
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    ss << std::setw(16) << now;
    
    // 添加随机后缀
    for (int i = 0; i < 8; i++) {
        ss << std::setw(2) << dis(gen);
    }
    
    id_ = ss.str();
    lastHeartbeat_ = std::chrono::steady_clock::now();
    start_time_ = std::chrono::steady_clock::now();
    
    AURORA_LOG_INFO("Created node with ID: {}, name: {}", id_, name_);
}

bool DefaultNode::init() {
    if (state_ != NodeState::UNINITIALIZED) {
        AURORA_LOG_WARN("Node {} is already initialized", id_);
        return false;
    }
    
    state_ = NodeState::INITIALIZED;
    AURORA_LOG_INFO("Node {} initialized", id_);
    return true;
}

bool DefaultNode::start() {
    if (state_ != NodeState::INITIALIZED) {
        AURORA_LOG_WARN("Node {} is not initialized", id_);
        return false;
    }
    
    state_ = NodeState::RUNNING;
    lastHeartbeat_ = std::chrono::steady_clock::now();
    AURORA_LOG_INFO("Node {} started", id_);
    return true;
}

bool DefaultNode::stop() {
    if (state_ != NodeState::RUNNING) {
        AURORA_LOG_WARN("Node {} is not running", id_);
        return false;
    }
    
    state_ = NodeState::STOPPED;
    AURORA_LOG_INFO("Node {} stopped", id_);
    return true;
}

void DefaultNode::updateHeartbeat() {
    lastHeartbeat_ = std::chrono::steady_clock::now();
}

bool DefaultNode::isHeartbeatExpired() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat_).count();
    return elapsed > 30; // 30秒心跳过期
}

NodeId DefaultNode::getID() const {
    return id_;
}

std::string DefaultNode::getName() const {
    return name_;
}

NodeState DefaultNode::getState() const {
    return state_;
}

bool DefaultNode::isAvailable() const {
    return state_ == NodeState::RUNNING && !isHeartbeatExpired();
}

std::string DefaultNode::getStatus() const {
    switch (state_) {
        case NodeState::UNINITIALIZED:
            return "UNINITIALIZED";
        case NodeState::INITIALIZED:
            return "INITIALIZED";
        case NodeState::RUNNING:
            return isHeartbeatExpired() ? "RUNNING (HEARTBEAT EXPIRED)" : "RUNNING";
        case NodeState::STOPPED:
            return "STOPPED";
        case NodeState::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

void DefaultNode::setMetadata(const std::string& key, const std::string& value) {
    metadata_[key] = value;
}

std::string DefaultNode::getMetadata(const std::string& key) const {
    auto it = metadata_.find(key);
    if (it != metadata_.end()) {
        return it->second;
    }
    return "";
}

void DefaultNode::updateLoad(double cpu_load, double memory_usage) {
    cpu_load_.store(cpu_load);
    memory_usage_.store(memory_usage);
    
    // 更新运行时间
    auto now = std::chrono::steady_clock::now();
    uptime_.store(std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count());
}

double DefaultNode::getCpuLoad() const {
    return cpu_load_.load();
}

double DefaultNode::getMemoryUsage() const {
    return memory_usage_.load();
}

uint64_t DefaultNode::getUptime() const {
    return uptime_.load();
}

// NodeDiscovery implementation

class DefaultNodeDiscovery : public NodeDiscovery {
public:
    DefaultNodeDiscovery() : multicast_enabled_(false), multicast_port_(5555), multicast_addr_("239.255.0.1") {
    }
    
    void start() override {
        running_ = true;
        
        // 启动发现线程
        discovery_thread_ = std::thread([this]() {
            while (running_) {
                // 执行节点发现和心跳检测
                discoverNodesInternal();
                // 发送多播心跳
                sendMulticastHeartbeat();
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        });
        
        // 启动多播接收线程
        if (multicast_enabled_) {
            startMulticastReceiver();
        }
        
        AURORA_LOG_INFO("Node discovery started");
    }
    
    void stop() override {
        running_ = false;
        if (discovery_thread_.joinable()) {
            discovery_thread_.join();
        }
        if (multicast_receiver_thread_.joinable()) {
            multicast_receiver_thread_.join();
        }
        AURORA_LOG_INFO("Node discovery stopped");
    }
    
    std::vector<NodeId> discoverNodes() override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<NodeId> result;
        for (const auto& node : registered_nodes_) {
            result.push_back(node.first);
        }
        return result;
    }
    
    void registerNode(const NodeId& nodeId, const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);
        registered_nodes_[nodeId] = {name, std::chrono::steady_clock::now(), {}, 0.0, 0.0, 0};
        AURORA_LOG_INFO("Registered node: {} ({})", nodeId, name);
        
        // 通知监听器
        for (auto& listener : listeners_) {
            listener->onNodeAdded(nodeId, name);
        }
    }
    
    void unregisterNode(const NodeId& nodeId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registered_nodes_.find(nodeId);
        if (it != registered_nodes_.end()) {
            registered_nodes_.erase(it);
            AURORA_LOG_INFO("Unregistered node: {}", nodeId);
            
            // 通知监听器
            for (auto& listener : listeners_) {
                listener->onNodeRemoved(nodeId);
            }
        }
    }
    
    void addEventListener(std::shared_ptr<NodeEventListener> listener) {
        std::lock_guard<std::mutex> lock(mutex_);
        listeners_.push_back(listener);
    }
    
    void removeEventListener(std::shared_ptr<NodeEventListener> listener) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find(listeners_.begin(), listeners_.end(), listener);
        if (it != listeners_.end()) {
            listeners_.erase(it);
        }
    }
    
    void updateNodeHeartbeat(const NodeId& nodeId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registered_nodes_.find(nodeId);
        if (it != registered_nodes_.end()) {
            it->second.last_heartbeat = std::chrono::steady_clock::now();
        }
    }
    
    void updateNodeMetadata(const NodeId& nodeId, const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registered_nodes_.find(nodeId);
        if (it != registered_nodes_.end()) {
            it->second.metadata[key] = value;
            // 通知监听器
            for (auto& listener : listeners_) {
                listener->onNodeMetadataChanged(nodeId, key, value);
            }
        }
    }
    
    void updateNodeLoad(const NodeId& nodeId, double cpu_load, double memory_usage) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registered_nodes_.find(nodeId);
        if (it != registered_nodes_.end()) {
            it->second.cpu_load = cpu_load;
            it->second.memory_usage = memory_usage;
            // 通知监听器
            for (auto& listener : listeners_) {
                listener->onNodeLoadChanged(nodeId, cpu_load);
            }
        }
    }
    
    void setMulticastEnabled(bool enabled) {
        multicast_enabled_ = enabled;
    }
    
    void setMulticastAddress(const std::string& addr) {
        multicast_addr_ = addr;
    }
    
    void setMulticastPort(int port) {
        multicast_port_ = port;
    }
    
private:
    struct NodeInfo {
        std::string name;
        std::chrono::steady_clock::time_point last_heartbeat;
        std::map<std::string, std::string> metadata;
        double cpu_load;
        double memory_usage;
        uint64_t uptime;
    };
    
    void discoverNodesInternal() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 检查节点心跳
        auto now = std::chrono::steady_clock::now();
        std::vector<NodeId> expired_nodes;
        
        for (const auto& node : registered_nodes_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - node.second.last_heartbeat).count();
            if (elapsed > 30) { // 30秒心跳过期
                expired_nodes.push_back(node.first);
            }
        }
        
        // 移除过期节点
        for (const auto& nodeId : expired_nodes) {
            AURORA_LOG_WARN("Node heartbeat expired: {}", nodeId);
            registered_nodes_.erase(nodeId);
            
            // 通知监听器
            for (auto& listener : listeners_) {
                listener->onNodeRemoved(nodeId);
            }
        }
    }
    
    void startMulticastReceiver() {
        multicast_receiver_thread_ = std::thread([this]() {
            // 模拟多播接收
            while (running_) {
                // 实际应用中应该使用套接字接收多播消息
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    }
    
    void sendMulticastHeartbeat() {
        if (!multicast_enabled_) return;
        
        // 模拟多播心跳发送
        // 实际应用中应该使用套接字发送多播消息
    }
    
    std::atomic<bool> running_ = false;
    std::thread discovery_thread_;
    std::thread multicast_receiver_thread_;
    std::mutex mutex_;
    std::unordered_map<NodeId, NodeInfo> registered_nodes_;
    std::vector<std::shared_ptr<NodeEventListener>> listeners_;
    
    // 多播相关配置
    bool multicast_enabled_;
    int multicast_port_;
    std::string multicast_addr_;
};

// NodeMonitor implementation

class DefaultNodeMonitor : public NodeMonitor, public NodeEventListener {
public:
    DefaultNodeMonitor() : cpu_threshold_(80.0), memory_threshold_(90.0) {
    }
    
    void start() override {
        running_ = true;
        monitor_thread_ = std::thread([this]() {
            while (running_) {
                // 执行节点健康检查
                checkNodeHealth();
                // 检查节点负载
                checkNodeLoad();
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        });
        AURORA_LOG_INFO("Node monitor started");
    }
    
    void stop() override {
        running_ = false;
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
        AURORA_LOG_INFO("Node monitor stopped");
    }
    
    void addNode(const std::shared_ptr<Node>& node) override {
        std::lock_guard<std::mutex> lock(mutex_);
        nodes_[node->getID()] = node;
        node_health_[node->getID()] = true;
        node_loads_[node->getID()] = {0.0, 0.0};
        AURORA_LOG_INFO("Added node to monitor: {}", node->getID());
    }
    
    void removeNode(const NodeId& nodeId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        nodes_.erase(nodeId);
        node_health_.erase(nodeId);
        node_loads_.erase(nodeId);
        AURORA_LOG_INFO("Removed node from monitor: {}", nodeId);
    }
    
    bool isNodeHealthy(const NodeId& nodeId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = node_health_.find(nodeId);
        return it != node_health_.end() && it->second;
    }
    
    std::map<NodeId, bool> getNodeHealthStatus() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return node_health_;
    }
    
    std::map<NodeId, std::pair<double, double>> getNodeLoadStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return node_loads_;
    }
    
    void setCpuThreshold(double threshold) {
        cpu_threshold_ = threshold;
    }
    
    void setMemoryThreshold(double threshold) {
        memory_threshold_ = threshold;
    }
    
    // NodeEventListener implementation
    void onNodeAdded(const NodeId& nodeId, const std::string& name) override {
        AURORA_LOG_INFO("Node added event: {} ({})", nodeId, name);
    }
    
    void onNodeRemoved(const NodeId& nodeId) override {
        AURORA_LOG_INFO("Node removed event: {}", nodeId);
        removeNode(nodeId);
    }
    
    void onNodeStatusChanged(const NodeId& nodeId, NodeState oldState, NodeState newState) override {
        AURORA_LOG_INFO("Node status changed: {} from {} to {}", nodeId, 
                      getStateString(oldState), getStateString(newState));
    }
    
    void onNodeHealthChanged(const NodeId& nodeId, bool isHealthy) override {
        AURORA_LOG_INFO("Node health changed: {} to {}", nodeId, isHealthy ? "HEALTHY" : "UNHEALTHY");
    }
    
    void onNodeMetadataChanged(const NodeId& nodeId, const std::string& key, const std::string& value) override {
        AURORA_LOG_INFO("Node metadata changed: {} {}={}", nodeId, key, value);
    }
    
    void onNodeLoadChanged(const NodeId& nodeId, double load) override {
        AURORA_LOG_INFO("Node load changed: {} to {:.2f}%", nodeId, load);
    }
    
private:
    struct LoadInfo {
        double cpu_load;
        double memory_usage;
    };
    
    void checkNodeHealth() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [nodeId, node] : nodes_) {
            bool is_healthy = node->isAvailable();
            auto it = node_health_.find(nodeId);
            
            if (it != node_health_.end() && it->second != is_healthy) {
                node_health_[nodeId] = is_healthy;
                AURORA_LOG_INFO("Node health status changed: {} is now {}", 
                              nodeId, is_healthy ? "HEALTHY" : "UNHEALTHY");
            } else if (it == node_health_.end()) {
                node_health_[nodeId] = is_healthy;
            }
        }
    }
    
    void checkNodeLoad() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [nodeId, node] : nodes_) {
            auto default_node = dynamic_cast<DefaultNode*>(node.get());
            if (default_node) {
                double cpu_load = default_node->getCpuLoad();
                double memory_usage = default_node->getMemoryUsage();
                
                node_loads_[nodeId] = {cpu_load, memory_usage};
                
                // 检查负载阈值
                if (cpu_load > cpu_threshold_ || memory_usage > memory_threshold_) {
                    AURORA_LOG_WARN("Node {} load warning: CPU={:.2f}%, Memory={:.2f}%", 
                                  nodeId, cpu_load, memory_usage);
                }
            }
        }
    }
    
    std::string getStateString(NodeState state) {
        switch (state) {
            case NodeState::UNINITIALIZED:
                return "UNINITIALIZED";
            case NodeState::INITIALIZED:
                return "INITIALIZED";
            case NodeState::RUNNING:
                return "RUNNING";
            case NodeState::STOPPED:
                return "STOPPED";
            case NodeState::ERROR:
                return "ERROR";
            default:
                return "UNKNOWN";
        }
    }
    
    std::atomic<bool> running_ = false;
    std::thread monitor_thread_;
    std::mutex mutex_;
    std::unordered_map<NodeId, std::shared_ptr<Node>> nodes_;
    std::map<NodeId, bool> node_health_;
    std::map<NodeId, LoadInfo> node_loads_;
    
    // 负载阈值
    double cpu_threshold_;
    double memory_threshold_;
};

// NodeManager implementation

NodeManager::NodeManager() : running_(false) {
}

NodeManager& NodeManager::instance() {
    static NodeManager instance;
    return instance;
}

void NodeManager::init() {
    discovery_ = std::make_unique<DefaultNodeDiscovery>();
    monitor_ = std::make_unique<DefaultNodeMonitor>();
    
    // 将监控器添加为发现服务的事件监听器
    auto discovery = dynamic_cast<DefaultNodeDiscovery*>(discovery_.get());
    auto monitor = dynamic_cast<DefaultNodeMonitor*>(monitor_.get());
    if (discovery && monitor) {
        discovery->addEventListener(monitor);
    }
    
    AURORA_LOG_INFO("Node manager initialized");
}

void NodeManager::start() {
    if (running_) {
        AURORA_LOG_WARN("Node manager is already running");
        return;
    }
    
    discovery_->start();
    monitor_->start();
    running_ = true;
    AURORA_LOG_INFO("Node manager started");
}

void NodeManager::stop() {
    if (!running_) {
        AURORA_LOG_WARN("Node manager is not running");
        return;
    }
    
    monitor_->stop();
    discovery_->stop();
    
    // 停止所有节点
    for (auto& [id, node] : nodes_) {
        node->stop();
    }
    
    nodes_.clear();
    running_ = false;
    AURORA_LOG_INFO("Node manager stopped");
}

std::shared_ptr<Node> NodeManager::createNode(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto node = std::make_shared<DefaultNode>(name);
    node->init();
    nodes_[node->getID()] = node;
    monitor_->addNode(node);
    discovery_->registerNode(node->getID(), name);
    
    AURORA_LOG_INFO("Created node: {} ({})", node->getID(), name);
    return node;
}

std::shared_ptr<Node> NodeManager::getNode(const NodeId& nodeId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(nodeId);
    if (it != nodes_.end()) {
        return it->second;
    }
    
    AURORA_LOG_WARN("Node not found: {}", nodeId);
    return nullptr;
}

bool NodeManager::removeNode(const NodeId& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(nodeId);
    if (it != nodes_.end()) {
        it->second->stop();
        monitor_->removeNode(nodeId);
        discovery_->unregisterNode(nodeId);
        nodes_.erase(it);
        AURORA_LOG_INFO("Removed node: {}", nodeId);
        return true;
    }
    
    AURORA_LOG_WARN("Node not found: {}", nodeId);
    return false;
}

std::vector<NodeId> NodeManager::discoverNodes() {
    return discovery_->discoverNodes();
}

std::map<NodeId, bool> NodeManager::getNodeHealthStatus() const {
    return monitor_->getNodeHealthStatus();
}

void NodeManager::updateNodeHeartbeat(const NodeId& nodeId) {
    auto discovery = dynamic_cast<DefaultNodeDiscovery*>(discovery_.get());
    if (discovery) {
        discovery->updateNodeHeartbeat(nodeId);
    }
    
    auto node = getNode(nodeId);
    if (node) {
        auto default_node = dynamic_cast<DefaultNode*>(node.get());
        if (default_node) {
            default_node->updateHeartbeat();
        }
    }
}

void NodeManager::updateNodeMetadata(const NodeId& nodeId, const std::string& key, const std::string& value) {
    auto discovery = dynamic_cast<DefaultNodeDiscovery*>(discovery_.get());
    if (discovery) {
        discovery->updateNodeMetadata(nodeId, key, value);
    }
    
    auto node = getNode(nodeId);
    if (node) {
        auto default_node = dynamic_cast<DefaultNode*>(node.get());
        if (default_node) {
            default_node->setMetadata(key, value);
        }
    }
}

void NodeManager::updateNodeLoad(const NodeId& nodeId, double cpu_load, double memory_usage) {
    auto discovery = dynamic_cast<DefaultNodeDiscovery*>(discovery_.get());
    if (discovery) {
        discovery->updateNodeLoad(nodeId, cpu_load, memory_usage);
    }
    
    auto node = getNode(nodeId);
    if (node) {
        auto default_node = dynamic_cast<DefaultNode*>(node.get());
        if (default_node) {
            default_node->updateLoad(cpu_load, memory_usage);
        }
    }
}

std::map<NodeId, std::pair<double, double>> NodeManager::getNodeLoadStatus() const {
    auto monitor = dynamic_cast<DefaultNodeMonitor*>(monitor_.get());
    if (monitor) {
        return monitor->getNodeLoadStatus();
    }
    return {};
}

void NodeManager::setMulticastEnabled(bool enabled) {
    auto discovery = dynamic_cast<DefaultNodeDiscovery*>(discovery_.get());
    if (discovery) {
        discovery->setMulticastEnabled(enabled);
    }
}

void NodeManager::setMulticastAddress(const std::string& addr) {
    auto discovery = dynamic_cast<DefaultNodeDiscovery*>(discovery_.get());
    if (discovery) {
        discovery->setMulticastAddress(addr);
    }
}

void NodeManager::setMulticastPort(int port) {
    auto discovery = dynamic_cast<DefaultNodeDiscovery*>(discovery_.get());
    if (discovery) {
        discovery->setMulticastPort(port);
    }
}

void NodeManager::setCpuThreshold(double threshold) {
    auto monitor = dynamic_cast<DefaultNodeMonitor*>(monitor_.get());
    if (monitor) {
        monitor->setCpuThreshold(threshold);
    }
}

void NodeManager::setMemoryThreshold(double threshold) {
    auto monitor = dynamic_cast<DefaultNodeMonitor*>(monitor_.get());
    if (monitor) {
        monitor->setMemoryThreshold(threshold);
    }
}

} // namespace node
} // namespace aurorart
