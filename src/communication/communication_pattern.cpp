#include "aurorart/communication/communication_pattern.h"
#include "aurorart/transport/transport.h"
#include "aurorart/utils/logger.h"
#include "aurorart/utils/serializer.h"
#include "aurorart/communication/qos.h"
#include <map>
#include <vector>
#include <queue>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <unordered_map>
#include <algorithm>

namespace aurorart {
namespace communication {

// QoS Policy implementation

QoSPolicy::QoSPolicy() 
    : reliability_(Reliability::BEST_EFFORT),
      durability_(Durability::VOLATILE),
      deadline_(std::chrono::milliseconds(0)),
      lifespan_(std::chrono::milliseconds(0)),
      priority_(0) {
}

QoSPolicy::QoSPolicy(Reliability reliability, Durability durability, 
                     std::chrono::milliseconds deadline, 
                     std::chrono::milliseconds lifespan, 
                     int priority)
    : reliability_(reliability),
      durability_(durability),
      deadline_(deadline),
      lifespan_(lifespan),
      priority_(priority) {
}

// PubSubPattern implementation

PubSubPattern::PubSubPattern() : running_(false) {
}

void PubSubPattern::init() {
    AURORA_LOG_INFO("Initializing pub-sub pattern");
}

void PubSubPattern::start() {
    AURORA_LOG_INFO("Starting pub-sub pattern");
    running_ = true;
}

void PubSubPattern::stop() {
    AURORA_LOG_INFO("Stopping pub-sub pattern");
    running_ = false;
    
    // 停止所有订阅者
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    for (auto& [topic, subs] : subscribers_) {
        for (auto& sub : subs) {
            sub->stop();
        }
    }
    
    // 清空消息队列
    std::lock_guard<std::mutex> msg_lock(message_queues_mutex_);
    message_queues_.clear();
}

template<typename T>
class ConcretePublisher : public Publisher<T> {
public:
    ConcretePublisher(const std::string& topic, PubSubPattern& pubSubPattern, const QoSPolicy& qos = QoSPolicy()) 
        : topic_(topic), pubSubPattern_(pubSubPattern), qos_(qos) {
        transport_ = transport::TransportManager::instance().selectOptimalTransport(sizeof(T), true);
    }
    
    void publish(const void* data, size_t size) override {
        if (!running_) {
            AURORA_LOG_WARN("ConcretePublisher::publish: Publisher not running");
            return;
        }
        
        if (!data) {
            AURORA_LOG_ERROR("ConcretePublisher::publish: Invalid null data");
            return;
        }
        
        if (size == 0) {
            AURORA_LOG_ERROR("ConcretePublisher::publish: Invalid size 0");
            return;
        }
        
        try {
            // 序列化数据
            std::vector<uint8_t> serialized_data;
            if (!Serializer::serialize(data, size, serialized_data)) {
                AURORA_LOG_ERROR("ConcretePublisher::publish: Failed to serialize data");
                return;
            }
            
            // 根据QoS策略处理消息
            if (qos_.getReliability() == Reliability::RELIABLE) {
                // 可靠传输：添加到消息队列，确保消息送达
                pubSubPattern_.addMessageToQueue(topic_, serialized_data);
                AURORA_LOG_DEBUG("ConcretePublisher::publish: Added reliable message to queue for topic {}", topic_);
            }
            
            // 发送数据
            if (!transport_) {
                AURORA_LOG_ERROR("ConcretePublisher::publish: No transport available");
                return;
            }
            
            if (!transport_->send(serialized_data.data(), serialized_data.size())) {
                AURORA_LOG_WARN("ConcretePublisher::publish: Failed to send data via transport");
                // 如果是可靠传输，需要处理失败情况
                if (qos_.getReliability() == Reliability::RELIABLE) {
                    AURORA_LOG_INFO("ConcretePublisher::publish: Will retry via message queue for topic {}", topic_);
                }
            }
            
            // 同时发布到本地订阅者
            pubSubPattern_.publishToTopic<T>(topic_, *static_cast<const T*>(data));
            
            AURORA_LOG_DEBUG("ConcretePublisher::publish: Published message to topic {} (size: {} bytes)", topic_, size);
        } catch (const std::exception& e) {
            AURORA_LOG_ERROR("ConcretePublisher::publish error: {}", e.what());
        }
    }
    
