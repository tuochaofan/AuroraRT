#ifndef AURORART_COMMUNICATION_PATTERN_H
#define AURORART_COMMUNICATION_PATTERN_H

#include <string>
#include <memory>
#include <functional>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <map>
#include "aurorart/communication/qos.h"

namespace aurorart {
namespace communication {

enum class PatternType {
    PUB_SUB,
    REQ_RESP,
    EVENT,
    PUSH_PULL
};

class CommunicationPattern {
public:
    virtual ~CommunicationPattern() = default;
    
    virtual void init() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

// 发布-订阅模式相关类

class PublisherBase {
public:
    virtual ~PublisherBase() = default;
    virtual void publish(const void* data, size_t size) = 0;
};

template<typename T>
class Publisher : public PublisherBase {
public:
    void publish(const T& data) {
        publish(&data, sizeof(T));
    }
};

// 具体发布者实现
template<typename T>
class ConcretePublisher : public Publisher<T> {
public:
    ConcretePublisher(const std::string& topic, PubSubPattern& pubSubPattern);
    void publish(const void* data, size_t size) override;
    
private:
    std::string topic_;
    PubSubPattern& pubSubPattern_;
    std::shared_ptr<transport::Transport> transport_;
    std::atomic<bool> running_;
};

class SubscriberBase {
public:
    virtual ~SubscriberBase() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    
protected:
    std::atomic<bool> running_;
};

template<typename T>
class Subscriber : public SubscriberBase {
public:
    Subscriber(std::function<void(const T&)> callback, const QoSPolicy& qos = QoSPolicy()) 
        : callback_(callback), qos_(qos) {}
    
    void start() override {
        running_ = true;
        thread_ = std::thread(&Subscriber::process, this);
    }
    
    void stop() override {
        running_ = false;
        cv_.notify_one();
        if (thread_.joinable()) {
            thread_.join();
        }
    }
    
    bool enqueue(const T& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        // 根据QoS策略检查队列大小
        if (queue_.size() >= max_queue_size_) {
            return false;
        }
        queue_.push(data);
        cv_.notify_one();
        return true;
    }
    
    QoSPolicy getQoS() const {
        return qos_;
    }
    
private:
    void process() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return !queue_.empty() || !running_; });
            
            if (!running_ && queue_.empty()) {
                break;
            }
            
            if (!queue_.empty()) {
                T data = queue_.front();
                queue_.pop();
                lock.unlock();
                callback_(data);
            }
        }
    }
    
    std::function<void(const T&)> callback_;
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread thread_;
    std::atomic<bool> running_;
    QoSPolicy qos_;
    static constexpr size_t max_queue_size_ = 1000; // 最大队列大小
};

class PubSubPattern : public CommunicationPattern {
public:
    PubSubPattern();
    void init() override;
    void start() override;
    void stop() override;
    
    template<typename T>
    std::shared_ptr<Publisher<T>> createPublisher(const std::string& topic);
    
    template<typename T>
    std::shared_ptr<SubscriberBase> createSubscriber(const std::string& topic, 
                                                  std::function<void(const T&)> callback, 
                                                  const QoSPolicy& qos = QoSPolicy());
    
    template<typename T>
    void publishToTopic(const std::string& topic, const T& data);
    
private:
    std::map<std::string, std::vector<std::shared_ptr<SubscriberBase>>> subscribers_;
    std::mutex subscribers_mutex_;
    std::atomic<bool> running_;
};

// 请求-响应模式相关类

class ClientBase {
public:
    virtual ~ClientBase() = default;
    virtual void sendRequest(const void* request, size_t requestSize, void* response, size_t responseSize) = 0;
};

class ServerBase {
public:
    virtual ~ServerBase() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

template<typename Req, typename Resp>
class Client : public ClientBase {
public:
    Client(const std::string& service, PubSubPattern& pubSubPattern);
    void sendRequest(const void* request, size_t requestSize, void* response, size_t responseSize) override;
    
private:
    // 带请求ID的请求结构
    template<typename T>
    struct RequestWithId {
        uint64_t request_id;
        T request;
    };
    
    // 带请求ID的响应结构
    template<typename T>
    struct ResponseWithId {
        uint64_t request_id;
        T response;
    };
    
