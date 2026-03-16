#include "aurorart/diagnostics/diagnostics.h"
#include "aurorart/utils/logger.h"
#include "aurorart/utils/config.h"
#include <random>

namespace aurorart {
namespace diagnostics {

// NodeFaultDetector implementation

void NodeFaultDetector::start() {
    AURORA_LOG_INFO("Node fault detector started");
}

void NodeFaultDetector::stop() {
    AURORA_LOG_INFO("Node fault detector stopped");
}

std::vector<Fault> NodeFaultDetector::detectFaults() {
    std::vector<Fault> faults;
    
    // 模拟节点故障检测
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 99);
    
    if (dis(gen) < 5) { // 5%概率检测到故障
        faults.emplace_back(FaultType::NODE_FAILURE, FaultSeverity::MEDIUM, "Node health check failed");
        AURORA_LOG_WARN("Node fault detected");
    }
    
    return faults;
}

// CommunicationFaultDetector implementation

void CommunicationFaultDetector::start() {
    AURORA_LOG_INFO("Communication fault detector started");
}

void CommunicationFaultDetector::stop() {
    AURORA_LOG_INFO("Communication fault detector stopped");
}

std::vector<Fault> CommunicationFaultDetector::detectFaults() {
    std::vector<Fault> faults;
    
    // 模拟通信故障检测
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 99);
    
    if (dis(gen) < 3) { // 3%概率检测到故障
        faults.emplace_back(FaultType::COMMUNICATION_FAILURE, FaultSeverity::HIGH, "Communication timeout detected");
        AURORA_LOG_WARN("Communication fault detected");
    }
    
    return faults;
}

// SecurityFaultDetector implementation

void SecurityFaultDetector::start() {
    AURORA_LOG_INFO("Security fault detector started");
}

void SecurityFaultDetector::stop() {
    AURORA_LOG_INFO("Security fault detector stopped");
}

std::vector<Fault> SecurityFaultDetector::detectFaults() {
    std::vector<Fault> faults;
    
    // 模拟安全故障检测
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 99);
    
    if (dis(gen) < 1) { // 1%概率检测到故障
        faults.emplace_back(FaultType::SECURITY_FAILURE, FaultSeverity::CRITICAL, "Unauthorized access attempt detected");
        AURORA_LOG_WARN("Security fault detected");
    }
    
    return faults;
}

// DefaultDiagnosticAnalyzer implementation

void DefaultDiagnosticAnalyzer::analyze(const std::vector<Fault>& faults) {
    if (faults.empty()) {
        status_ = DiagnosticStatus::OK;
        analysis_ = "No faults detected";
        return;
    }
    
    int criticalCount = 0;
    int highCount = 0;
    int mediumCount = 0;
    
    for (const auto& fault : faults) {
        if (fault.getSeverity() == FaultSeverity::CRITICAL) {
            criticalCount++;
        } else if (fault.getSeverity() == FaultSeverity::HIGH) {
            highCount++;
        } else if (fault.getSeverity() == FaultSeverity::MEDIUM) {
            mediumCount++;
        }
    }
    
    if (criticalCount > 0) {
        status_ = DiagnosticStatus::CRITICAL;
        analysis_ = "Critical faults detected: " + std::to_string(criticalCount);
    } else if (highCount > 0) {
        status_ = DiagnosticStatus::ERROR;
        analysis_ = "High severity faults detected: " + std::to_string(highCount);
    } else if (mediumCount > 0) {
        status_ = DiagnosticStatus::WARNING;
        analysis_ = "Medium severity faults detected: " + std::to_string(mediumCount);
    } else {
        status_ = DiagnosticStatus::OK;
        analysis_ = "Only low severity faults detected";
    }
    
    AURORA_LOG_INFO("Diagnostic analysis: {}", analysis_);
}

DiagnosticStatus DefaultDiagnosticAnalyzer::getStatus() const {
    return status_;
}

std::string DefaultDiagnosticAnalyzer::getAnalysis() const {
    return analysis_;
}

// DefaultRecoveryManager implementation

bool DefaultRecoveryManager::recoverFromFault(const Fault& fault) {
    AURORA_LOG_INFO("Attempting to recover from fault: {}", fault.getMessage());
    
    // 根据故障类型执行恢复操作
    switch (fault.getType()) {
        case FaultType::NODE_FAILURE:
            AURORA_LOG_INFO("Recovery action: Restarting node");
            break;
        case FaultType::COMMUNICATION_FAILURE:
            AURORA_LOG_INFO("Recovery action: Resetting communication channel");
            break;
        case FaultType::SECURITY_FAILURE:
            AURORA_LOG_INFO("Recovery action: Initiating security protocol");
            break;
        default:
            AURORA_LOG_INFO("Recovery action: Generic recovery");
            break;
    }
    
    // 模拟恢复成功
    AURORA_LOG_INFO("Recovery successful");
    return true;
}

