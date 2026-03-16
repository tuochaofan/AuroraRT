 #include "aurorart/domain/domain_manager.h"
#include "aurorart/utils/logger.h"
#include <sstream>

namespace aurorart {
namespace domain {

// DomainInfo实现

bool DomainInfo::addPartition(const std::string& partitionName) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (partitions_.find(partitionName) != partitions_.end()) {
        AURORA_LOG_WARN("Partition {} already exists in domain {}", partitionName, name_);
        return false;
    }
    partitions_[partitionName] = true;
    AURORA_LOG_INFO("Added partition {} to domain {}", partitionName, name_);
    return true;
}

bool DomainInfo::removePartition(const std::string& partitionName) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = partitions_.find(partitionName);
    if (it == partitions_.end()) {
        AURORA_LOG_WARN("Partition {} not found in domain {}", partitionName, name_);
        return false;
    }
    partitions_.erase(it);
    AURORA_LOG_INFO("Removed partition {} from domain {}", partitionName, name_);
    return true;
}

std::vector<std::string> DomainInfo::getPartitions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& entry : partitions_) {
        if (entry.second) {
            result.push_back(entry.first);
        }
    }
    return result;
}

bool DomainInfo::hasPartition(const std::string& partitionName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = partitions_.find(partitionName);
    return it != partitions_.end() && it->second;
}

bool DomainInfo::addNode(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_[nodeId] = true;
    AURORA_LOG_INFO("Added node {} to domain {}", nodeId, name_);
    return true;
}

bool DomainInfo::removeNode(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        AURORA_LOG_WARN("Node {} not found in domain {}", nodeId, name_);
        return false;
    }
    nodes_.erase(it);
    AURORA_LOG_INFO("Removed node {} from domain {}", nodeId, name_);
    return true;
}

std::vector<std::string> DomainInfo::getNodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& entry : nodes_) {
        if (entry.second) {
            result.push_back(entry.first);
        }
    }
    return result;
}

bool DomainInfo::hasNode(const std::string& nodeId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(nodeId);
    return it != nodes_.end() && it->second;
}

// PartitionInfo实现

bool PartitionInfo::addNode(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_[nodeId] = true;
    AURORA_LOG_INFO("Added node {} to partition {} in domain {}", nodeId, name_, domainName_);
    return true;
}

bool PartitionInfo::removeNode(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        AURORA_LOG_WARN("Node {} not found in partition {} in domain {}", nodeId, name_, domainName_);
        return false;
    }
    nodes_.erase(it);
    AURORA_LOG_INFO("Removed node {} from partition {} in domain {}", nodeId, name_, domainName_);
    return true;
}

std::vector<std::string> PartitionInfo::getNodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& entry : nodes_) {
        if (entry.second) {
            result.push_back(entry.first);
        }
    }
    return result;
}

bool PartitionInfo::hasNode(const std::string& nodeId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(nodeId);
    return it != nodes_.end() && it->second;
}

// DomainManager实现

DomainManager::DomainManager() : initialized_(false) {
}

DomainManager& DomainManager::instance() {
    static DomainManager instance;
    return instance;
}

bool DomainManager::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        AURORA_LOG_WARN("DomainManager is already initialized");
        return false;
    }
    
    // 创建默认域
    createDomain("default", "Default domain");
    
    initialized_ = true;
    AURORA_LOG_INFO("DomainManager initialized");
    return true;
}

bool DomainManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        AURORA_LOG_WARN("DomainManager is not initialized");
        return false;
    }
    
    // 清理所有域和分区
    domains_.clear();
    partitions_.clear();
    nodeDomains_.clear();
    nodePartitions_.clear();
    
    initialized_ = false;
    AURORA_LOG_INFO("DomainManager shutdown");
    return true;
}

bool DomainManager::createDomain(const std::string& domainName, const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (domains_.find(domainName) != domains_.end()) {
        AURORA_LOG_WARN("Domain {} already exists", domainName);
        return false;
    }
    
    domains_[domainName] = std::make_unique<DomainInfo>(domainName, description);
    partitions_[domainName] = std::unordered_map<std::string, std::unique_ptr<PartitionInfo>>();
    AURORA_LOG_INFO("Created domain {} with description: {}", domainName, description);
    return true;
}

bool DomainManager::deleteDomain(const std::string& domainName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (domains_.find(domainName) == domains_.end()) {
        AURORA_LOG_WARN("Domain {} not found", domainName);
        return false;
    }
    
    // 移除所有节点从该域
    auto domain = domains_[domainName].get();
    auto nodes = domain->getNodes();
    for (const auto& nodeId : nodes) {
        leaveDomain(domainName, nodeId);
    }
    
    // 移除所有分区
    partitions_.erase(domainName);
    
    // 移除域
    domains_.erase(domainName);
    AURORA_LOG_INFO("Deleted domain {}", domainName);
    return true;
}

