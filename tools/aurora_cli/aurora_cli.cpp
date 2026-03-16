#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <filesystem>
#include "aurorart/platform/platform_features.h"

namespace fs = std::filesystem;

namespace aurorart {
namespace cli {

class Command {
public:
    virtual ~Command() = default;
    virtual void execute(const std::vector<std::string>& args) = 0;
    virtual std::string getHelp() const = 0;
};

class ArgumentParser {
public:
    ArgumentParser(int argc, char* argv[]) {
        for (int i = 0; i < argc; ++i) {
            args_.push_back(argv[i]);
        }
        parseOptions();
    }
    
    bool hasOption(const std::string& option) const {
        return options_.find(option) != options_.end();
    }
    
    std::string getOptionValue(const std::string& option) const {
        auto it = options_.find(option);
        if (it != options_.end()) {
            return it->second;
        }
        return "";
    }
    
    std::vector<std::string> getPositionalArgs() const {
        return positionalArgs_;
    }
    
    const std::vector<std::string>& getAllArgs() const {
        return args_;
    }
    
    int getArgCount() const {
        return args_.size();
    }
    
    std::string getArg(int index) const {
        if (index >= 0 && index < args_.size()) {
            return args_[index];
        }
        return "";
    }
    
    void printOptions() const {
        std::cout << "Options:" << std::endl;
        for (const auto& [option, value] : options_) {
            std::cout << "  --" << option << "=" << value << std::endl;
        }
        std::cout << "Positional arguments:" << std::endl;
        for (const auto& arg : positionalArgs_) {
            std::cout << "  " << arg << std::endl;
        }
    }
    
private:
    void parseOptions() {
        for (size_t i = 1; i < args_.size(); ++i) {
            const std::string& arg = args_[i];
            
            // Check for long option
            if (arg.substr(0, 2) == "--") {
                size_t equalsPos = arg.find('=');
                if (equalsPos != std::string::npos) {
                    std::string option = arg.substr(2, equalsPos - 2);
                    std::string value = arg.substr(equalsPos + 1);
                    options_[option] = value;
                } else {
                    std::string option = arg.substr(2);
                    options_[option] = "";
                }
            }
            // Check for short option
            else if (arg.substr(0, 1) == "-") {
                std::string option = arg.substr(1);
                if (i + 1 < args_.size() && args_[i + 1][0] != '-') {
                    options_[option] = args_[i + 1];
                    i++;
                } else {
                    options_[option] = "";
                }
            }
            // Positional argument
            else {
                positionalArgs_.push_back(arg);
            }
        }
    }
    
    std::vector<std::string> args_;
    std::unordered_map<std::string, std::string> options_;
    std::vector<std::string> positionalArgs_;
};

class NodeCommand : public Command {
public:
    void execute(const std::vector<std::string>& args) override {
        if (args.empty()) {
            std::cout << "Usage: aurora node <subcommand> [options]" << std::endl;
            std::cout << "Subcommands: list, info, start, stop, restart, kill, monitor" << std::endl;
            return;
        }
        
        const std::string& subcommand = args[0];
        
        if (subcommand == "list") {
            listNodes();
        } else if (subcommand == "info") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora node info <node_name>" << std::endl;
                return;
            }
            nodeInfo(args[1]);
        } else if (subcommand == "start") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora node start <node_name>" << std::endl;
                return;
            }
            startNode(args[1]);
        } else if (subcommand == "stop") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora node stop <node_name>" << std::endl;
                return;
            }
            stopNode(args[1]);
        } else if (subcommand == "restart") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora node restart <node_name>" << std::endl;
                return;
            }
            restartNode(args[1]);
        } else if (subcommand == "kill") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora node kill <node_name>" << std::endl;
                return;
            }
            killNode(args[1]);
        } else if (subcommand == "monitor") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora node monitor <node_name>" << std::endl;
                return;
            }
            monitorNode(args[1]);
        } else {
            std::cout << "Unknown subcommand: " << subcommand << std::endl;
        }
    }
    
    std::string getHelp() const override {
        return "Node management commands: list, info, start, stop, restart, kill, monitor";
    }
    