    void setQoS(const QoSPolicy& qos) {
        qos_ = qos;
    }
    
    QoSPolicy getQoS() const {
        return qos_;
    }
    
private:
    std::string topic_;
    PubSubPattern& pubSubPattern_;
    std::shared_ptr<transport::Transport> transport_;
    std::atomic<bool> running_{true};
    QoSPolicy qos_;
};

template<typename T>
std::shared_ptr<Publisher<T>> PubSubPattern::createPublisher(const std::string& topic, const QoSPolicy& qos) {
    return std::make_shared<ConcretePublisher<T>>(topic, *this, qos);
}

template<typename T>
std::shared_ptr<Publisher<T>> PubSubPattern::createPublisher(const std::string& topic) {
    return createPublisher<T>(topic, QoSPolicy());
}

template<typename T>
std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber(const std::string& topic, 
                                                              std::function<void(const T&)> callback, 
                                                              const QoSPolicy& qos) {
    auto subscriber = std::make_shared<Subscriber<T>>(callback, qos);
    
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    subscribers_[topic].push_back(subscriber);
    
    subscriber->start();
    return subscriber;
}

template<typename T>
std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber(const std::string& topic, 
                                                              std::function<void(const T&)> callback) {
    return createSubscriber<T>(topic, callback, QoSPolicy());
}

template<typename T>
void PubSubPattern::publishToTopic(const std::string& topic, const T& data) {
    if (topic.empty()) {
        AURORA_LOG_ERROR("PubSubPattern::publishToTopic: Empty topic");
        return;
    }
    
    try {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        auto it = subscribers_.find(topic);
        if (it != subscribers_.end()) {
            // 按QoS优先级对订阅者进行排序
            std::vector<std::pair<int, Subscriber<T>*>> prioritized_subscribers;
            
            for (auto& subscriber : it->second) {
                auto typedSubscriber = dynamic_cast<Subscriber<T>*>(subscriber.get());
                if (typedSubscriber) {
                    int priority = typedSubscriber->getQoS().getPriority();
                    prioritized_subscribers.emplace_back(priority, typedSubscriber);
                }
            }
            
            // 按优先级降序排序
            std::sort(prioritized_subscribers.begin(), prioritized_subscribers.end(), 
                [](const auto& a, const auto& b) { return a.first > b.first; });
            
            size_t delivered = 0;
            size_t failed = 0;
            
            for (auto& [priority, subscriber] : prioritized_subscribers) {
                if (subscriber->enqueue(data)) {
                    delivered++;
                } else {
                    AURORA_LOG_WARN("PubSubPattern::publishToTopic: Failed to enqueue data for subscriber with priority {}", priority);
                    failed++;
                }
            }
            
            AURORA_LOG_DEBUG("PubSubPattern::publishToTopic: Delivered message to {} subscribers, failed: {} for topic {}", 
                           delivered, failed, topic);
        } else {
            AURORA_LOG_DEBUG("PubSubPattern::publishToTopic: No subscribers for topic {}", topic);
        }
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("PubSubPattern::publishToTopic error: {}", e.what());
    }
}

void PubSubPattern::addMessageToQueue(const std::string& topic, const std::vector<uint8_t>& message) {
    if (topic.empty()) {
        AURORA_LOG_ERROR("PubSubPattern::addMessageToQueue: Empty topic");
        return;
    }
    
    if (message.empty()) {
        AURORA_LOG_ERROR("PubSubPattern::addMessageToQueue: Empty message");
        return;
    }
    
    try {
        std::lock_guard<std::mutex> lock(message_queues_mutex_);
        message_queues_[topic].push(message);
        AURORA_LOG_DEBUG("PubSubPattern::addMessageToQueue: Added message to queue for topic {} (size: {} bytes)", topic, message.size());
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("PubSubPattern::addMessageToQueue error: {}", e.what());
    }
}

void PubSubPattern::processMessageQueue(const std::string& topic) {
    if (topic.empty()) {
        AURORA_LOG_ERROR("PubSubPattern::processMessageQueue: Empty topic");
        return;
    }
    
    try {
        std::lock_guard<std::mutex> lock(message_queues_mutex_);
        auto it = message_queues_.find(topic);
        if (it != message_queues_.end()) {
            size_t processed = 0;
            while (!it->second.empty()) {
                // 处理队列中的消息
                auto message = it->second.front();
                it->second.pop();
                
                // 这里可以添加消息重发逻辑
                // 例如：尝试通过传输层重新发送消息
                AURORA_LOG_DEBUG("Processing message from queue for topic: {}, size: {} bytes", topic, message.size());
                processed++;
            }
            if (processed > 0) {
                AURORA_LOG_INFO("PubSubPattern::processMessageQueue: Processed {} messages for topic {}", processed, topic);
            }
        } else {
            AURORA_LOG_DEBUG("PubSubPattern::processMessageQueue: No message queue for topic {}", topic);
        }
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("PubSubPattern::processMessageQueue error: {}", e.what());
    }
}

// ReqRespPattern implementation

ReqRespPattern::ReqRespPattern() : running_(false) {
}

void ReqRespPattern::init() {
    AURORA_LOG_INFO("Initializing req-resp pattern");
    pubSubPattern_ = std::make_shared<PubSubPattern>();
    pubSubPattern_->init();
}

void ReqRespPattern::start() {
    AURORA_LOG_INFO("Starting req-resp pattern");
    running_ = true;
    pubSubPattern_->start();
}

void ReqRespPattern::stop() {
    AURORA_LOG_INFO("Stopping req-resp pattern");
    running_ = false;
    pubSubPattern_->stop();
    
    // 清空请求响应映射
    std::lock_guard<std::mutex> lock(request_response_mutex_);
    request_response_map_.clear();
}

template<typename Req, typename Resp>
Client<Req, Resp>::Client(const std::string& service, PubSubPattern& pubSubPattern, const QoSPolicy& qos) 
    : service_(service), pubSubPattern_(pubSubPattern), qos_(qos), next_request_id_(1) {
    // 创建响应订阅者
    response_subscriber_ = pubSubPattern_.createSubscriber<Resp>(service + ".response", 
        [this](const Resp& response) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = pending_requests_.find(response.request_id);
            if (it != pending_requests_.end()) {
                *it->second.response = response;
                it->second.cv.notify_one();
            }
        }
    );
}

template<typename Req, typename Resp>
void Client<Req, Resp>::sendRequest(const void* request, size_t requestSize, void* response, size_t responseSize) {
    if (!running_) {
        return;
    }
    
    // 生成请求ID
    uint64_t request_id = next_request_id_++;
    
    // 准备请求
    Req req = *static_cast<const Req*>(request);
    req.request_id = request_id;
    
    // 序列化请求
    std::vector<uint8_t> serialized_request;
    Serializer::serialize(&req, sizeof(req), serialized_request);
    
    // 发送请求
    pubSubPattern_.publishToTopic<Req>(service_ + ".request", req);
    
    // 等待响应
    std::unique_lock<std::mutex> lock(mutex_);
    PendingRequest pending_req;
    pending_req.response = static_cast<Resp*>(response);
    pending_requests_[request_id] = pending_req;
    
    // 根据QoS策略设置超时
    auto deadline = qos_.getDeadline();
    if (deadline.count() > 0) {
        if (!pending_req.cv.wait_for(lock, deadline, [&]() {
            return pending_requests_.find(request_id) == pending_requests_.end();
        })) {
            // 超时处理
            pending_requests_.erase(request_id);
            AURORA_LOG_WARN("Request timed out for service: {}", service_);
            return;
        }
    } else {
        // 默认超时：10秒
        if (!pending_req.cv.wait_for(lock, std::chrono::seconds(10), [&]() {
            return pending_requests_.find(request_id) == pending_requests_.end();
        })) {
            // 超时处理
            pending_requests_.erase(request_id);
            AURORA_LOG_WARN("Request timed out for service: {}", service_);
            return;
        }
    }
    
    // 移除已处理的请求
    pending_requests_.erase(request_id);
}

template<typename Req, typename Resp>
Server<Req, Resp>::Server(const std::string& service, std::function<Resp(const Req&)> handler, PubSubPattern& pubSubPattern, const QoSPolicy& qos)
    : service_(service), handler_(handler), pubSubPattern_(pubSubPattern), qos_(qos), running_(false) {
    // 创建请求订阅者
    request_subscriber_ = pubSubPattern_.createSubscriber<Req>(service + ".request",
        [this](const Req& request) {
            std::lock_guard<std::mutex> lock(mutex_);
            // 根据QoS策略处理请求优先级
            if (qos_.getPriority() > 0) {
                // 高优先级请求插入到队列前面
                std::queue<Req> temp_queue;
                while (!request_queue_.empty() && request_queue_.front().priority < qos_.getPriority()) {
                    temp_queue.push(request_queue_.front());
                    request_queue_.pop();
                }
                request_queue_.push(request);
                while (!temp_queue.empty()) {
                    request_queue_.push(temp_queue.front());
                    temp_queue.pop();
                }
            } else {
                // 普通优先级请求插入到队列末尾
                request_queue_.push(request);
            }
            cv_.notify_one();
        },
        qos
    );
}


template<typename Req, typename Resp>
void Server<Req, Resp>::start() {
    running_ = true;
    thread_ = std::thread(&Server::processRequests, this);
}


template<typename Req, typename Resp>
void Server<Req, Resp>::stop() {
    running_ = false;
    cv_.notify_one();
    if (thread_.joinable()) {
        thread_.join();
    }
    
    // 清空请求队列
    std::lock_guard<std::mutex> lock(mutex_);
    while (!request_queue_.empty()) {
        request_queue_.pop();
    }
}


template<typename Req, typename Resp>
void Server<Req, Resp>::processRequests() {
    while (running_) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return !request_queue_.empty() || !running_; });
        
        if (!running_ && request_queue_.empty()) {
            break;
        }
        
        if (!request_queue_.empty()) {
            Req request = request_queue_.front();
            request_queue_.pop();
            lock.unlock();
            
            try {
                // 处理请求
                Resp response = handler_(request);
                
                // 设置响应ID
                response.request_id = request.request_id;
                
                // 序列化响应
                std::vector<uint8_t> serialized_response;
                Serializer::serialize(&response, sizeof(response), serialized_response);
                
                // 发送响应
                pubSubPattern_.publishToTopic<Resp>(service_ + ".response", response);
            } catch (const std::exception& e) {
                AURORA_LOG_ERROR("Error processing request: {}", e.what());
                // 发送错误响应
                Resp error_response;
                error_response.request_id = request.request_id;
                error_response.error_code = 1;
                pubSubPattern_.publishToTopic<Resp>(service_ + ".response", error_response);
            }
        }
    }
}

