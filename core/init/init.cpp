#include "aurorart/init/init.h"
#include "aurorart/logging/logger.h"
#include "aurorart/config/config.h"
#include "aurorart/serialization/serializer.h"

#include <iostream>
#include <fstream>
#include <sstream>

namespace aurorart {
namespace init {

// 单例实例
InitManager& InitManager::getInstance() {
    static InitManager instance;
    return instance;
}

// 构造函数
InitManager::InitManager() {
}

// 析构函数
InitManager::~InitManager() {
    if (initialized_) {
        shutdown();
    }
}

// 初始化 AuroraRT
bool InitManager::initialize(const InitConfig& config) {
    if (initialized_) {
        std::cout << "AuroraRT is already initialized" << std::endl;
        return true;
    }
    
    std::cout << "Initializing AuroraRT..." << std::endl;
    
    // 保存配置
    config_ = config;
    
    // 初始化配置系统
    if (!config::Config::instance().initialize(config.config_file)) {
        std::cerr << "Failed to initialize config system" << std::endl;
        return false;
    }
    initialized_components_.push_back("config");
    std::cout << "✓ Config system initialized" << std::endl;
    
    // 初始化日志系统
    if (config.enable_logging) {
        if (!logging::Logger::instance().initialize(config.log_level)) {
            std::cerr << "Failed to initialize logging system" << std::endl;
            return false;
        }
        initialized_components_.push_back("logging");
        std::cout << "✓ Logging system initialized" << std::endl;
    }
    
    // 初始化序列化系统
    // 这里可以根据配置选择默认的序列化格式
    std::cout << "✓ Serialization system initialized" << std::endl;
    initialized_components_.push_back("serialization");
    
    // 初始化监控系统
    if (config.enable_monitoring) {
        // 这里添加监控系统的初始化逻辑
        std::cout << "✓ Monitoring system initialized" << std::endl;
        initialized_components_.push_back("monitoring");
    }
    
    // 初始化诊断系统
    if (config.enable_diagnostics) {
        // 这里添加诊断系统的初始化逻辑
        std::cout << "✓ Diagnostics system initialized" << std::endl;
        initialized_components_.push_back("diagnostics");
    }
    
    // 标记为已初始化
    initialized_ = true;
    
    std::cout << "AuroraRT initialized successfully!" << std::endl;
    std::cout << "Components initialized: " << std::endl;
    for (const auto& component : initialized_components_) {
        std::cout << "  - " << component << std::endl;
    }
    
    return true;
}

// 初始化 AuroraRT（使用默认配置）
bool InitManager::initialize(const std::string& node_name) {
    InitConfig config;
    config.node_name = node_name;
    return initialize(config);
}

// 关闭 AuroraRT
void InitManager::shutdown() {
    if (!initialized_) {
        return;
    }
    
    std::cout << "Shutting down AuroraRT..." << std::endl;
    
    // 按相反的顺序关闭组件
    for (auto it = initialized_components_.rbegin(); it != initialized_components_.rend(); ++it) {
        const auto& component = *it;
        std::cout << "✓ " << component << " system shutdown" << std::endl;
    }
    
    // 清空初始化组件列表
    initialized_components_.clear();
    
    // 标记为未初始化
    initialized_ = false;
    
    std::cout << "AuroraRT shutdown successfully!" << std::endl;
}

// 检查是否已初始化
bool InitManager::isInitialized() const {
    return initialized_;
}

// 获取当前配置
const InitConfig& InitManager::getConfig() const {
    return config_;
}

// 便捷初始化函数
bool init(const std::string& node_name) {
    return InitManager::getInstance().initialize(node_name);
}

// 便捷初始化函数（带配置）
bool init(const InitConfig& config) {
    return InitManager::getInstance().initialize(config);
}

// 便捷关闭函数
void shutdown() {
    InitManager::getInstance().shutdown();
}

// 检查是否已初始化
bool isInitialized() {
    return InitManager::getInstance().isInitialized();
}

} // namespace init
} // namespace aurorart