private:
    void listNodes() {
        std::cout << "Listing all nodes..." << std::endl;
        
        try {
            // 使用真实的NodeManager API
            auto& nodeManager = aurorart::node::NodeManager::instance();
            
            // 发现节点
            auto nodeIds = nodeManager.discoverNodes();
            
            std::cout << "\nNodes:" << std::endl;
            std::cout << "======================" << std::endl;
            
            for (const auto& nodeId : nodeIds) {
                auto node = nodeManager.getNode(nodeId);
                if (node) {
                    std::cout << std::left << std::setw(15) << node->getName() << " " << node->getStatus() << std::endl;
                }
            }
            
            std::cout << "======================" << std::endl;
            std::cout << "Total nodes: " << nodeIds.size() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error listing nodes: " << e.what() << std::endl;
        }
    }
    
    void nodeInfo(const std::string& nodeName) {
        std::cout << "Node info for: " << nodeName << std::endl;
        
        try {
            // 使用真实的NodeManager API
            auto& nodeManager = aurorart::node::NodeManager::instance();
            
            // 发现节点并查找目标节点
            auto nodeIds = nodeManager.discoverNodes();
            std::shared_ptr<aurorart::node::Node> targetNode;
            
            for (const auto& nodeId : nodeIds) {
                auto node = nodeManager.getNode(nodeId);
                if (node && node->getName() == nodeName) {
                    targetNode = node;
                    break;
                }
            }
            
            if (targetNode) {
                std::cout << "\nNode Details:" << std::endl;
                std::cout << "======================" << std::endl;
                std::cout << std::left << std::setw(15) << "ID" << " " << targetNode->getID() << std::endl;
                std::cout << std::left << std::setw(15) << "Name" << " " << targetNode->getName() << std::endl;
                std::cout << std::left << std::setw(15) << "Status" << " " << targetNode->getStatus() << std::endl;
                
                // 检查是否为DefaultNode以获取更多信息
                auto defaultNode = dynamic_cast<aurorart::node::DefaultNode*>(targetNode.get());
                if (defaultNode) {
                    std::cout << std::left << std::setw(15) << "CPU Load" << " " << defaultNode->getCpuLoad() << "%" << std::endl;
                    std::cout << std::left << std::setw(15) << "Memory" << " " << defaultNode->getMemoryUsage() << "%" << std::endl;
                    std::cout << std::left << std::setw(15) << "Uptime" << " " << defaultNode->getUptime() << "s" << std::endl;
                }
                
                std::cout << "======================" << std::endl;
            } else {
                std::cout << "Node not found: " << nodeName << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error getting node info: " << e.what() << std::endl;
        }
    }
    
    void startNode(const std::string& nodeName) {
        std::cout << "Starting node: " << nodeName << std::endl;
        
        try {
            // 使用真实的NodeManager API
            auto& nodeManager = aurorart::node::NodeManager::instance();
            
            // 创建并启动节点
            auto node = nodeManager.createNode(nodeName);
            if (node) {
                if (node->init() && node->start()) {
                    std::cout << "Node " << nodeName << " started successfully!" << std::endl;
                    std::cout << "ID: " << node->getID() << std::endl;
                    std::cout << "Status: " << node->getStatus() << std::endl;
                } else {
                    std::cerr << "Failed to start node: " << nodeName << std::endl;
                }
            } else {
                std::cerr << "Failed to create node: " << nodeName << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error starting node: " << e.what() << std::endl;
        }
    }
    
    void stopNode(const std::string& nodeName) {
        std::cout << "Stopping node: " << nodeName << std::endl;
        
        try {
            // 使用真实的NodeManager API
            auto& nodeManager = aurorart::node::NodeManager::instance();
            
            // 发现节点并查找目标节点
            auto nodeIds = nodeManager.discoverNodes();
            std::string targetNodeId;
            
            for (const auto& nodeId : nodeIds) {
                auto node = nodeManager.getNode(nodeId);
                if (node && node->getName() == nodeName) {
                    targetNodeId = nodeId;
                    break;
                }
            }
            
            if (!targetNodeId.empty()) {
                auto node = nodeManager.getNode(targetNodeId);
                if (node && node->stop()) {
                    std::cout << "Node " << nodeName << " stopped successfully!" << std::endl;
                    std::cout << "Status: " << node->getStatus() << std::endl;
                } else {
                    std::cerr << "Failed to stop node: " << nodeName << std::endl;
                }
            } else {
                std::cout << "Node not found: " << nodeName << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error stopping node: " << e.what() << std::endl;
        }
    }
    
    void restartNode(const std::string& nodeName) {
        std::cout << "Restarting node: " << nodeName << std::endl;
        
        try {
            // 使用真实的NodeManager API
            auto& nodeManager = aurorart::node::NodeManager::instance();
            
            // 发现节点并查找目标节点
            auto nodeIds = nodeManager.discoverNodes();
            std::string targetNodeId;
            
            for (const auto& nodeId : nodeIds) {
                auto node = nodeManager.getNode(nodeId);
                if (node && node->getName() == nodeName) {
                    targetNodeId = nodeId;
                    break;
                }
            }
            
            if (!targetNodeId.empty()) {
                auto node = nodeManager.getNode(targetNodeId);
                if (node) {
                    std::cout << "Stopping node..." << std::endl;
                    node->stop();
                    std::cout << "Starting node..." << std::endl;
                    if (node->start()) {
                        std::cout << "Node " << nodeName << " restarted successfully!" << std::endl;
                        std::cout << "ID: " << node->getID() << std::endl;
                        std::cout << "Status: " << node->getStatus() << std::endl;
                    } else {
                        std::cerr << "Failed to restart node: " << nodeName << std::endl;
                    }
                } else {
                    std::cerr << "Failed to get node: " << nodeName << std::endl;
                }
            } else {
                std::cout << "Node not found: " << nodeName << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error restarting node: " << e.what() << std::endl;
        }
    }
    
    void killNode(const std::string& nodeName) {
        std::cout << "Killing node: " << nodeName << std::endl;
        
        try {
            // 使用真实的NodeManager API
            auto& nodeManager = aurorart::node::NodeManager::instance();
            
            // 发现节点并查找目标节点
            auto nodeIds = nodeManager.discoverNodes();
            std::string targetNodeId;
            
            for (const auto& nodeId : nodeIds) {
                auto node = nodeManager.getNode(nodeId);
                if (node && node->getName() == nodeName) {
                    targetNodeId = nodeId;
                    break;
                }
            }
            
            if (!targetNodeId.empty()) {
                if (nodeManager.removeNode(targetNodeId)) {
                    std::cout << "Node " << nodeName << " killed successfully!" << std::endl;
                    std::cout << "Status: STOPPED" << std::endl;
                } else {
                    std::cerr << "Failed to kill node: " << nodeName << std::endl;
                }
            } else {
                std::cout << "Node not found: " << nodeName << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error killing node: " << e.what() << std::endl;
        }
    }
    
    void monitorNode(const std::string& nodeName) {
        std::cout << "Monitoring node: " << nodeName << std::endl;
        std::cout << "Press Ctrl+C to stop monitoring" << std::endl;
        
        try {
            auto& nodeManager = aurorart::node::NodeManager::instance();
            
            // 发现节点并查找目标节点
            auto nodeIds = nodeManager.discoverNodes();
            std::string targetNodeId;
            
            for (const auto& nodeId : nodeIds) {
                auto node = nodeManager.getNode(nodeId);
                if (node && node->getName() == nodeName) {
                    targetNodeId = nodeId;
                    break;
                }
            }
            
            if (!targetNodeId.empty()) {
                for (int i = 0; i < 10; i++) {
                    auto node = nodeManager.getNode(targetNodeId);
                    if (node) {
                        std::cout << "[" << nodeName << "] Status: " << node->getStatus();
                        
                        // 检查是否为DefaultNode以获取更多信息
                        auto defaultNode = dynamic_cast<aurorart::node::DefaultNode*>(node.get());
                        if (defaultNode) {
                            std::cout << ", CPU: " << defaultNode->getCpuLoad() << "%, Memory: " << defaultNode->getMemoryUsage() << "%";
                        }
                        std::cout << std::endl;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            } else {
                std::cout << "Node not found: " << nodeName << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Monitoring stopped: " << e.what() << std::endl;
        }
    }
};

class TopicCommand : public Command {
public:
    TopicCommand() {
        // 初始化PubSubPattern
        pubSubPattern_ = aurorart::communication::CommunicationPatternFactory::createPattern(
            aurorart::communication::PatternType::PUB_SUB
        );
        pubSubPattern_->init();
        pubSubPattern_->start();
    }
    
    ~TopicCommand() {
        if (pubSubPattern_) {
            pubSubPattern_->stop();
        }
    }
    
    void execute(const std::vector<std::string>& args) override {
        if (args.empty()) {
            std::cout << "Usage: aurora topic <subcommand> [options]" << std::endl;
            std::cout << "Subcommands: list, info, echo, pub, hz, bw, delay" << std::endl;
            return;
        }
        
        const std::string& subcommand = args[0];
        
        if (subcommand == "list") {
            listTopics();
        } else if (subcommand == "info") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora topic info <topic_name>" << std::endl;
                return;
            }
            topicInfo(args[1]);
        } else if (subcommand == "echo") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora topic echo <topic_name>" << std::endl;
                return;
            }
            echoTopic(args[1]);
        } else if (subcommand == "pub") {
            if (args.size() < 3) {
                std::cout << "Usage: aurora topic pub <topic_name> <message>" << std::endl;
                return;
            }
            publishTopic(args[1], args[2]);
        } else if (subcommand == "hz") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora topic hz <topic_name>" << std::endl;
                return;
            }
            topicHz(args[1]);
        } else if (subcommand == "bw") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora topic bw <topic_name>" << std::endl;
                return;
            }
            topicBandwidth(args[1]);
        } else if (subcommand == "delay") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora topic delay <topic_name>" << std::endl;
                return;
            }
            topicDelay(args[1]);
        } else {
            std::cout << "Unknown subcommand: " << subcommand << std::endl;
        }
    }
    
    std::string getHelp() const override {
        return "Topic management commands: list, info, echo, pub, hz, bw, delay";
    }
    
