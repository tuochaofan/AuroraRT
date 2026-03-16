#include "aurorart/communication/communication_pattern.h"
#include "aurorart/utils/logger.h"

namespace aurorart {
namespace communication {

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

// EventNotifier implementation

EventNotifier::EventNotifier(const std::string& event, PubSubPattern& pubSubPattern)
    : event_(event), pubSubPattern_(pubSubPattern) {
}

void EventNotifier::notify() {
    // 发布事件通知
    int dummy = 0;
    pubSubPattern_.publishToTopic<int>(event_, dummy);
}

// EventListener implementation

EventListener::EventListener(const std::string& event, std::function<void()> callback, PubSubPattern& pubSubPattern)
    : event_(event), callback_(callback), pubSubPattern_(pubSubPattern) {
    // 创建事件订阅者
    subscriber_ = pubSubPattern_.createSubscriber<int>(event, [this](const int&) {
        callback_();
    });
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

// EventPattern methods

std::shared_ptr<Notifier> EventPattern::createNotifier(const std::string& event) {
    return std::make_shared<EventNotifier>(event, *pubSubPattern_);
}

std::shared_ptr<Listener> EventPattern::createListener(const std::string& event, 
                                                     std::function<void()> callback) {
    return std::make_shared<EventListener>(event, callback, *pubSubPattern_);
}

} // namespace communication
} // namespace aurorart
