#ifndef AURORART_NODE_H
#define AURORART_NODE_H

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace aurorart {
namespace node {

enum class NodeState {
    UNINITIALIZED,
    INITIALIZED,
    RUNNING,
    STOPPED,
    ERROR
};

using NodeId = std::string;

class Node {
public:
    virtual ~Node() = default;
    
    virtual bool init() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    
    virtual NodeId getID() const = 0;
    virtual std::string getName() const = 0;
    virtual NodeState getState() const = 0;
    
    virtual bool isAvailable() const = 0;
    virtual std::string getStatus() const = 0;
};

class DefaultNode : public Node {
public:
    DefaultNode(const std::string& name);
    
    bool init() override;
    bool start() override;
    bool stop() override;
    
    NodeId getID() const override;
    std::string getName() const override;
    NodeState getState() const override;
    
    bool isAvailable() const override;
    std::string getStatus() const override;
    
    void updateHeartbeat();
    bool isHeartbeatExpired() const;
    
    void setMetadata(const std::string& key, const std::string& value);
    std::string getMetadata(const std::string& key) const;
    void updateLoad(double cpu_load, double memory_usage);
    double getCpuLoad() const;
    double getMemoryUsage() const;
    uint64_t getUptime() const;
    
private:
    NodeId id_;
    std::string name_;
    std::atomic<NodeState> state_;
    std::chrono::steady_clock::time_point lastHeartbeat_;
    std::chrono::steady_clock::time_point start_time_;
    std::map<std::string, std::string> metadata_;
    std::atomic<double> cpu_load_;
    std::atomic<double> memory_usage_;
    std::atomic<uint64_t> uptime_;
};

class NodeDiscovery {
public:
    virtual ~NodeDiscovery() = default;
    
    virtual void start() = 0;
    virtual void stop() = 0;
    
    virtual std::vector<NodeId> discoverNodes() = 0;
    virtual void registerNode(const NodeId& nodeId, const std::string& name) = 0;
    virtual void unregisterNode(const NodeId& nodeId) = 0;
};

class NodeMonitor {
public:
    virtual ~NodeMonitor() = default;
    
    virtual void start() = 0;
    virtual void stop() = 0;
    
    virtual void addNode(const std::shared_ptr<Node>& node) = 0;
    virtual void removeNode(const NodeId& nodeId) = 0;
    virtual bool isNodeHealthy(const NodeId& nodeId) const = 0;
    virtual std::map<NodeId, bool> getNodeHealthStatus() const = 0;
};

class NodeManager {
public:
    static NodeManager& instance();
    
    void init();
    void start();
    void stop();
    
    std::shared_ptr<Node> createNode(const std::string& name);
    std::shared_ptr<Node> getNode(const NodeId& nodeId) const;
    bool removeNode(const NodeId& nodeId);
    
    std::vector<NodeId> discoverNodes();
    std::map<NodeId, bool> getNodeHealthStatus() const;
    void updateNodeHeartbeat(const NodeId& nodeId);
    
    void updateNodeMetadata(const NodeId& nodeId, const std::string& key, const std::string& value);
    void updateNodeLoad(const NodeId& nodeId, double cpu_load, double memory_usage);
    std::map<NodeId, std::pair<double, double>> getNodeLoadStatus() const;
    
    void setMulticastEnabled(bool enabled);
    void setMulticastAddress(const std::string& addr);
    void setMulticastPort(int port);
    
    void setCpuThreshold(double threshold);
    void setMemoryThreshold(double threshold);
    
private:
    NodeManager();
    
    std::map<NodeId, std::shared_ptr<Node>> nodes_;
    std::unique_ptr<NodeDiscovery> discovery_;
    std::unique_ptr<NodeMonitor> monitor_;
    std::mutex mutex_;
    std::atomic<bool> running_;
};

} // namespace node
} // namespace aurorart

#endif // AURORART_NODE_H