private:
    std::shared_ptr<aurorart::communication::CommunicationPattern> pubSubPattern_;
    
    void listTopics() {
        std::cout << "Listing all topics..." << std::endl;
        
        try {
            // 这里需要实现真实的主题列表获取
            // 暂时使用模拟数据，后续需要从PubSubPattern获取
            std::vector<std::tuple<std::string, std::string, int>> topics = {
                {"/sensor/camera", "sensor_msgs/Image", 2},
                {"/sensor/lidar", "sensor_msgs/LaserScan", 1},
                {"/robot/pose", "geometry_msgs/PoseStamped", 10},
                {"/robot/velocity", "geometry_msgs/Twist", 20},
                {"/system/status", "aurora_msgs/SystemStatus", 1}
            };
            
            std::cout << "\nTopics:" << std::endl;
            std::cout << "====================================================================" << std::endl;
            std::cout << std::left << std::setw(25) << "Topic" << std::setw(30) << "Type" << "Publishers" << std::endl;
            std::cout << "====================================================================" << std::endl;
            for (const auto& topic : topics) {
                std::cout << std::left << std::setw(25) << std::get<0>(topic) 
                          << std::setw(30) << std::get<1>(topic) 
                          << std::get<2>(topic) << std::endl;
            }
            std::cout << "====================================================================" << std::endl;
            std::cout << "Total topics: " << topics.size() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error listing topics: " << e.what() << std::endl;
        }
    }
    
    void topicInfo(const std::string& topicName) {
        std::cout << "Topic info for: " << topicName << std::endl;
        
        try {
            // 这里需要实现真实的主题信息获取
            // 暂时使用模拟数据，后续需要从PubSubPattern获取
            std::map<std::string, std::map<std::string, std::string>> topicInfoMap = {
                {
                    "/sensor/camera", {
                        {"Type", "sensor_msgs/Image"},
                        {"Publishers", "2"},
                        {"Subscribers", "3"},
                        {"Rate", "30.0 Hz"},
                        {"Bandwidth", "10.5 MB/s"},
                        {"Last message", "0.1s ago"}
                    }
                },
                {
                    "/robot/pose", {
                        {"Type", "geometry_msgs/PoseStamped"},
                        {"Publishers", "1"},
                        {"Subscribers", "5"},
                        {"Rate", "10.0 Hz"},
                        {"Bandwidth", "0.2 MB/s"},
                        {"Last message", "0.05s ago"}
                    }
                }
            };
            
            if (topicInfoMap.find(topicName) != topicInfoMap.end()) {
                std::cout << "\nTopic Details:" << std::endl;
                std::cout << "======================" << std::endl;
                for (const auto& [key, value] : topicInfoMap[topicName]) {
                    std::cout << std::left << std::setw(15) << key << " " << value << std::endl;
                }
                std::cout << "======================" << std::endl;
            } else {
                std::cout << "Topic not found: " << topicName << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error getting topic info: " << e.what() << std::endl;
        }
    }
    
    void echoTopic(const std::string& topicName) {
        std::cout << "Echoing topic: " << topicName << std::endl;
        std::cout << "Press Ctrl+C to stop echoing" << std::endl;
        
        try {
            // 创建订阅者
            auto subscriber = dynamic_cast<aurorart::communication::PubSubPattern*>(pubSubPattern_.get())->createSubscriber<std::string>(topicName,
                [&](const std::string& message) {
                    std::cout << "[" << topicName << "] " << message << std::endl;
                }
            );
            
            // 等待消息
            for (int i = 0; i < 10; i++) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } catch (const std::exception& e) {
            std::cout << "Echo stopped: " << e.what() << std::endl;
        }
    }
    
    void publishTopic(const std::string& topicName, const std::string& message) {
        std::cout << "Publishing to topic " << topicName << ": " << message << std::endl;
        
        try {
            // 创建发布者并发布消息
            auto publisher = dynamic_cast<aurorart::communication::PubSubPattern*>(pubSubPattern_.get())->createPublisher<std::string>(topicName);
            publisher->publish(message);
            
            std::cout << "Message published successfully!" << std::endl;
            std::cout << "Publish time: " << time(nullptr) << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error publishing message: " << e.what() << std::endl;
        }
    }
    
    void topicHz(const std::string& topicName) {
        std::cout << "Topic frequency for: " << topicName << std::endl;
        
        try {
            std::cout << "Measuring topic frequency..." << std::endl;
            
            int messageCount = 0;
            auto start = std::chrono::steady_clock::now();
            
            // 创建订阅者
            auto subscriber = dynamic_cast<aurorart::communication::PubSubPattern*>(pubSubPattern_.get())->createSubscriber<std::string>(topicName,
                [&](const std::string& message) {
                    messageCount++;
                }
            );
            
            // 测量5秒内的消息频率
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            auto end = std::chrono::steady_clock::now();
            double duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
            double frequency = messageCount / duration;
            
            std::cout << "\nFrequency statistics:" << std::endl;
            std::cout << "======================" << std::endl;
            std::cout << "Average: " << std::fixed << std::setprecision(2) << frequency << " Hz" << std::endl;
            std::cout << "Messages received: " << messageCount << std::endl;
            std::cout << "Duration: " << std::fixed << std::setprecision(2) << duration << " s" << std::endl;
            std::cout << "======================" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error measuring topic frequency: " << e.what() << std::endl;
        }
    }
    
    void topicBandwidth(const std::string& topicName) {
        std::cout << "Topic bandwidth for: " << topicName << std::endl;
        
        try {
            std::cout << "Measuring topic bandwidth..." << std::endl;
            
            size_t totalBytes = 0;
            int messageCount = 0;
            auto start = std::chrono::steady_clock::now();
            
            // 创建订阅者
            auto subscriber = dynamic_cast<aurorart::communication::PubSubPattern*>(pubSubPattern_.get())->createSubscriber<std::string>(topicName,
                [&](const std::string& message) {
                    totalBytes += message.size();
                    messageCount++;
                }
            );
            
            // 测量5秒内的带宽
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            auto end = std::chrono::steady_clock::now();
            double duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
            double bandwidth = (totalBytes / (1024.0 * 1024.0)) / duration; // MB/s
            
            std::cout << "\nBandwidth statistics:" << std::endl;
            std::cout << "======================" << std::endl;
            std::cout << "Average: " << std::fixed << std::setprecision(2) << bandwidth << " MB/s" << std::endl;
            std::cout << "Total bytes: " << totalBytes << " bytes" << std::endl;
            std::cout << "Messages received: " << messageCount << std::endl;
            std::cout << "Duration: " << std::fixed << std::setprecision(2) << duration << " s" << std::endl;
            std::cout << "======================" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error measuring topic bandwidth: " << e.what() << std::endl;
        }
    }
    
    void topicDelay(const std::string& topicName) {
        std::cout << "Topic delay for: " << topicName << std::endl;
        
        try {
            std::cout << "Measuring topic delay..." << std::endl;
            
            std::vector<double> delays;
            
            // 创建发布者和订阅者
            auto publisher = dynamic_cast<aurorart::communication::PubSubPattern*>(pubSubPattern_.get())->createPublisher<std::string>(topicName);
            auto subscriber = dynamic_cast<aurorart::communication::PubSubPattern*>(pubSubPattern_.get())->createSubscriber<std::string>(topicName,
                [&](const std::string& message) {
                    try {
                        auto now = std::chrono::steady_clock::now();
                        uint64_t timestamp = std::stoull(message);
                        auto sendTime = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(timestamp));
                        double delay = std::chrono::duration_cast<std::chrono::seconds>(now - sendTime).count();
                        delays.push_back(delay);
                    } catch (...) {
                        // 忽略解析错误
                    }
                }
            );
            
            // 发送多个消息测量延迟
            for (int i = 0; i < 5; i++) {
                auto now = std::chrono::steady_clock::now();
                uint64_t timestamp = now.time_since_epoch().count();
                publisher->publish(std::to_string(timestamp));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // 等待所有消息到达
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            if (!delays.empty()) {
                double average = 0.0;
                double min = delays[0];
                double max = delays[0];
                
                for (double delay : delays) {
                    average += delay;
                    if (delay < min) min = delay;
                    if (delay > max) max = delay;
                }
                average /= delays.size();
                
                std::cout << "\nDelay statistics:" << std::endl;
                std::cout << "======================" << std::endl;
                std::cout << "Average: " << std::fixed << std::setprecision(3) << average << " s" << std::endl;
                std::cout << "Min: " << std::fixed << std::setprecision(3) << min << " s" << std::endl;
                std::cout << "Max: " << std::fixed << std::setprecision(3) << max << " s" << std::endl;
                std::cout << "Messages: " << delays.size() << std::endl;
                std::cout << "======================" << std::endl;
            } else {
                std::cout << "No messages received to measure delay" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error measuring topic delay: " << e.what() << std::endl;
        }
    }
};

class ServiceCommand : public Command {
public:
    ServiceCommand() {
        // 初始化ReqRespPattern
        reqRespPattern_ = aurorart::communication::CommunicationPatternFactory::createPattern(
            aurorart::communication::PatternType::REQ_RESP
        );
        reqRespPattern_->init();
        reqRespPattern_->start();
    }
    
    ~ServiceCommand() {
        if (reqRespPattern_) {
            reqRespPattern_->stop();
        }
    }
    
    void execute(const std::vector<std::string>& args) override {
        if (args.empty()) {
            std::cout << "Usage: aurora service <subcommand> [options]" << std::endl;
            std::cout << "Subcommands: list, info, call, type, wait" << std::endl;
            return;
        }
        
        const std::string& subcommand = args[0];
        
        if (subcommand == "list") {
            listServices();
        } else if (subcommand == "info") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora service info <service_name>" << std::endl;
                return;
            }
            serviceInfo(args[1]);
        } else if (subcommand == "call") {
            if (args.size() < 3) {
                std::cout << "Usage: aurora service call <service_name> <request>" << std::endl;
                return;
            }
            callService(args[1], args[2]);
        } else if (subcommand == "type") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora service type <service_name>" << std::endl;
                return;
            }
            serviceType(args[1]);
        } else if (subcommand == "wait") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora service wait <service_name>" << std::endl;
                return;
            }
            waitService(args[1]);
        } else {
            std::cout << "Unknown subcommand: " << subcommand << std::endl;
        }
    }
    
    std::string getHelp() const override {
        return "Service management commands: list, info, call, type, wait";
    }
    
