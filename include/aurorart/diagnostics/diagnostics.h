#ifndef AURORART_DIAGNOSTICS_H
#define AURORART_DIAGNOSTICS_H

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>

namespace aurorart {
namespace diagnostics {

enum class FaultType {
    NODE_FAILURE,
    COMMUNICATION_FAILURE,
    SECURITY_FAILURE,
    MEMORY_FAILURE,
    TRANSPORT_FAILURE,
    SCHEDULER_FAILURE,
    UNKNOWN
};

enum class FaultSeverity {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

enum class RecoveryAction {
    RESTART_COMPONENT,
    RESTART_NODE,
    SWITCH_TO_BACKUP,
    ALERT_OPERATOR,
    IGNORE
};

enum class DiagnosticStatus {
    OK,
    WARNING,
    ERROR,
    CRITICAL
};

class Fault {
public:
    Fault(FaultType type, FaultSeverity severity, const std::string& message)
        : type_(type), severity_(severity), message_(message) {}
    
    FaultType getType() const { return type_; }
    FaultSeverity getSeverity() const { return severity_; }
    std::string getMessage() const { return message_; }
    
private:
    FaultType type_;
    FaultSeverity severity_;
    std::string message_;
};

class FaultDetector {
public:
    virtual ~FaultDetector() = default;
    
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual std::vector<Fault> detectFaults() = 0;
    virtual std::string getName() const = 0;
};

class NodeFaultDetector : public FaultDetector {
public:
    void start() override;
    void stop() override;
    std::vector<Fault> detectFaults() override;
    std::string getName() const override { return "NodeFaultDetector"; }
};

class CommunicationFaultDetector : public FaultDetector {
public:
    void start() override;
    void stop() override;
    std::vector<Fault> detectFaults() override;
    std::string getName() const override { return "CommunicationFaultDetector"; }
};

class SecurityFaultDetector : public FaultDetector {
public:
    void start() override;
    void stop() override;
    std::vector<Fault> detectFaults() override;
    std::string getName() const override { return "SecurityFaultDetector"; }
};

class DiagnosticAnalyzer {
public:
    virtual ~DiagnosticAnalyzer() = default;
    
    virtual void analyze(const std::vector<Fault>& faults) = 0;
    virtual DiagnosticStatus getStatus() const = 0;
    virtual std::string getAnalysis() const = 0;
};

class DefaultDiagnosticAnalyzer : public DiagnosticAnalyzer {
public:
    void analyze(const std::vector<Fault>& faults) override;
    DiagnosticStatus getStatus() const override;
    std::string getAnalysis() const override;
    
private:
    DiagnosticStatus status_ = DiagnosticStatus::OK;
    std::string analysis_;
};

class RecoveryManager {
public:
    virtual ~RecoveryManager() = default;
    
    virtual bool recoverFromFault(const Fault& fault) = 0;
    virtual bool recoverFromMultipleFaults(const std::vector<Fault>& faults) = 0;
    virtual std::vector<RecoveryAction> getRecommendedActions(const Fault& fault) = 0;
};

class DefaultRecoveryManager : public RecoveryManager {
public:
    bool recoverFromFault(const Fault& fault) override;
    bool recoverFromMultipleFaults(const std::vector<Fault>& faults) override;
    std::vector<RecoveryAction> getRecommendedActions(const Fault& fault) override;
};

class DiagnosticsManager {
public:
    static DiagnosticsManager& instance();
    
    void init();
    void start();
    void stop();
    
    void addFaultDetector(std::shared_ptr<FaultDetector> detector);
    void addDiagnosticAnalyzer(std::shared_ptr<DiagnosticAnalyzer> analyzer);
    
    std::vector<Fault> detectFaults();
    void analyzeFaults();
    bool recoverFromFault(const Fault& fault);
    
    DiagnosticStatus getDiagnosticStatus() const;
    std::string getDiagnosticAnalysis() const;
    std::vector<Fault> getDetectedFaults() const;
    
private:
    DiagnosticsManager();
    
    std::vector<std::shared_ptr<FaultDetector>> detectors_;
    std::vector<std::shared_ptr<DiagnosticAnalyzer>> analyzers_;
    std::unique_ptr<RecoveryManager> recoveryManager_;
    
    std::vector<Fault> detectedFaults_;
    DiagnosticStatus status_ = DiagnosticStatus::OK;
    std::string analysis_;
    
    std::mutex mutex_;
    bool running_ = false;
};

} // namespace diagnostics
} // namespace aurorart

#endif // AURORART_DIAGNOSTICS_H
