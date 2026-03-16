#pragma once

#include <string>
#include <vector>
#include <memory>

namespace aurorart {
namespace init {

/**
 * @brief AuroraRT 初始化配置
 */
struct InitConfig {
    // 节点名称
    std::string node_name;
    
    // 命名空间
    std::string namespace_;
    
    // 配置文件路径
    std::string config_file;
    
    // 是否启用日志
    bool enable_logging = true;
    
    // 日志级别
    std::string log_level = "info";
    
    // 是否启用监控
    bool enable_monitoring = false;
    
    // 是否启用诊断
    bool enable_diagnostics = false;
    
    // 序列化格式
    std::string serialization_format = "cdr";
};

/**
 * @brief AuroraRT 初始化管理器
 * 
 * 提供统一的初始化接口，简化工具的初始化过程
 */
class InitManager {
public:
    /**
     * @brief 获取单例实例
     */
    static InitManager& getInstance();
    
    /**
     * @brief 初始化 AuroraRT
     * 
     * @param config 初始化配置
     * @return 是否初始化成功
     */
    bool initialize(const InitConfig& config);
    
    /**
     * @brief 初始化 AuroraRT（使用默认配置）
     * 
     * @param node_name 节点名称
     * @return 是否初始化成功
     */
    bool initialize(const std::string& node_name);
    
    /**
     * @brief 关闭 AuroraRT
     */
    void shutdown();
    
    /**
     * @brief 检查是否已初始化
     * 
     * @return 是否已初始化
     */
    bool isInitialized() const;
    
    /**
     * @brief 获取当前配置
     * 
     * @return 当前配置
     */
    const InitConfig& getConfig() const;
    
private:
    // 私有构造函数
    InitManager();
    
    // 私有析构函数
    ~InitManager();
    
    // 禁用拷贝构造函数和赋值运算符
    InitManager(const InitManager&) = delete;
    InitManager& operator=(const InitManager&) = delete;
    
    // 初始化状态
    bool initialized_ = false;
    
    // 配置
    InitConfig config_;
    
    // 初始化组件列表
    std::vector<std::string> initialized_components_;
};

/**
 * @brief 便捷初始化函数
 * 
 * @param node_name 节点名称
 * @return 是否初始化成功
 */
bool init(const std::string& node_name);

/**
 * @brief 便捷初始化函数（带配置）
 * 
 * @param config 初始化配置
 * @return 是否初始化成功
 */
bool init(const InitConfig& config);

/**
 * @brief 便捷关闭函数
 */
void shutdown();

/**
 * @brief 检查是否已初始化
 * 
 * @return 是否已初始化
 */
bool isInitialized();

} // namespace init
} // namespace aurorart