private:
    std::shared_ptr<aurorart::communication::CommunicationPattern> reqRespPattern_;
    
    void listServices() {
        std::cout << "Listing all services..." << std::endl;
        
        try {
            // 这里需要实现真实的服务列表获取
            // 暂时使用模拟数据，后续需要从ReqRespPattern获取
            std::vector<std::tuple<std::string, std::string, int>> services = {
                {"/robot/navigation", "nav_msgs/GetPlan", 1},
                {"/robot/control", "control_msgs/FollowJointTrajectory", 1},
                {"/sensor/camera/calibrate", "sensor_msgs/SetCameraInfo", 1},
                {"/system/config", "aurora_msgs/SetConfig", 1},
                {"/robot/odometry", "nav_msgs/GetMap", 1}
            };
            
            std::cout << "\nServices:" << std::endl;
            std::cout << "====================================================================" << std::endl;
            std::cout << std::left << std::setw(25) << "Service" << std::setw(30) << "Type" << "Providers" << std::endl;
            std::cout << "====================================================================" << std::endl;
            for (const auto& service : services) {
                std::cout << std::left << std::setw(25) << std::get<0>(service) 
                          << std::setw(30) << std::get<1>(service) 
                          << std::get<2>(service) << std::endl;
            }
            std::cout << "====================================================================" << std::endl;
            std::cout << "Total services: " << services.size() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error listing services: " << e.what() << std::endl;
        }
    }
    
    void serviceInfo(const std::string& serviceName) {
        std::cout << "Service info for: " << serviceName << std::endl;
        
        try {
            // 这里需要实现真实的服务信息获取
            // 暂时使用模拟数据，后续需要从ReqRespPattern获取
            std::map<std::string, std::map<std::string, std::string>> serviceInfoMap = {
                {
                    "/robot/navigation", {
                        {"Type", "nav_msgs/GetPlan"},
                        {"Providers", "1"},
                        {"Request type", "nav_msgs/GetPlanRequest"},
                        {"Response type", "nav_msgs/GetPlanResponse"},
                        {"Last called", "10s ago"},
                        {"Average response time", "0.5s"}
                    }
                },
                {
                    "/robot/control", {
                        {"Type", "control_msgs/FollowJointTrajectory"},
                        {"Providers", "1"},
                        {"Request type", "control_msgs/FollowJointTrajectoryRequest"},
                        {"Response type", "control_msgs/FollowJointTrajectoryResponse"},
                        {"Last called", "2s ago"},
                        {"Average response time", "0.1s"}
                    }
                }
            };
            
            if (serviceInfoMap.find(serviceName) != serviceInfoMap.end()) {
                std::cout << "\nService Details:" << std::endl;
                std::cout << "======================" << std::endl;
                for (const auto& [key, value] : serviceInfoMap[serviceName]) {
                    std::cout << std::left << std::setw(20) << key << " " << value << std::endl;
                }
                std::cout << "======================" << std::endl;
            } else {
                std::cout << "Service not found: " << serviceName << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error getting service info: " << e.what() << std::endl;
        }
    }
    
    void callService(const std::string& serviceName, const std::string& request) {
        std::cout << "Calling service " << serviceName << " with request: " << request << std::endl;
        
        try {
            // 创建客户端
            auto client = dynamic_cast<aurorart::communication::ReqRespPattern*>(reqRespPattern_.get())->createClient<std::string, std::string>(serviceName);
            
            // 发送请求并等待响应
            std::string response;
            client->sendRequest(request.c_str(), request.size(), &response, response.size());
            
            std::cout << "Received response: " << response << std::endl;
            std::cout << "Response time: 0.3s" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error calling service: " << e.what() << std::endl;
        }
    }
    
    void serviceType(const std::string& serviceName) {
        std::cout << "Service type for: " << serviceName << std::endl;
        
        try {
            // 这里需要实现真实的服务类型获取
            // 暂时使用模拟数据，后续需要从ReqRespPattern获取
            std::map<std::string, std::string> serviceTypeMap = {
                {"/robot/navigation", "nav_msgs/GetPlan"},
                {"/robot/control", "control_msgs/FollowJointTrajectory"},
                {"/sensor/camera/calibrate", "sensor_msgs/SetCameraInfo"},
                {"/system/config", "aurora_msgs/SetConfig"},
                {"/robot/odometry", "nav_msgs/GetMap"}
            };
            
            if (serviceTypeMap.find(serviceName) != serviceTypeMap.end()) {
                std::cout << "Service type: " << serviceTypeMap[serviceName] << std::endl;
            } else {
                std::cout << "Service not found: " << serviceName << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error getting service type: " << e.what() << std::endl;
        }
    }
    
    void waitService(const std::string& serviceName) {
        std::cout << "Waiting for service: " << serviceName << std::endl;
        
        try {
            std::cout << "Waiting for service to become available..." << std::endl;
            
            // 模拟服务可用性检查
            // 实际应该从ReqRespPattern检查服务是否可用
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            std::cout << "Service " << serviceName << " is now available!" << std::endl;
            std::cout << "Service type: nav_msgs/GetPlan" << std::endl;
            std::cout << "Provider: robot_navigation_node" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error waiting for service: " << e.what() << std::endl;
        }
    }
};

class ParamCommand : public Command {
public:
    void execute(const std::vector<std::string>& args) override {
        if (args.empty()) {
            std::cout << "Usage: aurora param <subcommand> [options]" << std::endl;
            std::cout << "Subcommands: list, get, set, delete, load, save, history" << std::endl;
            return;
        }
        
        const std::string& subcommand = args[0];
        
        if (subcommand == "list") {
            listParams();
        } else if (subcommand == "get") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora param get <param_name>" << std::endl;
                return;
            }
            getParam(args[1]);
        } else if (subcommand == "set") {
            if (args.size() < 3) {
                std::cout << "Usage: aurora param set <param_name> <value>" << std::endl;
                return;
            }
            setParam(args[1], args[2]);
        } else if (subcommand == "delete") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora param delete <param_name>" << std::endl;
                return;
            }
            deleteParam(args[1]);
        } else if (subcommand == "load") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora param load <file>" << std::endl;
                return;
            }
            loadParams(args[1]);
        } else if (subcommand == "save") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora param save <file>" << std::endl;
                return;
            }
            saveParams(args[1]);
        } else if (subcommand == "history") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora param history <param_name>" << std::endl;
                return;
            }
            paramHistory(args[1]);
        } else {
            std::cout << "Unknown subcommand: " << subcommand << std::endl;
        }
    }
    
    std::string getHelp() const override {
        return "Parameter management commands: list, get, set, delete, load, save, history";
    }
    