template<typename Req, typename Resp>
std::shared_ptr<ClientBase> ReqRespPattern::createClient(const std::string& service, const QoSPolicy& qos) {
    return std::make_shared<Client<Req, Resp>>(service, *pubSubPattern_, qos);
}

template<typename Req, typename Resp>
std::shared_ptr<ClientBase> ReqRespPattern::createClient(const std::string& service) {
    return createClient<Req, Resp>(service, QoSPolicy());
}


template<typename Req, typename Resp>
std::shared_ptr<ServerBase> ReqRespPattern::createServer(const std::string& service, 
                                                       std::function<Resp(const Req&)> handler, 
                                                       const QoSPolicy& qos) {
    return std::make_shared<Server<Req, Resp>>(service, handler, *pubSubPattern_, qos);
}

template<typename Req, typename Resp>
std::shared_ptr<ServerBase> ReqRespPattern::createServer(const std::string& service, 
                                                       std::function<Resp(const Req&)> handler) {
    return createServer<Req, Resp>(service, handler, QoSPolicy());
}

// EventPattern implementation

EventPattern::EventPattern() : running_(false) {
}

void EventPattern::init() {
    AURORA_LOG_INFO("Initializing event pattern");
    pubSubPattern_ = std::make_shared<PubSubPattern>();
    pubSubPattern_->init();
}

