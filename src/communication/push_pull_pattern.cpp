#include "aurorart/communication/communication_pattern.h"
#include "aurorart/utils/logger.h"

namespace aurorart {
namespace communication {

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

// PushNotifier implementation

PushNotifier::PushNotifier(const std::string& channel, PubSubPattern& pubSubPattern)
    : channel_(channel), pubSubPattern_(pubSubPattern) {
}

void PushNotifier::push(const void* data, size_t size) {
    // 简化实现：通过发布-订阅模式推送数据
    pubSubPattern_.publishToTopic<std::vector<uint8_t>>(channel_, 
        std::vector<uint8_t>(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size));
}

// PullListener implementation
template<typename T>
PullListener::PullListener(const std::string& channel, std::function<void(const T&)> callback, PubSubPattern& pubSubPattern)
    : channel_(channel), pubSubPattern_(pubSubPattern) {
    // 创建数据订阅者
    subscriber_ = pubSubPattern_.createSubscriber<T>(channel, callback);
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

// PushPullPattern methods

std::shared_ptr<Pusher> PushPullPattern::createPusher(const std::string& channel) {
    return std::make_shared<PushNotifier>(channel, *pubSubPattern_);
}

template<typename T>
std::shared_ptr<Puller> PushPullPattern::createPuller(const std::string& channel, 
                                                     std::function<void(const T&)> callback) {
    return std::make_shared<PullListener>(channel, callback, *pubSubPattern_);
}

// 显式实例化常用类型
template PullListener::PullListener(const std::string&, std::function<void(const std::vector<uint8_t>&)>, PubSubPattern&);
template std::shared_ptr<Puller> PushPullPattern::createPuller<std::vector<uint8_t>>(const std::string&, std::function<void(const std::vector<uint8_t>&)>);

} // namespace communication
} // namespace aurorart