bool DomainManager::joinDomain(const std::string& domainName, const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = domains_.find(domainName);
    if (it == domains_.end()) {
        AURORA_LOG_WARN("Domain {} not found", domainName);
        return false;
    }
    
    // 添加节点到域
    it->second->addNode(nodeId);
    
    // 更新节点-域映射
    nodeDomains_[nodeId][domainName] = true;
    AURORA_LOG_INFO("Node {} joined domain {}", nodeId, domainName);
    return true;
}

bool DomainManager::leaveDomain(const std::string& domainName, const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = domains_.find(domainName);
    if (it == domains_.end()) {
        AURORA_LOG_WARN("Domain {} not found", domainName);
        return false;
    }
    
    // 从域中移除节点
    it->second->removeNode(nodeId);
    
    // 从所有分区中移除节点
    auto partIt = partitions_.find(domainName);
    if (partIt != partitions_.end()) {
        for (const auto& partitionEntry : partIt->second) {
            partitionEntry.second->removeNode(nodeId);
        }
    }
    
    // 更新节点-域映射
    auto nodeIt = nodeDomains_.find(nodeId);
    if (nodeIt != nodeDomains_.end()) {
        nodeIt->second.erase(domainName);
        if (nodeIt->second.empty()) {
            nodeDomains_.erase(nodeIt);
        }
    }
    
    // 更新节点-分区映射
    auto nodePartIt = nodePartitions_.find(nodeId);
    if (nodePartIt != nodePartitions_.end()) {
        nodePartIt->second.erase(domainName);
        if (nodePartIt->second.empty()) {
            nodePartitions_.erase(nodePartIt);
        }
    }
    
    AURORA_LOG_INFO("Node {} left domain {}", nodeId, domainName);
    return true;
}

std::vector<std::string> DomainManager::listDomains() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& entry : domains_) {
        if (entry.second->isActive()) {
            result.push_back(entry.first);
        }
    }
    return result;
}

DomainInfo* DomainManager::getDomain(const std::string& domainName) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = domains_.find(domainName);
    if (it != domains_.end() && it->second->isActive()) {
        return it->second.get();
    }
    return nullptr;
}

bool DomainManager::createPartition(const std::string& domainName, const std::string& partitionName, const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto domainIt = domains_.find(domainName);
    if (domainIt == domains_.end()) {
        AURORA_LOG_WARN("Domain {} not found", domainName);
        return false;
    }
    
    auto& domainPartitions = partitions_[domainName];
    if (domainPartitions.find(partitionName) != domainPartitions.end()) {
        AURORA_LOG_WARN("Partition {} already exists in domain {}", partitionName, domainName);
        return false;
    }
    
    domainPartitions[partitionName] = std::make_unique<PartitionInfo>(partitionName, domainName, description);
    domainIt->second->addPartition(partitionName);
    AURORA_LOG_INFO("Created partition {} in domain {} with description: {}", partitionName, domainName, description);
    return true;
}

bool DomainManager::deletePartition(const std::string& domainName, const std::string& partitionName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto domainIt = domains_.find(domainName);
    if (domainIt == domains_.end()) {
        AURORA_LOG_WARN("Domain {} not found", domainName);
        return false;
    }
    
    auto& domainPartitions = partitions_[domainName];
    auto partitionIt = domainPartitions.find(partitionName);
    if (partitionIt == domainPartitions.end()) {
        AURORA_LOG_WARN("Partition {} not found in domain {}", partitionName, domainName);
        return false;
    }
    
    // 移除所有节点从该分区
    auto partition = partitionIt->second.get();
    auto nodes = partition->getNodes();
    for (const auto& nodeId : nodes) {
        leavePartition(domainName, partitionName, nodeId);
    }
    
    // 从域中移除分区
    domainIt->second->removePartition(partitionName);
    domainPartitions.erase(partitionIt);
    AURORA_LOG_INFO("Deleted partition {} from domain {}", partitionName, domainName);
    return true;
}