bool DefaultRecoveryManager::recoverFromMultipleFaults(const std::vector<Fault>& faults) {
    for (const auto& fault : faults) {
        if (!recoverFromFault(fault)) {
            AURORA_LOG_ERROR("Failed to recover from fault: {}", fault.getMessage());
            return false;
        }
    }
    
    AURORA_LOG_INFO("All faults recovered successfully");
    return true;
}

std::vector<RecoveryAction> DefaultRecoveryManager::getRecommendedActions(const Fault& fault) {
    std::vector<RecoveryAction> actions;
    
    switch (fault.getType()) {
        case FaultType::NODE_FAILURE:
            actions.push_back(RecoveryAction::RESTART_NODE);
            break;
        case FaultType::COMMUNICATION_FAILURE:
            actions.push_back(RecoveryAction::RESTART_COMPONENT);
            break;
        case FaultType::SECURITY_FAILURE:
            actions.push_back(RecoveryAction::ALERT_OPERATOR);
            break;
        default:
            actions.push_back(RecoveryAction::RESTART_COMPONENT);
            break;
    }
    
    return actions;
}

// DiagnosticsManager implementation

DiagnosticsManager::DiagnosticsManager() {}

DiagnosticsManager& DiagnosticsManager::instance() {
    static DiagnosticsManager instance;
    return instance;
}

void DiagnosticsManager::init() {
    recoveryManager_ = std::make_unique<DefaultRecoveryManager>();
    
    // 添加默认的故障检测器
    addFaultDetector(std::make_shared<NodeFaultDetector>());
    addFaultDetector(std::make_shared<CommunicationFaultDetector>());
    addFaultDetector(std::make_shared<SecurityFaultDetector>());
    
    // 添加默认的诊断分析器
    addDiagnosticAnalyzer(std::make_shared<DefaultDiagnosticAnalyzer>());
    
    AURORA_LOG_INFO("Diagnostics manager initialized");
}

void DiagnosticsManager::start() {
    if (running_) {
        AURORA_LOG_WARN("Diagnostics manager is already running");
        return;
    }
    
    for (auto& detector : detectors_) {
        detector->start();
    }
    
    running_ = true;
    AURORA_LOG_INFO("Diagnostics manager started");
}

void DiagnosticsManager::stop() {
    if (!running_) {
        AURORA_LOG_WARN("Diagnostics manager is not running");
        return;
    }
    
    for (auto& detector : detectors_) {
        detector->stop();
    }
    
    running_ = false;
    AURORA_LOG_INFO("Diagnostics manager stopped");
}

void DiagnosticsManager::addFaultDetector(std::shared_ptr<FaultDetector> detector) {
    std::lock_guard<std::mutex> lock(mutex_);
    detectors_.push_back(detector);
    AURORA_LOG_INFO("Added fault detector: {}", detector->getName());
}

void DiagnosticsManager::addDiagnosticAnalyzer(std::shared_ptr<DiagnosticAnalyzer> analyzer) {
    std::lock_guard<std::mutex> lock(mutex_);
    analyzers_.push_back(analyzer);
    AURORA_LOG_INFO("Added diagnostic analyzer");
}

std::vector<Fault> DiagnosticsManager::detectFaults() {
    std::vector<Fault> allFaults;
    
    for (auto& detector : detectors_) {
        auto faults = detector->detectFaults();
        allFaults.insert(allFaults.end(), faults.begin(), faults.end());
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    detectedFaults_ = allFaults;
    
    return allFaults;
}

void DiagnosticsManager::analyzeFaults() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& analyzer : analyzers_) {
        analyzer->analyze(detectedFaults_);
        status_ = analyzer->getStatus();
        analysis_ = analyzer->getAnalysis();
    }
}

bool DiagnosticsManager::recoverFromFault(const Fault& fault) {
    return recoveryManager_->recoverFromFault(fault);
}

DiagnosticStatus DiagnosticsManager::getDiagnosticStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

std::string DiagnosticsManager::getDiagnosticAnalysis() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return analysis_;
}

std::vector<Fault> DiagnosticsManager::getDetectedFaults() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return detectedFaults_;
}

} // namespace diagnostics
} // namespace aurorart
