#ifndef AURORART_TRANSPORT_H
#define AURORART_TRANSPORT_H

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <atomic>
#include <unordered_map>

namespace aurorart {
namespace transport {

enum class TransportType {
    INTRA_PROCESS,
    SHARED_MEMORY,
    NETWORK,
    TSN
};

enum class TransportStatus {
    UNINITIALIZED,
    INITIALIZED,
    RUNNING,
    STOPPED,
    ERROR
};

class Transport {
public:
    virtual ~Transport() = default;
    
    virtual void init() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool send(const void* data, size_t size) = 0;
    virtual bool receive(void* data, size_t size) = 0;
    virtual bool isAvailable() const = 0;
    virtual TransportType getType() const = 0;
    virtual TransportStatus getStatus() const = 0;
    
protected:
    TransportStatus status_ = TransportStatus::UNINITIALIZED;
};

// 装饰器模式：传输层装饰器
class TransportDecorator : public Transport {
public:
    TransportDecorator(std::shared_ptr<Transport> transport)
        : transport_(transport) {}
    
    void init() override {
        transport_->init();
    }
    
    void start() override {
        transport_->start();
    }
    
    void stop() override {
        transport_->stop();
    }
    
    bool send(const void* data, size_t size) override {
        return transport_->send(data, size);
    }
    
    bool receive(void* data, size_t size) override {
        return transport_->receive(data, size);
    }
    
    bool isAvailable() const override {
        return transport_->isAvailable();
    }
    
    TransportType getType() const override {
        return transport_->getType();
    }
    
    TransportStatus getStatus() const override {
        return transport_->getStatus();
    }
    
protected:
    std::shared_ptr<Transport> transport_;
};

// 具体装饰器：日志装饰器
class LoggingTransportDecorator : public TransportDecorator {
public:
    LoggingTransportDecorator(std::shared_ptr<Transport> transport)
        : TransportDecorator(transport) {}
    
    bool send(const void* data, size_t size) override;
    bool receive(void* data, size_t size) override;
};

// 具体装饰器：压缩装饰器
class CompressionTransportDecorator : public TransportDecorator {
public:
    CompressionTransportDecorator(std::shared_ptr<Transport> transport)
        : TransportDecorator(transport) {}
    
    bool send(const void* data, size_t size) override;
    bool receive(void* data, size_t size) override;
};

// 具体装饰器：加密装饰器
class EncryptionTransportDecorator : public TransportDecorator {
public:
    EncryptionTransportDecorator(std::shared_ptr<Transport> transport)
        : TransportDecorator(transport) {}
    
    bool send(const void* data, size_t size) override;
    bool receive(void* data, size_t size) override;
};

// 前向声明
template <typename T>
class LockFreeQueue;

class IntraProcessTransport : public Transport {
public:
    void init() override;
    void start() override;
    void stop() override;
    bool send(const void* data, size_t size) override;
    bool receive(void* data, size_t size) override;
    bool isAvailable() const override { return true; }
    TransportType getType() const override { return TransportType::INTRA_PROCESS; }
    TransportStatus getStatus() const override;
    
    // 统计信息
    void printStats() const;
    
private:
    std::unique_ptr<LockFreeQueue<std::vector<char>>> queue_;
    
    // 统计信息
    std::atomic<uint64_t> total_bytes_sent_ = 0;
    std::atomic<uint64_t> total_bytes_received_ = 0;
    std::atomic<uint64_t> total_transactions_ = 0;
    std::atomic<uint64_t> failure_count_ = 0;
};

typedef struct ShmBlock ShmBlock;

class SharedMemoryTransport : public Transport {
public:
    SharedMemoryTransport(const std::string& name, size_t size);
    void init() override;
    void start() override;
    void stop() override;
    bool send(const void* data, size_t size) override;
    bool receive(void* data, size_t size) override;
    // 零拷贝接口
    const void* getReadBuffer() const;
    void* getWriteBuffer(size_t size);
    bool commitWrite(size_t size);
    bool commitRead();
    // 高级零拷贝接口（支持动态大小消息）
    template <typename T>
    T* getTypedWriteBuffer();
    template <typename T>
    const T* getTypedReadBuffer() const;
    bool isAvailable() const override { return true; }
    TransportType getType() const override { return TransportType::SHARED_MEMORY; }
    TransportStatus getStatus() const override;
    
    // 统计信息
    void printStats() const;
    
private:
    void initializeMemoryPool();
    ShmBlock* allocateBlock(size_t size);
    void freeBlock(ShmBlock* block);
    
