#ifndef AURORART_QOS_POLICY_H
#define AURORART_QOS_POLICY_H

#include <vector>
#include <memory>
#include <chrono>
#include <map>
#include <string>

namespace aurorart {
namespace qos {

enum class QoSType {
    RELIABILITY,
    DURABILITY,
    HISTORY,
    LIFESPAN,
    PRIORITY,
    DEADLINE,
    LATENCY_BUDGET,
    OWNERSHIP
};

class QoSPolicy {
public:
    virtual ~QoSPolicy() = default;
    
    virtual QoSType getType() const = 0;
    virtual void apply(void* data) = 0;
    virtual std::string toString() const = 0;
};

class ReliabilityPolicy : public QoSPolicy {
public:
    enum class ReliabilityLevel {
        BEST_EFFORT,
        RELIABLE
    };
    
    ReliabilityPolicy(ReliabilityLevel level) : level_(level) {}
    QoSType getType() const override { return QoSType::RELIABILITY; }
    void apply(void* data) override;
    std::string toString() const override;
    
    ReliabilityLevel getLevel() const { return level_; }
    
private:
    ReliabilityLevel level_;
};

class DurabilityPolicy : public QoSPolicy {
public:
    enum class DurabilityLevel {
        VOLATILE,
        TRANSIENT_LOCAL,
        TRANSIENT,
        PERSISTENT
    };
    
    DurabilityPolicy(DurabilityLevel level) : level_(level) {}
    QoSType getType() const override { return QoSType::DURABILITY; }
    void apply(void* data) override;
    std::string toString() const override;
    
    DurabilityLevel getLevel() const { return level_; }
    
private:
    DurabilityLevel level_;
};

class HistoryPolicy : public QoSPolicy {
public:
    enum class HistoryKind {
        KEEP_LAST,
        KEEP_ALL
    };
    
    HistoryPolicy(HistoryKind kind, size_t depth) : kind_(kind), depth_(depth) {}
    QoSType getType() const override { return QoSType::HISTORY; }
    void apply(void* data) override;
    std::string toString() const override;
    
    HistoryKind getKind() const { return kind_; }
    size_t getDepth() const { return depth_; }
    
private:
    HistoryKind kind_;
    size_t depth_;
};

class LifespanPolicy : public QoSPolicy {
public:
    LifespanPolicy(const std::chrono::duration<double>& duration) : duration_(duration) {}
    QoSType getType() const override { return QoSType::LIFESPAN; }
    void apply(void* data) override;
    std::string toString() const override;
    
    std::chrono::duration<double> getDuration() const { return duration_; }
    
private:
    std::chrono::duration<double> duration_;
};

class PriorityPolicy : public QoSPolicy {
public:
    PriorityPolicy(uint8_t priority) : priority_(priority) {}
    QoSType getType() const override { return QoSType::PRIORITY; }
    void apply(void* data) override;
    std::string toString() const override;
    
    uint8_t getPriority() const { return priority_; }
    
private:
    uint8_t priority_;
};

class DeadlinePolicy : public QoSPolicy {
public:
    DeadlinePolicy(const std::chrono::duration<double>& period) : period_(period) {}
    QoSType getType() const override { return QoSType::DEADLINE; }
    void apply(void* data) override;
    std::string toString() const override;
    
    std::chrono::duration<double> getPeriod() const { return period_; }
    
private:
    std::chrono::duration<double> period_;
};

class LatencyBudgetPolicy : public QoSPolicy {
public:
    LatencyBudgetPolicy(const std::chrono::duration<double>& budget) : budget_(budget) {}
    QoSType getType() const override { return QoSType::LATENCY_BUDGET; }
    void apply(void* data) override;
    std::string toString() const override;
    
    std::chrono::duration<double> getBudget() const { return budget_; }
    
private:
    std::chrono::duration<double> budget_;
};

class OwnershipPolicy : public QoSPolicy {
public:
    enum class OwnershipKind {
        SHARED,
        EXCLUSIVE
    };
    
    OwnershipPolicy(OwnershipKind kind) : kind_(kind) {}
    QoSType getType() const override { return QoSType::OWNERSHIP; }
    void apply(void* data) override;
    std::string toString() const override;
    
    OwnershipKind getKind() const { return kind_; }
    
private:
    OwnershipKind kind_;
};

class QoSPolicyBuilder {
public:
    QoSPolicyBuilder& withReliability(ReliabilityPolicy::ReliabilityLevel level);
    QoSPolicyBuilder& withDurability(DurabilityPolicy::DurabilityLevel level);
    QoSPolicyBuilder& withHistory(HistoryPolicy::HistoryKind kind, size_t depth);
    QoSPolicyBuilder& withLifespan(const std::chrono::duration<double>& duration);
    QoSPolicyBuilder& withPriority(uint8_t priority);
    QoSPolicyBuilder& withDeadline(const std::chrono::duration<double>& period);
    QoSPolicyBuilder& withLatencyBudget(const std::chrono::duration<double>& budget);
    QoSPolicyBuilder& withOwnership(OwnershipPolicy::OwnershipKind kind);
    std::vector<std::shared_ptr<QoSPolicy>> build();
    
private:
    std::vector<std::shared_ptr<QoSPolicy>> policies_;
};

class QoSProfile {
public:
    enum class ProfileType {
        BEST_EFFORT,
        RELIABLE,
        REAL_TIME,
        HIGH_THROUGHPUT,
        CUSTOM
    };
    
    QoSProfile(ProfileType type);
    QoSProfile(const std::vector<std::shared_ptr<QoSPolicy>>& policies);
    
    const std::vector<std::shared_ptr<QoSPolicy>>& getPolicies() const { return policies_; }
    ProfileType getType() const { return type_; }
    
private:
    ProfileType type_;
    std::vector<std::shared_ptr<QoSPolicy>> policies_;
    void initDefaultProfile(ProfileType type);
};

class QoSManager {
public:
    static QoSManager& instance();
    
    void applyPolicies(const std::vector<std::shared_ptr<QoSPolicy>>& policies, void* data);
    std::vector<std::shared_ptr<QoSPolicy>> getOptimalPolicies(size_t dataSize, bool realTime);
    std::shared_ptr<QoSProfile> getProfile(QoSProfile::ProfileType type);
    void registerProfile(const std::string& name, const std::shared_ptr<QoSProfile>& profile);
    std::shared_ptr<QoSProfile> getProfile(const std::string& name);
    
private:
    QoSManager();
    std::map<QoSProfile::ProfileType, std::shared_ptr<QoSProfile>> defaultProfiles_;
    std::map<std::string, std::shared_ptr<QoSProfile>> customProfiles_;
};

} // namespace qos
} // namespace aurorart

#endif // AURORART_QOS_POLICY_H