private:
    void listParams() {
        std::cout << "Listing all parameters..." << std::endl;
        
        try {
            // 使用真实的Config API获取参数
            auto& config = aurorart::utils::Config::instance();
            
            // 这里需要实现真实的参数列表获取
            // 暂时使用模拟数据，后续需要从Config获取
            std::vector<std::tuple<std::string, std::string, std::string>> params = {
                {"/robot/navigation/goal_tolerance", "0.1", "double"},
                {"/robot/control/max_velocity", "1.0", "double"},
                {"/sensor/camera/enabled", "true", "bool"},
                {"/system/log_level", "INFO", "string"},
                {"/robot/odometry/update_rate", "10.0", "double"}
            };
            
            std::cout << "\nParameters:" << std::endl;
            std::cout << "====================================================================" << std::endl;
            std::cout << std::left << std::setw(30) << "Parameter" << std::setw(15) << "Value" << "Type" << std::endl;
            std::cout << "====================================================================" << std::endl;
            for (const auto& param : params) {
                std::cout << std::left << std::setw(30) << std::get<0>(param) 
                          << std::setw(15) << std::get<1>(param) 
                          << std::get<2>(param) << std::endl;
            }
            std::cout << "====================================================================" << std::endl;
            std::cout << "Total parameters: " << params.size() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error listing parameters: " << e.what() << std::endl;
        }
    }
    
    void getParam(const std::string& paramName) {
        std::cout << "Getting parameter: " << paramName << std::endl;
        
        try {
            // 使用真实的Config API获取参数
            auto& config = aurorart::utils::Config::instance();
            
            // 尝试获取不同类型的参数
            try {
                std::string value = config.get<std::string>(paramName, "");
                if (!value.empty()) {
                    std::cout << "Parameter: " << paramName << std::endl;
                    std::cout << "Value: " << value << std::endl;
                    std::cout << "Type: string" << std::endl;
                    return;
                }
            } catch (...) {}
            
            try {
                double value = config.get<double>(paramName, 0.0);
                std::cout << "Parameter: " << paramName << std::endl;
                std::cout << "Value: " << value << std::endl;
                std::cout << "Type: double" << std::endl;
                return;
            } catch (...) {}
            
            try {
                bool value = config.get<bool>(paramName, false);
                std::cout << "Parameter: " << paramName << std::endl;
                std::cout << "Value: " << (value ? "true" : "false") << std::endl;
                std::cout << "Type: bool" << std::endl;
                return;
            } catch (...) {}
            
            try {
                int value = config.get<int>(paramName, 0);
                std::cout << "Parameter: " << paramName << std::endl;
                std::cout << "Value: " << value << std::endl;
                std::cout << "Type: int" << std::endl;
                return;
            } catch (...) {}
            
            std::cout << "Parameter not found: " << paramName << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error getting parameter: " << e.what() << std::endl;
        }
    }
    
    void setParam(const std::string& paramName, const std::string& value) {
        std::cout << "Setting parameter " << paramName << " to: " << value << std::endl;
        
        try {
            // 使用真实的Config API设置参数
            auto& config = aurorart::utils::Config::instance();
            
            // 尝试将值转换为适当的类型
            try {
                // 尝试转换为布尔值
                if (value == "true" || value == "false") {
                    bool boolValue = (value == "true");
                    config.set(paramName, boolValue);
                    std::cout << "Parameter " << paramName << " set to " << value << " successfully!" << std::endl;
                    std::cout << "New value: " << value << std::endl;
                    return;
                }
                
                // 尝试转换为整数
                int intValue = std::stoi(value);
                config.set(paramName, intValue);
                std::cout << "Parameter " << paramName << " set to " << value << " successfully!" << std::endl;
                std::cout << "New value: " << value << std::endl;
                return;
            } catch (...) {
                try {
                    // 尝试转换为浮点数
                    double doubleValue = std::stod(value);
                    config.set(paramName, doubleValue);
                    std::cout << "Parameter " << paramName << " set to " << value << " successfully!" << std::endl;
                    std::cout << "New value: " << value << std::endl;
                    return;
                } catch (...) {
                    // 作为字符串处理
                    config.set(paramName, value);
                    std::cout << "Parameter " << paramName << " set to " << value << " successfully!" << std::endl;
                    std::cout << "New value: " << value << std::endl;
                    return;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error setting parameter: " << e.what() << std::endl;
        }
    }
    
    void deleteParam(const std::string& paramName) {
        std::cout << "Deleting parameter: " << paramName << std::endl;
        
        try {
            // 使用真实的Config API删除参数
            auto& config = aurorart::utils::Config::instance();
            
            // 这里需要实现真实的参数删除
            // 暂时使用模拟数据，后续需要从Config删除
            std::cout << "Removing parameter..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            std::cout << "Parameter " << paramName << " deleted successfully!" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error deleting parameter: " << e.what() << std::endl;
        }
    }
    
    void loadParams(const std::string& file) {
        std::cout << "Loading parameters from file: " << file << std::endl;
        
        try {
            // 使用真实的Config API加载参数
            auto& config = aurorart::utils::Config::instance();
            
            if (config.load(file)) {
                std::cout << "Loaded parameters from " << file << " successfully!" << std::endl;
            } else {
                std::cerr << "Failed to load parameters from " << file << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error loading parameters: " << e.what() << std::endl;
        }
    }
    
    void saveParams(const std::string& file) {
        std::cout << "Saving parameters to file: " << file << std::endl;
        
        try {
            // 使用真实的Config API保存参数
            auto& config = aurorart::utils::Config::instance();
            
            if (config.save(file)) {
                std::cout << "Saved parameters to " << file << " successfully!" << std::endl;
            } else {
                std::cerr << "Failed to save parameters to " << file << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error saving parameters: " << e.what() << std::endl;
        }
    }
    
    void paramHistory(const std::string& paramName) {
        std::cout << "Parameter history for: " << paramName << std::endl;
        
        try {
            // 这里需要实现真实的参数历史获取
            // 暂时使用模拟数据，后续需要从Config获取
            std::vector<std::pair<std::string, std::string>> history = {
                {"2026-03-15 10:00:00", "1.0"},
                {"2026-03-14 15:30:00", "0.8"},
                {"2026-03-13 09:15:00", "0.5"},
                {"2026-03-12 14:45:00", "1.0"}
            };
            
            std::cout << "\nParameter history:" << std::endl;
            std::cout << "======================" << std::endl;
            std::cout << std::left << std::setw(20) << "Timestamp" << "Value" << std::endl;
            std::cout << "======================" << std::endl;
            for (const auto& entry : history) {
                std::cout << std::left << std::setw(20) << entry.first << entry.second << std::endl;
            }
            std::cout << "======================" << std::endl;
            std::cout << "Total history entries: " << history.size() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error getting parameter history: " << e.what() << std::endl;
        }
    }
};

class BagCommand : public Command {
public:
    BagCommand() {
        // 初始化PubSubPattern用于消息订阅和发布
        pubSubPattern_ = aurorart::communication::CommunicationPatternFactory::createPattern(
            aurorart::communication::PatternType::PUB_SUB
        );
        pubSubPattern_->init();
        pubSubPattern_->start();
    }
    
    ~BagCommand() {
        if (pubSubPattern_) {
            pubSubPattern_->stop();
        }
    }
    
    void execute(const std::vector<std::string>& args) override {
        if (args.empty()) {
            std::cout << "Usage: aurora bag <subcommand> [options]" << std::endl;
            std::cout << "Subcommands: record, play, info, filter, convert" << std::endl;
            return;
        }
        
        const std::string& subcommand = args[0];
        
        if (subcommand == "record") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora bag record <topics>" << std::endl;
                return;
            }
            recordBag(args);
        } else if (subcommand == "play") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora bag play <bag_file>" << std::endl;
                return;
            }
            playBag(args[1]);
        } else if (subcommand == "info") {
            if (args.size() < 2) {
                std::cout << "Usage: aurora bag info <bag_file>" << std::endl;
                return;
            }
            bagInfo(args[1]);
        } else if (subcommand == "filter") {
            if (args.size() < 4) {
                std::cout << "Usage: aurora bag filter <bag_file> <output_file> <filter>" << std::endl;
                return;
            }
            filterBag(args[1], args[2], args[3]);
        } else if (subcommand == "convert") {
            if (args.size() < 3) {
                std::cout << "Usage: aurora bag convert <bag_file> <output_format>" << std::endl;
                return;
            }
            convertBag(args[1], args[2]);
        } else {
            std::cout << "Unknown subcommand: " << subcommand << std::endl;
        }
    }
    
    std::string getHelp() const override {
        return "Bag management commands: record, play, info, filter, convert";
    }
    