void EventPattern::start() {
    AURORA_LOG_INFO("Starting event pattern");
    running_ = true;
    pubSubPattern_->start();
}

void EventPattern::stop() {
    AURORA_LOG_INFO("Stopping event pattern");
    running_ = false;
    pubSubPattern_->stop();
}

EventNotifier::EventNotifier(const std::string& event, PubSubPattern& pubSubPattern, const QoSPolicy& qos)
    : event_(event), pubSubPattern_(pubSubPattern), qos_(qos) {
}

void EventNotifier::notify() {
    // 发布事件通知
    int dummy = 0;
    pubSubPattern_.publishToTopic<int>(event_, dummy);
}

EventListener::EventListener(const std::string& event, std::function<void()> callback, PubSubPattern& pubSubPattern, const QoSPolicy& qos)
    : event_(event), callback_(callback), pubSubPattern_(pubSubPattern), qos_(qos) {
    // 创建事件订阅者
    subscriber_ = pubSubPattern_.createSubscriber<int>(event, [this](const int&) {
        callback_();
    }, qos);
}

void EventListener::start() {
    if (subscriber_) {
        subscriber_->start();
    }
}

void EventListener::stop() {
    if (subscriber_) {
        subscriber_->stop();
    }
}

std::shared_ptr<Notifier> EventPattern::createNotifier(const std::string& event, const QoSPolicy& qos) {
    return std::make_shared<EventNotifier>(event, *pubSubPattern_, qos);
}