    std::string name_;
    size_t size_;
    void* shm_;
    char* buffer_;
    std::unique_ptr<LockFreeQueue<ShmBlock*>> queue_;
    std::unique_ptr<MultiLevelMemoryPool> memory_pool_;
    ShmBlock* current_read_block_;
    ShmBlock* current_write_block_;
    std::atomic<bool> dataAvailable_;
    
    // 统计信息
    std::atomic<uint64_t> total_bytes_sent_;
    std::atomic<uint64_t> total_bytes_received_;
    std::atomic<uint64_t> total_transactions_;
    std::atomic<uint64_t> failure_count_;
    std::atomic<size_t> peak_memory_usage_;
    std::atomic<size_t> current_memory_usage_;
};

class NetworkTransport : public Transport {
public:
    NetworkTransport(const std::string& host, int port);
    void init() override;
    void start() override;
    void stop() override;
    bool send(const void* data, size_t size) override;
    bool receive(void* data, size_t size) override;
    bool isAvailable() const override;
    TransportType getType() const override { return TransportType::NETWORK; }
    TransportStatus getStatus() const override;
    
    // 统计信息
    void printStats() const;
    
    // 重连机制
    bool reconnect();
    void setMaxReconnectAttempts(int attempts) { max_reconnect_attempts_ = attempts; }
    void setReconnectDelay(int delay_ms) { reconnect_delay_ = delay_ms; }
    
private:
    std::string host_;
    int port_;
    int sockfd_;
    bool available_;
    int send_timeout_;        // 发送超时时间（毫秒）
    int receive_timeout_;     // 接收超时时间（毫秒）
    size_t recv_buffer_size_; // 接收缓冲区大小
    size_t send_buffer_size_; // 发送缓冲区大小
    sockaddr_in remote_addr_; // 远程地址
    
    // 统计信息
    std::atomic<uint64_t> total_bytes_sent_;
    std::atomic<uint64_t> total_bytes_received_;
    std::atomic<uint64_t> total_transactions_;
    std::atomic<uint64_t> failure_count_;
    
    // 重连机制
    std::atomic<int> reconnect_attempts_;
    int max_reconnect_attempts_;
    int reconnect_delay_;
    uint64_t last_activity_time_;
};

class TSNTransport : public Transport {
public:
    TSNTransport(const std::string& interface, int port, int priority = 7);
    void init() override;
    void start() override;
    void stop() override;
    bool send(const void* data, size_t size) override;
    bool receive(void* data, size_t size) override;
    bool isAvailable() const override;
    TransportType getType() const override { return TransportType::TSN; }
    TransportStatus getStatus() const override;
    
    // TSN specific methods
    bool setTrafficClass(int priority);
    bool setTimeSyncEnabled(bool enabled);
    bool setScheduleEnabled(bool enabled);
    bool setQueueSize(size_t tx_size, size_t rx_size);
    uint64_t getCurrentTime() const;
    
private:
    // 前向声明
    class TimeSyncManager;
    class TrafficScheduler;
    
    std::string interface_;
    int port_;
    int priority_;
    int sockfd_;
    bool available_;
    bool timeSyncEnabled_;
    bool scheduleEnabled_;
    size_t tx_queue_size_;
    size_t rx_queue_size_;
    std::unique_ptr<TimeSyncManager> time_sync_manager_;
    std::unique_ptr<TrafficScheduler> traffic_scheduler_;
};

class TransportManager {
public:
    enum class NetworkStatus {
        POOR,
        FAIR,
        GOOD
    };
    
    static TransportManager& instance();
    
    void init();
    void start();
    void stop();
    
    std::shared_ptr<Transport> getTransport(TransportType type);
    std::shared_ptr<Transport> selectOptimalTransport(size_t dataSize, bool realTime, const std::string& destination = "");
    
    // 获取装饰后的传输
    std::shared_ptr<Transport> getDecoratedTransport(TransportType type, bool enableLogging = false, bool enableCompression = false, bool enableEncryption = false);
    
    void registerTransport(TransportType type, std::shared_ptr<Transport> transport);
    void unregisterTransport(TransportType type);
    
private:
    // 前向声明
    struct TransportPerformance;
    
    TransportManager();
    NetworkStatus evaluateNetworkStatus();
    bool isLocalDestination(const std::string& destination);
    std::shared_ptr<Transport> selectBasedOnPerformance(size_t dataSize, bool realTime);
    
    std::map<TransportType, std::shared_ptr<Transport>> transports_;
    std::map<std::string, std::shared_ptr<Transport>> decoratedTransports_;
    std::map<TransportType, std::unique_ptr<TransportPerformance>> performance_stats_;
    
public:
    // 性能统计相关方法
    void updateTransportStats(TransportType type, size_t bytes, uint64_t latency, bool success);
    void printPerformanceStats();
};

} // namespace transport
} // namespace aurorart

#endif // AURORART_TRANSPORT_H