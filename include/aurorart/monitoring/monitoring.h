#ifndef AURORART_MONITORING_H
#define AURORART_MONITORING_H

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>

namespace aurorart {
namespace monitoring {

enum class MetricType {
    CPU_USAGE,
    MEMORY_USAGE,
    NETWORK_TX,
    NETWORK_RX,
    DISK_USAGE,
    DISK_IO_READ,
    DISK_IO_WRITE,
    TEMPERATURE,
    PROCESS_COUNT,
    THREAD_COUNT,
    MESSAGE_RATE,
    LATENCY
};

enum class HealthStatus {
    HEALTHY,
    DEGRADED,
    CRITICAL,
    UNKNOWN
};

class Metric {
public:
    Metric(MetricType type, double value, std::chrono::steady_clock::time_point timestamp)
        : type_(type), value_(value), timestamp_(timestamp) {}
    
    MetricType getType() const { return type_; }
    double getValue() const { return value_; }
    std::chrono::steady_clock::time_point getTimestamp() const { return timestamp_; }
    
private:
    MetricType type_;
    double value_;
    std::chrono::steady_clock::time_point timestamp_;
};

class MetricCollector {
public:
    virtual ~MetricCollector() = default;
    
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual std::vector<Metric> collectMetrics() = 0;
    virtual std::string getName() const = 0;
};

class CpuMetricCollector : public MetricCollector {
public:
    void start() override;
    void stop() override;
    std::vector<Metric> collectMetrics() override;
    std::string getName() const override { return "CPU"; }
};

class MemoryMetricCollector : public MetricCollector {
public:
    void start() override;
    void stop() override;
    std::vector<Metric> collectMetrics() override;
    std::string getName() const override { return "Memory"; }
};

class NetworkMetricCollector : public MetricCollector {
public:
    void start() override;
    void stop() override;
    std::vector<Metric> collectMetrics() override;
    std::string getName() const override { return "Network"; }
};

class DiskMetricCollector : public MetricCollector {
public:
    void start() override;
    void stop() override;
    std::vector<Metric> collectMetrics() override;
    std::string getName() const override { return "Disk"; }
};

class TemperatureMetricCollector : public MetricCollector {
public:
    void start() override;
    void stop() override;
    std::vector<Metric> collectMetrics() override;
    std::string getName() const override { return "Temperature"; }
};

class ProcessCountMetricCollector : public MetricCollector {
public:
    void start() override;
    void stop() override;
    std::vector<Metric> collectMetrics() override;
    std::string getName() const override { return "ProcessCount"; }
};

class ThreadCountMetricCollector : public MetricCollector {
public:
    void start() override;
    void stop() override;
    std::vector<Metric> collectMetrics() override;
    std::string getName() const override { return "ThreadCount"; }
};

class MessageRateMetricCollector : public MetricCollector {
public:
    void start() override;
    void stop() override;
    std::vector<Metric> collectMetrics() override;
    std::string getName() const override { return "MessageRate"; }
};

class LatencyMetricCollector : public MetricCollector {
public:
    void start() override;
    void stop() override;
    std::vector<Metric> collectMetrics() override;
    std::string getName() const override { return "Latency"; }
};

class Monitor {
public:
    virtual ~Monitor() = default;
    
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual std::vector<Metric> collectMetrics() = 0;
    virtual HealthStatus getHealthStatus() = 0;
    virtual std::string getName() const = 0;
};

class DefaultMonitor : public Monitor {
public:
    DefaultMonitor(const std::string& name);
    
    void start() override;
    void stop() override;
    std::vector<Metric> collectMetrics() override;
    HealthStatus getHealthStatus() override;
    std::string getName() const override { return name_; }
    
    void addMetricCollector(std::shared_ptr<MetricCollector> collector);
    
private:
    std::string name_;
    std::vector<std::shared_ptr<MetricCollector>> collectors_;
    std::mutex mutex_;
    bool running_;
};

class HealthMonitor {
public:
    virtual ~HealthMonitor() = default;
    
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual HealthStatus checkHealth() = 0;
    virtual std::map<std::string, HealthStatus> getComponentHealth() = 0;
};

class DefaultHealthMonitor : public HealthMonitor {
public:
    void start() override;
    void stop() override;
    HealthStatus checkHealth() override;
    std::map<std::string, HealthStatus> getComponentHealth() override;
};

class MonitorManager {
public:
    static MonitorManager& instance();
    
    void init();
    void start();
    void stop();
    
    std::shared_ptr<Monitor> createMonitor(const std::string& name);
    std::shared_ptr<Monitor> getMonitor(const std::string& name) const;
    bool removeMonitor(const std::string& name);
    
    void addMetricCollector(std::shared_ptr<MetricCollector> collector);
    std::vector<Metric> collectMetrics();
    HealthStatus getHealthStatus();
    std::map<std::string, HealthStatus> getComponentHealth();
    
private:
    MonitorManager();
    
    std::map<std::string, std::shared_ptr<Monitor>> monitors_;
    std::vector<std::shared_ptr<MetricCollector>> collectors_;
    std::unique_ptr<HealthMonitor> healthMonitor_;
    std::mutex mutex_;
    bool running_;
};

} // namespace monitoring
} // namespace aurorart

#endif // AURORART_MONITORING_H
