#include "aurorart/qos/qos_policy.h"
#include <sstream>

namespace aurorart {
namespace qos {

// ReliabilityPolicy implementation

void ReliabilityPolicy::apply(void* data) {
    // 应用可靠性策略
    // 实际实现：根据可靠性级别设置重传机制
    if (level_ == ReliabilityLevel::RELIABLE) {
        // 启用重传机制
        // 这里可以设置传输层的重传参数
        // 例如：设置最大重传次数、重传超时时间等
    }
}

std::string ReliabilityPolicy::toString() const {
    std::stringstream ss;
    ss << "ReliabilityPolicy: ";
    switch (level_) {
    case ReliabilityLevel::BEST_EFFORT:
        ss << "BEST_EFFORT";
        break;
    case ReliabilityLevel::RELIABLE:
        ss << "RELIABLE";
        break;
    }
    return ss.str();
}

// DurabilityPolicy implementation

void DurabilityPolicy::apply(void* data) {
    // 应用持久性策略
    // 实际实现：根据持久性级别设置存储方式
    switch (level_) {
    case DurabilityLevel::VOLATILE:
        // 仅保存在内存中，不持久化
        break;
    case DurabilityLevel::TRANSIENT_LOCAL:
        // 保存在本地内存中，进程重启后丢失
        break;
    case DurabilityLevel::TRANSIENT:
        // 保存在本地内存中，跨进程可见
        break;
    case DurabilityLevel::PERSISTENT:
        // 持久化到磁盘，系统重启后仍然存在
        break;
    }
}

std::string DurabilityPolicy::toString() const {
    std::stringstream ss;
    ss << "DurabilityPolicy: ";
    switch (level_) {
    case DurabilityLevel::VOLATILE:
        ss << "VOLATILE";
        break;
    case DurabilityLevel::TRANSIENT_LOCAL:
        ss << "TRANSIENT_LOCAL";
        break;
    case DurabilityLevel::TRANSIENT:
        ss << "TRANSIENT";
        break;
    case DurabilityLevel::PERSISTENT:
        ss << "PERSISTENT";
        break;
    }
    return ss.str();
}

// HistoryPolicy implementation

void HistoryPolicy::apply(void* data) {
    // 应用历史记录策略
    // 实际实现：根据历史记录类型设置缓存策略
    switch (kind_) {
    case HistoryKind::KEEP_LAST:
        // 只保留最近的depth_个样本
        // 这里可以设置缓存的大小和替换策略
        break;
    case HistoryKind::KEEP_ALL:
        // 保留所有样本
        // 这里需要考虑内存使用和性能影响
        break;
    }
}

std::string HistoryPolicy::toString() const {
    std::stringstream ss;
    ss << "HistoryPolicy: ";
    switch (kind_) {
    case HistoryKind::KEEP_LAST:
        ss << "KEEP_LAST(" << depth_ << ")";
        break;
    case HistoryKind::KEEP_ALL:
        ss << "KEEP_ALL";
        break;
    }
    return ss.str();
}

// LifespanPolicy implementation

void LifespanPolicy::apply(void* data) {
    // 应用生命周期策略
    // 实际实现：设置数据的生命周期
    // 这里可以为数据添加时间戳，并在处理时检查是否过期
    // 例如：在消息中添加创建时间，接收方检查是否在生命周期内
}

std::string LifespanPolicy::toString() const {
    std::stringstream ss;
    ss << "LifespanPolicy: " << duration_.count() << "s";
    return ss.str();
}

// PriorityPolicy implementation

void PriorityPolicy::apply(void* data) {
    // 应用优先级策略
    // 实际实现：设置数据的优先级
    // 这里可以将优先级值设置到数据结构中，以便传输层和处理层使用
    // 例如：在消息头中设置优先级字段，传输层根据优先级进行调度
}

std::string PriorityPolicy::toString() const {
    std::stringstream ss;
    ss << "PriorityPolicy: " << static_cast<int>(priority_);
    return ss.str();
}

// DeadlinePolicy implementation

void DeadlinePolicy::apply(void* data) {
    // 应用截止时间策略
    // 实际实现：设置数据的截止时间
    // 这里可以为数据添加截止时间戳，并在处理时检查是否超时
    // 例如：在周期性数据中设置截止时间，接收方检查是否按时到达
}

std::string DeadlinePolicy::toString() const {
    std::stringstream ss;
    ss << "DeadlinePolicy: " << period_.count() << "s";
    return ss.str();
}

// LatencyBudgetPolicy implementation

void LatencyBudgetPolicy::apply(void* data) {
    // 应用延迟预算策略
    // 实际实现：设置数据的延迟预算
    // 这里可以根据延迟预算调整传输策略，例如选择更快速的传输方式
    // 例如：对于低延迟要求的数据，选择共享内存传输而不是网络传输
}

std::string LatencyBudgetPolicy::toString() const {
    std::stringstream ss;
    ss << "LatencyBudgetPolicy: " << budget_.count() << "s";
    return ss.str();
}

// OwnershipPolicy implementation

void OwnershipPolicy::apply(void* data) {
    // 应用所有权策略
    // 实际实现：设置数据的所有权
    // 这里可以实现数据的所有权管理，确保只有一个发布者能够修改数据
    // 例如：在数据中添加所有者标识，确保只有所有者能够更新数据
}

std::string OwnershipPolicy::toString() const {
    std::stringstream ss;
    ss << "OwnershipPolicy: ";
    switch (kind_) {
    case OwnershipKind::SHARED:
        ss << "SHARED";
        break;
    case OwnershipKind::EXCLUSIVE:
        ss << "EXCLUSIVE";
        break;
    }
    return ss.str();
}

// QoSPolicyBuilder implementation

QoSPolicyBuilder& QoSPolicyBuilder::withReliability(ReliabilityPolicy::ReliabilityLevel level) {
    policies_.push_back(std::make_shared<ReliabilityPolicy>(level));
    return *this;
}

QoSPolicyBuilder& QoSPolicyBuilder::withDurability(DurabilityPolicy::DurabilityLevel level) {
    policies_.push_back(std::make_shared<DurabilityPolicy>(level));
    return *this;
}

QoSPolicyBuilder& QoSPolicyBuilder::withHistory(HistoryPolicy::HistoryKind kind, size_t depth) {
    policies_.push_back(std::make_shared<HistoryPolicy>(kind, depth));
    return *this;
}

QoSPolicyBuilder& QoSPolicyBuilder::withLifespan(const std::chrono::duration<double>& duration) {
    policies_.push_back(std::make_shared<LifespanPolicy>(duration));
    return *this;
}

QoSPolicyBuilder& QoSPolicyBuilder::withPriority(uint8_t priority) {
    policies_.push_back(std::make_shared<PriorityPolicy>(priority));
    return *this;
}

QoSPolicyBuilder& QoSPolicyBuilder::withDeadline(const std::chrono::duration<double>& period) {
    policies_.push_back(std::make_shared<DeadlinePolicy>(period));
    return *this;
}

QoSPolicyBuilder& QoSPolicyBuilder::withLatencyBudget(const std::chrono::duration<double>& budget) {
    policies_.push_back(std::make_shared<LatencyBudgetPolicy>(budget));
    return *this;
}

QoSPolicyBuilder& QoSPolicyBuilder::withOwnership(OwnershipPolicy::OwnershipKind kind) {
    policies_.push_back(std::make_shared<OwnershipPolicy>(kind));
    return *this;
}

std::vector<std::shared_ptr<QoSPolicy>> QoSPolicyBuilder::build() {
    return policies_;
}

// QoSProfile implementation

void QoSProfile::initDefaultProfile(ProfileType type) {
    QoSPolicyBuilder builder;
    
    switch (type) {
    case ProfileType::BEST_EFFORT:
        builder.withReliability(ReliabilityPolicy::ReliabilityLevel::BEST_EFFORT)
               .withDurability(DurabilityPolicy::DurabilityLevel::VOLATILE)
               .withHistory(HistoryPolicy::HistoryKind::KEEP_LAST, 1)
               .withPriority(0)
               .withLifespan(std::chrono::duration<double>(1.0));
        break;
    case ProfileType::RELIABLE:
        builder.withReliability(ReliabilityPolicy::ReliabilityLevel::RELIABLE)
               .withDurability(DurabilityPolicy::DurabilityLevel::TRANSIENT_LOCAL)
               .withHistory(HistoryPolicy::HistoryKind::KEEP_LAST, 10)
               .withPriority(128)
               .withLifespan(std::chrono::duration<double>(10.0));
        break;
    case ProfileType::REAL_TIME:
        builder.withReliability(ReliabilityPolicy::ReliabilityLevel::RELIABLE)
               .withDurability(DurabilityPolicy::DurabilityLevel::VOLATILE)
               .withHistory(HistoryPolicy::HistoryKind::KEEP_LAST, 1)
               .withPriority(255)
               .withDeadline(std::chrono::duration<double>(0.01))
               .withLatencyBudget(std::chrono::duration<double>(0.005))
               .withLifespan(std::chrono::duration<double>(0.1));
        break;
    case ProfileType::HIGH_THROUGHPUT:
        builder.withReliability(ReliabilityPolicy::ReliabilityLevel::BEST_EFFORT)
               .withDurability(DurabilityPolicy::DurabilityLevel::VOLATILE)
               .withHistory(HistoryPolicy::HistoryKind::KEEP_LAST, 1)
               .withPriority(0)
               .withLifespan(std::chrono::duration<double>(0.1));
        break;
    case ProfileType::CUSTOM:
        // 自定义配置，使用默认值
        builder.withReliability(ReliabilityPolicy::ReliabilityLevel::BEST_EFFORT)
               .withDurability(DurabilityPolicy::DurabilityLevel::VOLATILE)
               .withHistory(HistoryPolicy::HistoryKind::KEEP_LAST, 1)
               .withPriority(0)
               .withLifespan(std::chrono::duration<double>(1.0));
        break;
    }
    
    policies_ = builder.build();
}

QoSProfile::QoSProfile(ProfileType type) : type_(type) {
    initDefaultProfile(type);
}

QoSProfile::QoSProfile(const std::vector<std::shared_ptr<QoSPolicy>>& policies) 
    : type_(ProfileType::CUSTOM), policies_(policies) {
}

// QoSManager implementation

QoSManager::QoSManager() {
    // 初始化默认配置文件
    defaultProfiles_[QoSProfile::ProfileType::BEST_EFFORT] = 
        std::make_shared<QoSProfile>(QoSProfile::ProfileType::BEST_EFFORT);
    defaultProfiles_[QoSProfile::ProfileType::RELIABLE] = 
        std::make_shared<QoSProfile>(QoSProfile::ProfileType::RELIABLE);
    defaultProfiles_[QoSProfile::ProfileType::REAL_TIME] = 
        std::make_shared<QoSProfile>(QoSProfile::ProfileType::REAL_TIME);
    defaultProfiles_[QoSProfile::ProfileType::HIGH_THROUGHPUT] = 
        std::make_shared<QoSProfile>(QoSProfile::ProfileType::HIGH_THROUGHPUT);
}

QoSManager& QoSManager::instance() {
    static QoSManager instance;
    return instance;
}

void QoSManager::applyPolicies(const std::vector<std::shared_ptr<QoSPolicy>>& policies, void* data) {
    for (const auto& policy : policies) {
        policy->apply(data);
    }
}

std::vector<std::shared_ptr<QoSPolicy>> QoSManager::getOptimalPolicies(size_t dataSize, bool realTime) {
    QoSPolicyBuilder builder;
    
    if (realTime) {
        // 实时性要求高，使用可靠传输和高优先级
        builder.withReliability(ReliabilityPolicy::ReliabilityLevel::RELIABLE)
               .withPriority(255)
               .withHistory(HistoryPolicy::HistoryKind::KEEP_LAST, 1)
               .withDeadline(std::chrono::duration<double>(0.01))
               .withLatencyBudget(std::chrono::duration<double>(0.005));
    } else {
        // 非实时，使用尽力而为传输
        builder.withReliability(ReliabilityPolicy::ReliabilityLevel::BEST_EFFORT)
               .withHistory(HistoryPolicy::HistoryKind::KEEP_LAST, 10);
    }
    
    // 根据数据大小调整持久性
    if (dataSize > 1024 * 1024) {
        builder.withDurability(DurabilityPolicy::DurabilityLevel::VOLATILE);
    } else {
        builder.withDurability(DurabilityPolicy::DurabilityLevel::TRANSIENT_LOCAL);
    }
    
    return builder.build();
}

std::shared_ptr<QoSProfile> QoSManager::getProfile(QoSProfile::ProfileType type) {
    auto it = defaultProfiles_.find(type);
    if (it != defaultProfiles_.end()) {
        return it->second;
    }
    return nullptr;
}

void QoSManager::registerProfile(const std::string& name, const std::shared_ptr<QoSProfile>& profile) {
    customProfiles_[name] = profile;
}

std::shared_ptr<QoSProfile> QoSManager::getProfile(const std::string& name) {
    auto it = customProfiles_.find(name);
    if (it != customProfiles_.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace qos
} // namespace aurorart