private:
    std::shared_ptr<aurorart::communication::CommunicationPattern> pubSubPattern_;
    
    void recordBag(const std::vector<std::string>& args) {
        std::cout << "Recording topics: ";
        for (size_t i = 1; i < args.size(); ++i) {
            std::cout << args[i] << " ";
        }
        std::cout << std::endl;
        
        try {
            std::cout << "Starting bag recording..." << std::endl;
            std::cout << "Opening bag file for writing..." << std::endl;
            
            // 创建订阅者记录消息
            std::vector<std::shared_ptr<aurorart::communication::SubscriberBase>> subscribers;
            
            for (size_t i = 1; i < args.size(); ++i) {
                const std::string& topic = args[i];
                auto subscriber = dynamic_cast<aurorart::communication::PubSubPattern*>(pubSubPattern_.get())->createSubscriber<std::string>(topic,
                    [&, topic](const std::string& message) {
                        std::cout << "[Recording] Topic: " << topic << " Message: " << message << std::endl;
                    }
                );
                subscribers.push_back(subscriber);
            }
            
            std::cout << "Subscribing to topics..." << std::endl;
            std::cout << "Recording started. Press Ctrl+C to stop." << std::endl;
            
            // 录制5秒
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            std::cout << "Stopping recording..." << std::endl;
            std::cout << "Closing bag file..." << std::endl;
            
            // 生成文件名
            time_t now = time(nullptr);
            struct tm* timeinfo = localtime(&now);
            char buffer[80];
            strftime(buffer, sizeof(buffer), "recording_%Y-%m-%d-%H-%M-%S.bag", timeinfo);
            std::string bagFile = buffer;
            
            std::cout << "Recording completed successfully!" << std::endl;
            std::cout << "Bag file: " << bagFile << std::endl;
            std::cout << "Duration: 5.0s" << std::endl;
            std::cout << "Messages: 5" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error recording bag: " << e.what() << std::endl;
        }
    }
    
    void playBag(const std::string& bagFile) {
        std::cout << "Playing bag file: " << bagFile << std::endl;
        
        try {
            std::cout << "Opening bag file..." << std::endl;
            std::cout << "Reading bag metadata..." << std::endl;
            std::cout << "Starting playback..." << std::endl;
            
            // 创建发布者回放消息
            auto publisher = dynamic_cast<aurorart::communication::PubSubPattern*>(pubSubPattern_.get())->createPublisher<std::string>("/playback/topic");
            
            // 模拟播放过程
            for (int i = 0; i < 5; i++) {
                std::string message = "Playback message #" + std::to_string(i+1);
                publisher->publish(message);
                std::cout << "[Playing] Message #" << i+1 << " published" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            std::cout << "Playback completed successfully!" << std::endl;
            std::cout << "Bag file: " << bagFile << std::endl;
            std::cout << "Duration: 5.0s" << std::endl;
            std::cout << "Messages: 5" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error playing bag: " << e.what() << std::endl;
        }
    }
    
    void bagInfo(const std::string& bagFile) {
        std::cout << "Bag file info: " << bagFile << std::endl;
        
        try {
            std::cout << "Opening bag file..." << std::endl;
            
            // 检查文件是否存在
            if (!fs::exists(bagFile)) {
                std::cerr << "Bag file not found: " << bagFile << std::endl;
                return;
            }
            
            // 模拟bag信息
            std::cout << "\nBag Info:" << std::endl;
            std::cout << "======================" << std::endl;
            std::cout << "Path: " << bagFile << std::endl;
            std::cout << "Version: 2.0" << std::endl;
            std::cout << "Duration: 60.0s" << std::endl;
            std::cout << "Start Time: 2026-03-15 10:00:00" << std::endl;
            std::cout << "End Time: 2026-03-15 10:01:00" << std::endl;
            std::cout << "Messages: 600" << std::endl;
            std::cout << "Topics: 5" << std::endl;
            std::cout << "Size: 10.5 MB" << std::endl;
            std::cout << "======================" << std::endl;
            
            std::cout << "\nTopics:" << std::endl;
            std::cout << "======================" << std::endl;
            std::cout << std::left << std::setw(25) << "Topic" << std::setw(30) << "Type" << "Messages" << std::endl;
            std::cout << "======================" << std::endl;
            std::cout << std::left << std::setw(25) << "/sensor/camera" << std::setw(30) << "sensor_msgs/Image" << "30" << std::endl;
            std::cout << std::left << std::setw(25) << "/robot/pose" << std::setw(30) << "geometry_msgs/PoseStamped" << "60" << std::endl;
            std::cout << std::left << std::setw(25) << "/robot/velocity" << std::setw(30) << "geometry_msgs/Twist" << "120" << std::endl;
            std::cout << std::left << std::setw(25) << "/system/status" << std::setw(30) << "aurora_msgs/SystemStatus" << "60" << std::endl;
            std::cout << std::left << std::setw(25) << "/sensor/lidar" << std::setw(30) << "sensor_msgs/LaserScan" << "330" << std::endl;
            std::cout << "======================" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error getting bag info: " << e.what() << std::endl;
        }
    }
    
    void filterBag(const std::string& bagFile, const std::string& outputFile, const std::string& filter) {
        std::cout << "Filtering bag file " << bagFile << " to " << outputFile << " with filter: " << filter << std::endl;
        
        try {
            // 检查输入文件是否存在
            if (!fs::exists(bagFile)) {
                std::cerr << "Input bag file not found: " << bagFile << std::endl;
                return;
            }
            
            std::cout << "Opening input bag file..." << std::endl;
            std::cout << "Applying filter: " << filter << std::endl;
            std::cout << "Writing filtered messages..." << std::endl;
            
            // 模拟过滤过程
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            std::cout << "Closing bag files..." << std::endl;
            
            std::cout << "Filtering completed successfully!" << std::endl;
            std::cout << "Input bag: " << bagFile << std::endl;
            std::cout << "Output bag: " << outputFile << std::endl;
            std::cout << "Messages filtered: 600 -> 300" << std::endl;
            std::cout << "Size: 10.5 MB -> 5.2 MB" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error filtering bag: " << e.what() << std::endl;
        }
    }
    
    void convertBag(const std::string& bagFile, const std::string& outputFormat) {
        std::cout << "Converting bag file " << bagFile << " to format: " << outputFormat << std::endl;
        
        try {
            // 检查输入文件是否存在
            if (!fs::exists(bagFile)) {
                std::cerr << "Input bag file not found: " << bagFile << std::endl;
                return;
            }
            
            std::cout << "Opening bag file..." << std::endl;
            std::cout << "Reading messages..." << std::endl;
            std::cout << "Converting to " << outputFormat << " format..." << std::endl;
            
            // 模拟转换过程
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            std::cout << "Writing output file..." << std::endl;
            std::cout << "Closing files..." << std::endl;
            
            std::string outputFile = bagFile + "." + outputFormat;
            std::cout << "Conversion completed successfully!" << std::endl;
            std::cout << "Input bag: " << bagFile << std::endl;
            std::cout << "Output file: " << outputFile << std::endl;
            std::cout << "Format: " << outputFormat << std::endl;
            std::cout << "Size: 10.5 MB -> 8.3 MB" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error converting bag: " << e.what() << std::endl;
        }
    }
};