    std::string service_;
    PubSubPattern& pubSubPattern_;
    std::atomic<uint64_t> next_request_id_;
    std::map<uint64_t, std::function<void(const Resp&)>> pending_requests_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::shared_ptr<SubscriberBase> response_subscriber_;
};

template<typename Req, typename Resp>
class Server : public ServerBase {
public:
    Server(const std::string& service, std::function<Resp(const Req&)> handler, PubSubPattern& pubSubPattern);
    void start() override;
    void stop() override;
    
private:
    void processRequests();
    
    std::string service_;
    std::function<Resp(const Req&)> handler_;
    PubSubPattern& pubSubPattern_;
    
    // 带请求ID的请求队列
    template<typename T>
    struct RequestWithId {
        uint64_t request_id;
        T request;
    };
    
    template<typename T>
    struct ResponseWithId {
        uint64_t request_id;
        T response;
    };
    
    std::queue<RequestWithId<Req>> request_queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread thread_;
    std::atomic<bool> running_;
};

class ReqRespPattern : public CommunicationPattern {
public:
    ReqRespPattern();
    void init() override;
    void start() override;
    void stop() override;
    
    template<typename Req, typename Resp>
    std::shared_ptr<ClientBase> createClient(const std::string& service);
    
    template<typename Req, typename Resp>
    std::shared_ptr<ServerBase> createServer(const std::string& service, 
                                           std::function<Resp(const Req&)> handler);
    
private:
    std::shared_ptr<PubSubPattern> pubSubPattern_;
    std::atomic<bool> running_;
};

// 事件模式相关类

class Notifier {
public:
    virtual ~Notifier() = default;
    virtual void notify() = 0;
};

class Listener {
public:
    virtual ~Listener() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class EventNotifier : public Notifier {
public:
    EventNotifier(const std::string& event, PubSubPattern& pubSubPattern);
    void notify() override;
    
private:
    std::string event_;
    PubSubPattern& pubSubPattern_;
};

class EventListener : public Listener {
public:
    EventListener(const std::string& event, std::function<void()> callback, PubSubPattern& pubSubPattern);
    void start() override;
    void stop() override;
    
private:
    std::string event_;
    std::function<void()> callback_;
    PubSubPattern& pubSubPattern_;
    std::shared_ptr<SubscriberBase> subscriber_;
};

class EventPattern : public CommunicationPattern {
public:
    EventPattern();
    void init() override;
    void start() override;
    void stop() override;
    
    std::shared_ptr<Notifier> createNotifier(const std::string& event);
    std::shared_ptr<Listener> createListener(const std::string& event, 
                                           std::function<void()> callback);
    
private:
    std::shared_ptr<PubSubPattern> pubSubPattern_;
    std::atomic<bool> running_;
};

// 推-拉模式相关类

class Pusher {
public:
    virtual ~Pusher() = default;
    template<typename T>
    void push(const T& data);
    virtual void push(const void* data, size_t size) = 0;
};

class Puller {
public:
    virtual ~Puller() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class PushNotifier : public Pusher {
public:
    PushNotifier(const std::string& channel, PubSubPattern& pubSubPattern);
    void push(const void* data, size_t size) override;
    
private:
    std::string channel_;
    PubSubPattern& pubSubPattern_;
};

class PullListener : public Puller {
public:
    template<typename T>
    PullListener(const std::string& channel, std::function<void(const T&)> callback, PubSubPattern& pubSubPattern);
    void start() override;
    void stop() override;
    
private:
    std::string channel_;
    PubSubPattern& pubSubPattern_;
    std::shared_ptr<SubscriberBase> subscriber_;
};

class PushPullPattern : public CommunicationPattern {
public:
    PushPullPattern();
    void init() override;
    void start() override;
    void stop() override;
    
    std::shared_ptr<Pusher> createPusher(const std::string& channel);
    template<typename T>
    std::shared_ptr<Puller> createPuller(const std::string& channel, 
                                        std::function<void(const T&)> callback);
    
private:
    std::shared_ptr<PubSubPattern> pubSubPattern_;
    std::atomic<bool> running_;
};

// 通信模式工厂

class CommunicationPatternFactory {
public:
    static std::shared_ptr<CommunicationPattern> createPattern(PatternType type);
};

} // namespace communication
} // namespace aurorart

#endif // AURORART_COMMUNICATION_PATTERN_H