bool DomainManager::joinPartition(const std::string& domainName, const std::string& partitionName, const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto domainIt = domains_.find(domainName);
    if (domainIt == domains_.end()) {
        AURORA_LOG_WARN("Domain {} not found", domainName);
        return false;
    }
    
    auto& domainPartitions = partitions_[domainName];
    auto partitionIt = domainPartitions.find(partitionName);
    if (partitionIt == domainPartitions.end()) {
        AURORA_LOG_WARN("Partition {} not found in domain {}", partitionName, domainName);
        return false;
    }
    
    // 确保节点已加入域
    if (!domainIt->second->hasNode(nodeId)) {
        joinDomain(domainName, nodeId);
    }
    
    // 添加节点到分区
    partitionIt->second->addNode(nodeId);
    
    // 更新节点-分区映射
    nodePartitions_[nodeId][domainName][partitionName] = true;
    AURORA_LOG_INFO("Node {} joined partition {} in domain {}", nodeId, partitionName, domainName);
    return true;
}

bool DomainManager::leavePartition(const std::string& domainName, const std::string& partitionName, const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto domainIt = domains_.find(domainName);
    if (domainIt == domains_.end()) {
        AURORA_LOG_WARN("Domain {} not found", domainName);
        return false;
    }
    
    auto& domainPartitions = partitions_[domainName];
    auto partitionIt = domainPartitions.find(partitionName);
    if (partitionIt == domainPartitions.end()) {
        AURORA_LOG_WARN("Partition {} not found in domain {}", partitionName, domainName);
        return false;
    }
    
    // 从分区中移除节点
    partitionIt->second->removeNode(nodeId);
    
    // 更新节点-分区映射
    auto nodeIt = nodePartitions_.find(nodeId);
    if (nodeIt != nodePartitions_.end()) {
        auto domainPartIt = nodeIt->second.find(domainName);
        if (domainPartIt != nodeIt->second.end()) {
            domainPartIt->second.erase(partitionName);
            if (domainPartIt->second.empty()) {
                nodeIt->second.erase(domainPartIt);
            }
        }
        if (nodeIt->second.empty()) {
            nodePartitions_.erase(nodeIt);
        }
    }
    
    AURORA_LOG_INFO("Node {} left partition {} in domain {}", nodeId, partitionName, domainName);
    return true;
}

std::vector<std::string> DomainManager::listPartitions(const std::string& domainName) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    
    auto domainIt = domains_.find(domainName);
    if (domainIt == domains_.end()) {
        AURORA_LOG_WARN("Domain {} not found", domainName);
        return result;
    }
    
    return domainIt->second->getPartitions();
}

PartitionInfo* DomainManager::getPartition(const std::string& domainName, const std::string& partitionName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto domainIt = domains_.find(domainName);
    if (domainIt == domains_.end()) {
        AURORA_LOG_WARN("Domain {} not found", domainName);
        return nullptr;
    }
    
    auto& domainPartitions = partitions_[domainName];
    auto partitionIt = domainPartitions.find(partitionName);
    if (partitionIt != domainPartitions.end() && partitionIt->second->isActive()) {
        return partitionIt->second.get();
    }
    return nullptr;
}

std::vector<std::string> DomainManager::getNodeDomains(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    
    auto nodeIt = nodeDomains_.find(nodeId);
    if (nodeIt != nodeDomains_.end()) {
        for (const auto& entry : nodeIt->second) {
            if (entry.second) {
                result.push_back(entry.first);
            }
        }
    }
    return result;
}

std::vector<std::string> DomainManager::getNodePartitions(const std::string& nodeId, const std::string& domainName) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    
    auto nodeIt = nodePartitions_.find(nodeId);
    if (nodeIt != nodePartitions_.end()) {
        auto domainIt = nodeIt->second.find(domainName);
        if (domainIt != nodeIt->second.end()) {
            for (const auto& entry : domainIt->second) {
                if (entry.second) {
                    result.push_back(entry.first);
                }
            }
        }
    }
    return result;
}

