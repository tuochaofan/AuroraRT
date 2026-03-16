#include "aurorart/communication/communication_pattern.h"
#include "aurorart/utils/logger.h"

namespace aurorart {
namespace communication {

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
}



// Client implementation
template<typename Req, typename Resp>
Client<Req, Resp>::Client(const std::string& service, PubSubPattern& pubSubPattern)
    : service_(service), pubSubPattern_(pubSubPattern), next_request_id_(0) {
    // 创建响应订阅者
    response_subscriber_ = pubSubPattern_.createSubscriber<typename Client<Req, Resp>::template ResponseWithId<Resp>>(service_ + ".response",
        [this](const typename Client<Req, Resp>::template ResponseWithId<Resp>& response) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = pending_requests_.find(response.request_id);
            if (it != pending_requests_.end()) {
                it->second(response.response);
                pending_requests_.erase(it);
                cv_.notify_one();
            }
        }
    );
}

template<typename Req, typename Resp>
void Client<Req, Resp>::sendRequest(const void* request, size_t requestSize, void* response, size_t responseSize) {
    const Req& req = *static_cast<const Req*>(request);
    uint64_t request_id = next_request_id_++;
    
    // 创建带ID的请求
    typename Client<Req, Resp>::template RequestWithId<Req> request_with_id;
    request_with_id.request_id = request_id;
    request_with_id.request = req;
    
    // 发送请求
    pubSubPattern_.publishToTopic<typename Client<Req, Resp>::template RequestWithId<Req>>(service_ + ".request", request_with_id);
    
    // 等待响应
    std::unique_lock<std::mutex> lock(mutex_);
    bool received = false;
    Resp resp;
    
    // 设置响应回调
    pending_requests_[request_id] = [&resp, &received](const Resp& response) {
        resp = response;
        received = true;
    };
    
    // 等待响应或超时
    if (cv_.wait_for(lock, std::chrono::seconds(5), [&received]() { return received; })) {
        // 收到响应
        memcpy(response, &resp, responseSize);
        AURORA_LOG_DEBUG("Received response for request ID: {}", request_id);
    } else {
        // 超时
        pending_requests_.erase(request_id);
        AURORA_LOG_WARN("Request timeout for service: {}", service_);
        // 返回默认响应
        Resp defaultResp;
        memcpy(response, &defaultResp, responseSize);
    }
}

// Server implementation
template<typename Req, typename Resp>
Server<Req, Resp>::Server(const std::string& service, std::function<Resp(const Req&)> handler, PubSubPattern& pubSubPattern)
    : service_(service), handler_(handler), pubSubPattern_(pubSubPattern), running_(false) {
    // 创建请求订阅者，处理带ID的请求
    pubSubPattern_.createSubscriber<typename Server<Req, Resp>::template RequestWithId<Req>>(service + ".request",
        [this](const typename Server<Req, Resp>::template RequestWithId<Req>& request_with_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            request_queue_.push(request_with_id);
            cv_.notify_one();
        }
    );
    
    AURORA_LOG_INFO("Created server for service: {}", service_);
}

template<typename Req, typename Resp>
void Server<Req, Resp>::start() {
    running_ = true;
    thread_ = std::thread(&Server::processRequests, this);
    AURORA_LOG_INFO("Server started for service: {}", service_);
}

template<typename Req, typename Resp>
void Server<Req, Resp>::stop() {
    running_ = false;
    cv_.notify_one();
    if (thread_.joinable()) {
        thread_.join();
    }
    AURORA_LOG_INFO("Server stopped for service: {}", service_);
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
            auto request_with_id = request_queue_.front();
            request_queue_.pop();
            lock.unlock();
            
            try {
                // 处理请求
                Resp response = handler_(request_with_id.request);
                
                // 创建带ID的响应
                typename Server<Req, Resp>::template ResponseWithId<Resp> response_with_id;
                response_with_id.request_id = request_with_id.request_id;
                response_with_id.response = response;
                
                // 发送响应
                pubSubPattern_.publishToTopic<typename Server<Req, Resp>::template ResponseWithId<Resp>>(service_ + ".response", response_with_id);
                
                AURORA_LOG_DEBUG("Processed request ID: {} for service: {}", request_with_id.request_id, service_);
            } catch (const std::exception& e) {
                AURORA_LOG_ERROR("Error processing request for service {}: {}", service_, e.what());
                // 发送错误响应
                typename Server<Req, Resp>::template ResponseWithId<Resp> error_response;
                error_response.request_id = request_with_id.request_id;
                error_response.response = Resp(); // 默认响应
                pubSubPattern_.publishToTopic<typename Server<Req, Resp>::template ResponseWithId<Resp>>(service_ + ".response", error_response);
            }
        }
    }
}

// ReqRespPattern template methods
template<typename Req, typename Resp>
std::shared_ptr<ClientBase> ReqRespPattern::createClient(const std::string& service) {
    return std::make_shared<Client<Req, Resp>>(service, *pubSubPattern_);
}

template<typename Req, typename Resp>
std::shared_ptr<ServerBase> ReqRespPattern::createServer(const std::string& service, 
                                                       std::function<Resp(const Req&)> handler) {
    return std::make_shared<Server<Req, Resp>>(service, handler, *pubSubPattern_);
}

// 显式实例化常用类型
template class Client<int, int>;
template class Client<float, float>;
template class Client<std::string, std::string>;
template class Server<int, int>;
template class Server<float, float>;
template class Server<std::string, std::string>;

template std::shared_ptr<ClientBase> ReqRespPattern::createClient<int, int>(const std::string&);
template std::shared_ptr<ClientBase> ReqRespPattern::createClient<float, float>(const std::string&);
template std::shared_ptr<ClientBase> ReqRespPattern::createClient<std::string, std::string>(const std::string&);
template std::shared_ptr<ServerBase> ReqRespPattern::createServer<int, int>(const std::string&, std::function<int(const int&)>);
template std::shared_ptr<ServerBase> ReqRespPattern::createServer<float, float>(const std::string&, std::function<float(const float&)>);
template std::shared_ptr<ServerBase> ReqRespPattern::createServer<std::string, std::string>(const std::string&, std::function<std::string(const std::string&)>);

} // namespace communication
} // namespace aurorart
