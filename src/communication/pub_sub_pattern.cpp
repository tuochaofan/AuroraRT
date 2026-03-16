#include "aurorart/communication/communication_pattern.h"
#include "aurorart/transport/transport.h"
#include "aurorart/utils/logger.h"

namespace aurorart {
namespace communication {

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
}

// ConcretePublisher implementation
template<typename T>
ConcretePublisher<T>::ConcretePublisher(const std::string& topic, PubSubPattern& pubSubPattern) 
    : topic_(topic), pubSubPattern_(pubSubPattern), running_(true) {
    transport_ = transport::TransportManager::instance().selectOptimalTransport(sizeof(T), true);
    AURORA_LOG_INFO("Created publisher for topic: {}", topic_);
}

template<typename T>
void ConcretePublisher<T>::publish(const void* data, size_t size) {
    if (!running_) {
        AURORA_LOG_WARN("Publisher not running, cannot publish to topic: {}", topic_);
        return;
    }
    
    if (!transport_) {
        AURORA_LOG_ERROR("No transport available for publisher, topic: {}", topic_);
        return;
    }
    
    // 发送到网络传输
    if (!transport_->send(data, size)) {
        AURORA_LOG_WARN("Failed to send data over transport for topic: {}", topic_);
    }
    
    // 同时发布到本地订阅者
    pubSubPattern_.publishToTopic<T>(topic_, *static_cast<const T*>(data));
    
    AURORA_LOG_DEBUG("Published data to topic: {}, size: {} bytes", topic_, size);
}

// PubSubPattern template methods
template<typename T>
std::shared_ptr<Publisher<T>> PubSubPattern::createPublisher(const std::string& topic) {
    auto publisher = std::make_shared<ConcretePublisher<T>>(topic, *this);
    AURORA_LOG_INFO("Created publisher for topic: {}", topic);
    return publisher;
}

template<typename T>
std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber(const std::string& topic, 
                                                              std::function<void(const T&)> callback, 
                                                              const QoSPolicy& qos) {
    auto subscriber = std::make_shared<Subscriber<T>>(callback, qos);
    
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    subscribers_[topic].push_back(subscriber);
    
    subscriber->start();
    AURORA_LOG_INFO("Created subscriber for topic: {}, QoS: {}", topic, qos.getPriority());
    return subscriber;
}

template<typename T>
void PubSubPattern::publishToTopic(const std::string& topic, const T& data) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    auto it = subscribers_.find(topic);
    if (it != subscribers_.end()) {
        size_t delivered_count = 0;
        for (auto& subscriber : it->second) {
            auto typedSubscriber = dynamic_cast<Subscriber<T>*>(subscriber.get());
            if (typedSubscriber) {
                if (typedSubscriber->enqueue(data)) {
                    delivered_count++;
                } else {
                    AURORA_LOG_WARN("Failed to enqueue data for subscriber, topic: {}", topic);
                }
            }
        }
        AURORA_LOG_DEBUG("Delivered data to {} subscribers for topic: {}", delivered_count, topic);
    } else {
        AURORA_LOG_DEBUG("No subscribers for topic: {}", topic);
    }
}

// Subscriber implementation

SubscriberBase::SubscriberBase() : running_(false) {
}

void SubscriberBase::start() {
    running_ = true;
}

void SubscriberBase::stop() {
    running_ = false;
}

template<typename T>
Subscriber<T>::Subscriber(std::function<void(const T&)> callback) 
    : callback_(callback), running_(false) {
}

template<typename T>
void Subscriber<T>::start() {
    running_ = true;
    thread_ = std::thread(&Subscriber::processMessages, this);
}

template<typename T>
void Subscriber<T>::stop() {
    running_ = false;
    cv_.notify_one();
    if (thread_.joinable()) {
        thread_.join();
    }
}

template<typename T>
void Subscriber<T>::enqueue(const T& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(data);
    cv_.notify_one();
}

template<typename T>
void Subscriber<T>::processMessages() {
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

// 显式实例化常用类型
template class ConcretePublisher<int>;
template class ConcretePublisher<float>;
template class ConcretePublisher<double>;
template class ConcretePublisher<std::string>;
template class Subscriber<int>;
template class Subscriber<float>;
template class Subscriber<double>;
template class Subscriber<std::string>;
template std::shared_ptr<Publisher<int>> PubSubPattern::createPublisher<int>(const std::string&);
template std::shared_ptr<Publisher<float>> PubSubPattern::createPublisher<float>(const std::string&);
template std::shared_ptr<Publisher<double>> PubSubPattern::createPublisher<double>(const std::string&);
template std::shared_ptr<Publisher<std::string>> PubSubPattern::createPublisher<std::string>(const std::string&);
template std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber<int>(const std::string&, std::function<void(const int&)>);
template std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber<float>(const std::string&, std::function<void(const float&)>);
template std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber<double>(const std::string&, std::function<void(const double&)>);
template std::shared_ptr<SubscriberBase> PubSubPattern::createSubscriber<std::string>(const std::string&, std::function<void(const std::string&)>);
template void PubSubPattern::publishToTopic<int>(const std::string&, const int&);
template void PubSubPattern::publishToTopic<float>(const std::string&, const float&);
template void PubSubPattern::publishToTopic<double>(const std::string&, const double&);
template void PubSubPattern::publishToTopic<std::string>(const std::string&, const std::string&);

} // namespace communication
} // namespace aurorart