bool DomainManager::processOpcode(DomainOpcode opcode, const std::string& data, std::string& response) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::istringstream iss(data);
        std::string token;
        
        switch (opcode) {
            case DomainOpcode::CREATE_DOMAIN: {
                std::string domainName, description;
                if (iss >> domainName) {
                    std::getline(iss, description);
                    if (description.size() > 0 && description[0] == ' ') {
                        description = description.substr(1);
                    }
                    bool success = createDomain(domainName, description);
                    response = success ? "SUCCESS" : "FAILED: Domain already exists";
                } else {
                    response = "FAILED: Invalid data format";
                }
                break;
            }
            case DomainOpcode::DELETE_DOMAIN: {
                std::string domainName;
                if (iss >> domainName) {
                    bool success = deleteDomain(domainName);
                    response = success ? "SUCCESS" : "FAILED: Domain not found";
                } else {
                    response = "FAILED: Invalid data format";
                }
                break;
            }
            case DomainOpcode::JOIN_DOMAIN: {
                std::string domainName, nodeId;
                if (iss >> domainName >> nodeId) {
                    bool success = joinDomain(domainName, nodeId);
                    response = success ? "SUCCESS" : "FAILED: Domain not found";
                } else {
                    response = "FAILED: Invalid data format";
                }
                break;
            }
            case DomainOpcode::LEAVE_DOMAIN: {
                std::string domainName, nodeId;
                if (iss >> domainName >> nodeId) {
                    bool success = leaveDomain(domainName, nodeId);
                    response = success ? "SUCCESS" : "FAILED: Domain or node not found";
                } else {
                    response = "FAILED: Invalid data format";
                }
                break;
            }
            case DomainOpcode::CREATE_PARTITION: {
                std::string domainName, partitionName, description;
                if (iss >> domainName >> partitionName) {
                    std::getline(iss, description);
                    if (description.size() > 0 && description[0] == ' ') {
                        description = description.substr(1);
                    }
                    bool success = createPartition(domainName, partitionName, description);
                    response = success ? "SUCCESS" : "FAILED: Domain not found or partition already exists";
                } else {
                    response = "FAILED: Invalid data format";
                }
                break;
            }
            case DomainOpcode::DELETE_PARTITION: {
                std::string domainName, partitionName;
                if (iss >> domainName >> partitionName) {
                    bool success = deletePartition(domainName, partitionName);
                    response = success ? "SUCCESS" : "FAILED: Domain or partition not found";
                } else {
                    response = "FAILED: Invalid data format";
                }
                break;
            }
            case DomainOpcode::JOIN_PARTITION: {
                std::string domainName, partitionName, nodeId;
                if (iss >> domainName >> partitionName >> nodeId) {
                    bool success = joinPartition(domainName, partitionName, nodeId);
                    response = success ? "SUCCESS" : "FAILED: Domain or partition not found";
                } else {
                    response = "FAILED: Invalid data format";
                }
                break;
            }
            case DomainOpcode::LEAVE_PARTITION: {
                std::string domainName, partitionName, nodeId;
                if (iss >> domainName >> partitionName >> nodeId) {
                    bool success = leavePartition(domainName, partitionName, nodeId);
                    response = success ? "SUCCESS" : "FAILED: Domain, partition, or node not found";
                } else {
                    response = "FAILED: Invalid data format";
                }
                break;
            }
            case DomainOpcode::LIST_DOMAINS: {
                auto domains = listDomains();
                std::ostringstream oss;
                oss << "SUCCESS";
                for (const auto& domain : domains) {
                    oss << " " << domain;
                }
                response = oss.str();
                break;
            }
            case DomainOpcode::LIST_PARTITIONS: {
                std::string domainName;
                if (iss >> domainName) {
                    auto partitions = listPartitions(domainName);
                    std::ostringstream oss;
                    oss << "SUCCESS";
                    for (const auto& partition : partitions) {
                        oss << " " << partition;
                    }
                    response = oss.str();
                } else {
                    response = "FAILED: Invalid data format";
                }
                break;
            }
            case DomainOpcode::GET_DOMAIN_INFO: {
                std::string domainName;
                if (iss >> domainName) {
                    auto domain = getDomain(domainName);
                    if (domain) {
                        std::ostringstream oss;
                        oss << "SUCCESS " << domain->getName() << " " << domain->getDescription() << " " << (domain->isActive() ? "active" : "inactive");
                        response = oss.str();
                    } else {
                        response = "FAILED: Domain not found";
                    }
                } else {
                    response = "FAILED: Invalid data format";
                }
                break;
            }
            case DomainOpcode::GET_PARTITION_INFO: {
                std::string domainName, partitionName;
                if (iss >> domainName >> partitionName) {
                    auto partition = getPartition(domainName, partitionName);
                    if (partition) {
                        std::ostringstream oss;
                        oss << "SUCCESS " << partition->getName() << " " << partition->getDescription() << " " << (partition->isActive() ? "active" : "inactive");
                        response = oss.str();
                    } else {
                        response = "FAILED: Domain or partition not found";
                    }
                } else {
                    response = "FAILED: Invalid data format";
                }
                break;
            }
            default:
                response = "FAILED: Unknown opcode";
                break;
        }
        
        AURORA_LOG_DEBUG("Processed opcode {} with data '{}', response: '{}'", static_cast<int>(opcode), data, response);
        return true;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("Error processing opcode {}: {}", static_cast<int>(opcode), e.what());
        response = "FAILED: Internal error";
        return false;
    }
}

} // namespace domain
} // namespace aurorart