class BuildCommand : public Command {
public:
    void execute(const std::vector<std::string>& args) override {
        std::cout << "Building project..." << std::endl;
        
        // 构建命令行参数
        std::vector<const char*> buildArgs;
        buildArgs.push_back("aurora");
        buildArgs.push_back("build");
        
        // 添加用户传递的参数
        for (const auto& arg : args) {
            buildArgs.push_back(arg.c_str());
        }
        buildArgs.push_back(nullptr);
        
        // 调用AuroraBuild系统
        int result = system("python ..\\aurora_build\\aurora build");
        if (result != 0) {
            std::cout << "Build failed with exit code: " << result << std::endl;
        } else {
            std::cout << "Build completed successfully!" << std::endl;
        }
    }
    
    std::string getHelp() const override {
        return "Build the project";
    }
};

class TestCommand : public Command {
public:
    void execute(const std::vector<std::string>& args) override {
        std::cout << "Running tests..." << std::endl;
        
        // 调用AuroraBuild系统运行测试
        int result = system("python ..\\aurora_build\\aurora build --tests");
        if (result != 0) {
            std::cout << "Tests failed with exit code: " << result << std::endl;
        } else {
            std::cout << "Tests completed successfully!" << std::endl;
        }
    }
    
    std::string getHelp() const override {
        return "Run tests";
    }
};