std::shared_ptr<Notifier> EventPattern::createNotifier(const std::string& event) {
    return createNotifier(event, QoSPolicy());
}

std::shared_ptr<Listener> EventPattern::createListener(const std::string& event, 
                                                     std::function<void()> callback, 
                                                     const QoSPolicy& qos) {
    return std::make_shared<EventListener>(event, callback, *pubSubPattern_, qos);
}

std::shared_ptr<Listener> EventPattern::createListener(const std::string& event, 
                                                     std::function<void()> callback) {
    return createListener(event, callback, QoSPolicy());
}

// CommunicationPatternFactory implementation

// PushPullPattern implementation

PushPullPattern::PushPullPattern() : running_(false) {
}

void PushPullPattern::init() {
    AURORA_LOG_INFO("Initializing push-pull pattern");
    pubSubPattern_ = std::make_shared<PubSubPattern>();
    pubSubPattern_->init();
}

void PushPullPattern::start() {
    AURORA_LOG_INFO("Starting push-pull pattern");
    running_ = true;
    pubSubPattern_->start();
}

void PushPullPattern::stop() {
    AURORA_LOG_INFO("Stopping push-pull pattern");
    running_ = false;
    pubSubPattern_->stop();
}

PushNotifier::PushNotifier(const std::string& channel, PubSubPattern& pubSubPattern, const QoSPolicy& qos)
    : channel_(channel), pubSubPattern_(pubSubPattern), qos_(qos) {
}

