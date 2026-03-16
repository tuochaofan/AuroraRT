#ifndef AURORART_DOMAIN_MANAGER_H
#define AURORART_DOMAIN_MANAGER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

namespace aurorart {
namespace domain {

// 域操作码定义
enum class DomainOpcode {
    CREATE_DOMAIN = 0x01,      // 创建域
    DELETE_DOMAIN = 0x02,      // 删除域
    JOIN_DOMAIN = 0x03,        // 加入域
    LEAVE_DOMAIN = 0x04,       // 离开域
    CREATE_PARTITION = 0x05,    // 创建分区
    DELETE_PARTITION = 0x06,    // 删除分区
    JOIN_PARTITION = 0x07,      // 加入分区
    LEAVE_PARTITION = 0x08,     // 离开分区
    LIST_DOMAINS = 0x09,       // 列出所有域
    LIST_PARTITIONS = 0x0A,     // 列出域内所有分区
    GET_DOMAIN_INFO = 0x0B,     // 获取域信息
    GET_PARTITION_INFO = 0x0C,   // 获取分区信息
};

// 域信息类
class DomainInfo {
public:
    DomainInfo(const std::string& name, const std::string& description = "")
        : name_(name), description_(description), active_(true) {
    }
    
    std::string getName() const { return name_; }
    std::string getDescription() const { return description_; }
    bool isActive() const { return active_; }
    
    void setDescription(const std::string& description) { description_ = description; }
    void setActive(bool active) { active_ = active; }
    
    // 分区管理
    bool addPartition(const std::string& partitionName);
    bool removePartition(const std::string& partitionName);
    std::vector<std::string> getPartitions() const;
    bool hasPartition(const std::string& partitionName) const;
    
    // 节点管理
    bool addNode(const std::string& nodeId);
    bool removeNode(const std::string& nodeId);
    std::vector<std::string> getNodes() const;
    bool hasNode(const std::string& nodeId) const;
    
private:
    std::string name_;
    std::string description_;
    std::atomic<bool> active_;
    std::unordered_map<std::string, bool> partitions_; // 分区名称 -> 是否活跃
    std::unordered_map<std::string, bool> nodes_; // 节点ID -> 是否活跃
    mutable std::mutex mutex_;
};

// 分区信息类
class PartitionInfo {
public:
    PartitionInfo(const std::string& name, const std::string& domainName, const std::string& description = "")
        : name_(name), domainName_(domainName), description_(description), active_(true) {
    }
    
    std::string getName() const { return name_; }
    std::string getDomainName() const { return domainName_; }
    std::string getDescription() const { return description_; }
    bool isActive() const { return active_; }
    
    void setDescription(const std::string& description) { description_ = description; }
    void setActive(bool active) { active_ = active; }
    
    // 节点管理
    bool addNode(const std::string& nodeId);
    bool removeNode(const std::string& nodeId);
    std::vector<std::string> getNodes() const;
    bool hasNode(const std::string& nodeId) const;
    
private:
    std::string name_;
    std::string domainName_;
    std::string description_;
    std::atomic<bool> active_;
    std::unordered_map<std::string, bool> nodes_; // 节点ID -> 是否活跃
    mutable std::mutex mutex_;
};

// 域管理器类
class DomainManager {
public:
    static DomainManager& instance();
    
    bool init();
    bool shutdown();
    
    // 域管理
    bool createDomain(const std::string& domainName, const std::string& description = "");
    bool deleteDomain(const std::string& domainName);
    bool joinDomain(const std::string& domainName, const std::string& nodeId);
    bool leaveDomain(const std::string& domainName, const std::string& nodeId);
    std::vector<std::string> listDomains();
    DomainInfo* getDomain(const std::string& domainName);
    
    // 分区管理
    bool createPartition(const std::string& domainName, const std::string& partitionName, const std::string& description = "");
    bool deletePartition(const std::string& domainName, const std::string& partitionName);
    bool joinPartition(const std::string& domainName, const std::string& partitionName, const std::string& nodeId);
    bool leavePartition(const std::string& domainName, const std::string& partitionName, const std::string& nodeId);
    std::vector<std::string> listPartitions(const std::string& domainName);
    PartitionInfo* getPartition(const std::string& domainName, const std::string& partitionName);
    
    // 节点管理
    std::vector<std::string> getNodeDomains(const std::string& nodeId);
    std::vector<std::string> getNodePartitions(const std::string& nodeId, const std::string& domainName);
    
    // 操作码处理
    bool processOpcode(DomainOpcode opcode, const std::string& data, std::string& response);
    
private:
    DomainManager();
    
    std::unordered_map<std::string, std::unique_ptr<DomainInfo>> domains_; // 域名称 -> 域信息
    std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<PartitionInfo>>> partitions_; // 域名称 -> 分区名称 -> 分区信息
    std::unordered_map<std::string, std::unordered_map<std::string, bool>> nodeDomains_; // 节点ID -> 域名称 -> 是否活跃
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, bool>>> nodePartitions_; // 节点ID -> 域名称 -> 分区名称 -> 是否活跃
    
    mutable std::mutex mutex_;
    std::atomic<bool> initialized_;
};

} // namespace domain
} // namespace aurorart

#endif // AURORART_DOMAIN_MANAGER_H