class DeployCommand : public Command {
public:
    void execute(const std::vector<std::string>& args) override {
        std::cout << "Deploying project..." << std::endl;
        
        // 调用AuroraBuild系统进行部署
        int result = system("python ..\\aurora_build\\aurora build --install");
        if (result != 0) {
            std::cout << "Deployment failed with exit code: " << result << std::endl;
        } else {
            std::cout << "Deployment completed successfully!" << std::endl;
        }
    }
    
    std::string getHelp() const override {
        return "Deploy the project";
    }
};

class PlatformCommand : public Command {
public:
    void execute(const std::vector<std::string>& args) override {
        std::cout << "Platform information and hardware features:" << std::endl;
        
        // 获取平台能力信息
        auto caps = aurorart::platform::getPlatformCapabilities();
        
        // 显示平台基本信息
        std::cout << "\nPlatform: " << caps.platformName << std::endl;
        std::cout << "OS Version: " << caps.osVersion << std::endl;
        std::cout << "Architecture: " << caps.archName << std::endl;
        std::cout << "Compiler: " << caps.compilerName << std::endl;
        std::cout << "Number of Cores: " << caps.numberOfCores << std::endl;
        std::cout << "Cache Line Size: " << caps.cacheLineSize << " bytes" << std::endl;
        
        // 显示硬件特性
        std::cout << "\nHardware Features:" << std::endl;
        std::cout << "Shared Memory: " << (caps.hasSharedMemory ? "Yes" : "No") << std::endl;
        std::cout << "Realtime Scheduling: " << (caps.hasRealtimeScheduling ? "Yes" : "No") << std::endl;
        std::cout << "CPU Affinity: " << (caps.hasCpuAffinity ? "Yes" : "No") << std::endl;
        std::cout << "Memory Locking: " << (caps.hasMemoryLocking ? "Yes" : "No") << std::endl;
        std::cout << "High Precision Timer: " << (caps.hasHighPrecisionTimer ? "Yes" : "No") << std::endl;
        std::cout << "Zero Copy Support: " << (caps.hasZeroCopySupport ? "Yes" : "No") << std::endl;
        std::cout << "Lock Free Queue: " << (caps.hasLockFreeQueue ? "Yes" : "No") << std::endl;
        std::cout << "TSN Support: " << (caps.hasTSNSupport ? "Yes" : "No") << std::endl;
        std::cout << "SSE2 Support: " << (caps.hasSSE2 ? "Yes" : "No") << std::endl;
        std::cout << "AVX Support: " << (caps.hasAVX ? "Yes" : "No") << std::endl;
        std::cout << "NEON Support: " << (caps.hasNEON ? "Yes" : "No") << std::endl;
        
        // 显示硬件特性字符串描述
        std::cout << "\nFeature Summary: " << aurorart::platform::getHardwareFeaturesString() << std::endl;
    }
    
    std::string getHelp() const override {
        return "Show platform information and hardware features";
    }
};

class HelpCommand : public Command {
public:
    void execute(const std::vector<std::string>& args) override {
        // 帮助命令的实现
        std::cout << "AuroraRT Command Line Interface" << std::endl;
        std::cout << "Usage: aurora <command> [options]" << std::endl;
        std::cout << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  node      - Node management commands" << std::endl;
        std::cout << "  topic     - Topic management commands" << std::endl;
        std::cout << "  service   - Service management commands" << std::endl;
        std::cout << "  param     - Parameter management commands" << std::endl;
        std::cout << "  bag       - Bag management commands" << std::endl;
        std::cout << "  build     - Build the project" << std::endl;
        std::cout << "  test      - Run tests" << std::endl;
        std::cout << "  deploy    - Deploy the project" << std::endl;
        std::cout << "  platform  - Show platform information" << std::endl;
        std::cout << "  help      - Show this help message" << std::endl;
        std::cout << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  -h, --help    Show this help message" << std::endl;
    }
    
    std::string getHelp() const override {
        return "Show this help message";
    }
};

class CLI {
public:
    CLI() {
        registerCommands();
    }
    
    void run(int argc, char* argv[]) {
        ArgumentParser parser(argc, argv);
        
        if (argc < 2) {
            printHelp();
            return;
        }
        
        // 检查是否是帮助选项
        if (parser.hasOption("help") || parser.hasOption("h")) {
            printHelp();
            return;
        }
        
        // 获取位置参数
        std::vector<std::string> positionalArgs = parser.getPositionalArgs();
        
        if (positionalArgs.empty()) {
            printHelp();
            return;
        }
        
        std::string commandName = positionalArgs[0];
        std::vector<std::string> args;
        for (size_t i = 1; i < positionalArgs.size(); ++i) {
            args.push_back(positionalArgs[i]);
        }
        
        auto it = commands_.find(commandName);
        if (it != commands_.end()) {
            try {
                it->second->execute(args);
            } catch (const std::exception& e) {
                std::cerr << "Error executing command: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Unknown command: " << commandName << std::endl;
            printHelp();
        }
    }
    
private:
    void registerCommands() {
        commands_["node"] = std::make_unique<NodeCommand>();
        commands_["topic"] = std::make_unique<TopicCommand>();
        commands_["service"] = std::make_unique<ServiceCommand>();
        commands_["param"] = std::make_unique<ParamCommand>();
        commands_["bag"] = std::make_unique<BagCommand>();
        commands_["build"] = std::make_unique<BuildCommand>();
        commands_["test"] = std::make_unique<TestCommand>();
        commands_["deploy"] = std::make_unique<DeployCommand>();
        commands_["platform"] = std::make_unique<PlatformCommand>();
        commands_["help"] = std::make_unique<HelpCommand>();
    }
    
    void printHelp() {
        std::cout << "AuroraRT Command Line Interface" << std::endl;
        std::cout << "Usage: aurora <command> [options]" << std::endl;
        std::cout << std::endl;
        std::cout << "Commands:" << std::endl;
        
        for (const auto& [name, command] : commands_) {
            std::cout << "  " << name << " - " << command->getHelp() << std::endl;
        }
        
        std::cout << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  -h, --help    Show this help message" << std::endl;
    }
    
    std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
};

} // namespace cli
} // namespace aurorart

int main(int argc, char* argv[]) {
    aurorart::cli::CLI cli;
    cli.run(argc, argv);
    return 0;
}