void PushNotifier::push(const void* data, size_t size) {
    // 序列化数据
    std::vector<uint8_t> serialized_data(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    
    // 根据QoS策略处理消息
    if (qos_.getReliability() == Reliability::RELIABLE) {
        // 可靠传输：添加到消息队列，确保消息送达
        pubSubPattern_.addMessageToQueue(channel_, serialized_data);
    }
    
    // 推送数据
    pubSubPattern_.publishToTopic<std::vector<uint8_t>>(channel_, serialized_data);
}

template<typename T>
PullListener::PullListener(const std::string& channel, std::function<void(const T&)> callback, PubSubPattern& pubSubPattern, const QoSPolicy& qos)
    : channel_(channel), pubSubPattern_(pubSubPattern), qos_(qos) {
    // 创建数据订阅者
    subscriber_ = pubSubPattern_.createSubscriber<T>(channel, callback, qos);
}

void PullListener::start() {
    if (subscriber_) {
        subscriber_->start();
    }
}

void PullListener::stop() {
    if (subscriber_) {
        subscriber_->stop();
    }
}

std::shared_ptr<Pusher> PushPullPattern::createPusher(const std::string& channel, const QoSPolicy& qos) {
    return std::make_shared<PushNotifier>(channel, *pubSubPattern_, qos);
}

std::shared_ptr<Pusher> PushPullPattern::createPusher(const std::string& channel) {
    return createPusher(channel, QoSPolicy());
}

template<typename T>
std::shared_ptr<Puller> PushPullPattern::createPuller(const std::string& channel, 
                                                     std::function<void(const T&)> callback, 
                                                     const QoSPolicy& qos) {
    return std::make_shared<PullListener>(channel, callback, *pubSubPattern_, qos);
}

template<typename T>
std::shared_ptr<Puller> PushPullPattern::createPuller(const std::string& channel, 
                                                     std::function<void(const T&)> callback) {
    return createPuller<T>(channel, callback, QoSPolicy());
}

// CommunicationPatternFactory implementation

std::shared_ptr<CommunicationPattern> CommunicationPatternFactory::createPattern(PatternType type) {
    switch (type) {
    case PatternType::PUB_SUB:
        return std::make_shared<PubSubPattern>();
    case PatternType::REQ_RESP:
        return std::make_shared<ReqRespPattern>();
    case PatternType::EVENT:
        return std::make_shared<EventPattern>();
    case PatternType::PUSH_PULL:
        return std::make_shared<PushPullPattern>();
    default:
        return nullptr;
    }
}

// 显式实例化常用类型
template class ConcretePublisher<int>;
template class ConcretePublisher<float>;
template class ConcretePublisher<double>;
template class Subscriber<int>;
template class Subscriber<float>;
template class Subscriber<double>;
template class Client<int, int>;
template class Client<float, float>;
template class Client<double, double>;
template class Server<int, int>;
template class Server<float, float>;
template class Server<double, double>;
template class PullListener<int>;
template class PullListener<float>;
template class PullListener<double>;
template std::shared_ptr<Publisher<int>> PubSubPattern::createPublisher<int>(const std::string&, const QoSPolicy&);
template std::shared_ptr<Publisher<float>> PubSubPattern::createPublisher<float>(const std::string&, const QoSPolicy&);
template std::shared_ptr<Publisher<double>> PubSubPattern::createPublisher<double>(const std::string&, const QoSPolicy&);
template std::shared_ptr<Publisher<int>> PubSubPattern::createPublisher<int>(const std::string&);
template std::shared_ptr<Publisher<float>> PubSubPattern::createPublisher<float>(const std::string&);
template std::shared_ptr<Publisher<double>> PubSubPattern::createPublisher<double>(const std::string&);
template std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber<int>(const std::string&, std::function<void(const int&)>, const QoSPolicy&);
template std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber<float>(const std::string&, std::function<void(const float&)>, const QoSPolicy&);
template std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber<double>(const std::string&, std::function<void(const double&)>, const QoSPolicy&);
template std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber<int>(const std::string&, std::function<void(const int&)>);
template std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber<float>(const std::string&, std::function<void(const float&)>);
template std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber<double>(const std::string&, std::function<void(const double&)>);
template std::shared_ptr<ClientBase> ReqRespPattern::createClient<int, int>(const std::string&, const QoSPolicy&);
template std::shared_ptr<ClientBase> ReqRespPattern::createClient<float, float>(const std::string&, const QoSPolicy&);
template std::shared_ptr<ClientBase> ReqRespPattern::createClient<int, int>(const std::string&);
template std::shared_ptr<ClientBase> ReqRespPattern::createClient<float, float>(const std::string&);
template std::shared_ptr<ServerBase> ReqRespPattern::createServer<int, int>(const std::string&, std::function<int(const int&)>, const QoSPolicy&);
template std::shared_ptr<ServerBase> ReqRespPattern::createServer<float, float>(const std::string&, std::function<float(const float&)>, const QoSPolicy&);
template std::shared_ptr<ServerBase> ReqRespPattern::createServer<int, int>(const std::string&, std::function<int(const int&)>);
template std::shared_ptr<ServerBase> ReqRespPattern::createServer<float, float>(const std::string&, std::function<float(const float&)>);
template std::shared_ptr<Puller> PushPullPattern::createPuller<int>(const std::string&, std::function<void(const int&)>, const QoSPolicy&);
template std::shared_ptr<Puller> PushPullPattern::createPuller<float>(const std::string&, std::function<void(const float&)>, const QoSPolicy&);
template std::shared_ptr<Puller> PushPullPattern::createPuller<int>(const std::string&, std::function<void(const int&)>);
template std::shared_ptr<Puller> PushPullPattern::createPuller<float>(const std::string&, std::function<void(const float&)>);
template void PubSubPattern::publishToTopic<int>(const std::string&, const int&);
template void PubSubPattern::publishToTopic<float>(const std::string&, const float&);
template void PubSubPattern::publishToTopic<double>(const std::string&, const double&);
template void PubSubPattern::publishToTopic<std::vector<uint8_t>>(const std::string&, const std::vector<uint8_t>&);

} // namespace communication
} // namespace aurorart