#ifndef AURORART_HARDWARE_ABSTRACTION_H
#define AURORART_HARDWARE_ABSTRACTION_H

#include <string>
#include <vector>
#include <memory>

namespace aurorart {
namespace platform {

// 网络接口信息
struct NetworkInterfaceInfo {
    std::string name;         // 接口名称
    std::string ip_address;   // IP地址
    std::string mac_address;  // MAC地址
    bool is_up;               // 接口是否启用
    bool has_tsn_support;     // 是否支持TSN
};

// CAN总线消息
struct CanMessage {
    uint32_t id;              // CAN ID
    uint8_t data[8];          // 数据
    uint8_t dlc;              // 数据长度
    bool is_extended;          // 是否为扩展ID
    bool is_fd;               // 是否为CAN FD
};

// TSN流配置
struct TSNStreamConfig {
    uint32_t stream_id;        // 流ID
    uint32_t priority;         // 优先级
    uint32_t bandwidth;        // 带宽（Kbps）
    uint32_t max_frame_size;   // 最大帧大小
    bool enable;               // 是否启用
};

class HardwareAbstraction {
public:
    virtual ~HardwareAbstraction() = default;
    
    // 网络接口管理
    virtual std::vector<NetworkInterfaceInfo> GetNetworkInterfaces() = 0;
    virtual bool SetNetworkInterfaceUp(const std::string& interface_name) = 0;
    virtual bool SetNetworkInterfaceDown(const std::string& interface_name) = 0;
    
    // CAN总线管理
    virtual bool OpenCanInterface(const std::string& interface_name, int bitrate) = 0;
    virtual bool CloseCanInterface(const std::string& interface_name) = 0;
    virtual bool SendCanMessage(const std::string& interface_name, const CanMessage& message) = 0;
    virtual bool ReceiveCanMessage(const std::string& interface_name, CanMessage& message, int timeout_ms = 1000) = 0;
    
    // TSN支持
    virtual bool HasTSNSupport(const std::string& interface_name) = 0;
    virtual bool EnableTSN(const std::string& interface_name) = 0;
    virtual bool ConfigureTSNStream(const std::string& interface_name, const TSNStreamConfig& config) = 0;
    virtual bool SetTSNTimeSync(const std::string& interface_name, bool enable) = 0;
    
    // 硬件中断管理
    virtual bool RegisterInterruptHandler(int interrupt_number, std::function<void()> handler) = 0;
    virtual bool UnregisterInterruptHandler(int interrupt_number) = 0;
};

class LinuxHardware : public HardwareAbstraction {
public:
    std::vector<NetworkInterfaceInfo> GetNetworkInterfaces() override;
    bool SetNetworkInterfaceUp(const std::string& interface_name) override;
    bool SetNetworkInterfaceDown(const std::string& interface_name) override;
    
    bool OpenCanInterface(const std::string& interface_name, int bitrate) override;
    bool CloseCanInterface(const std::string& interface_name) override;
    bool SendCanMessage(const std::string& interface_name, const CanMessage& message) override;
    bool ReceiveCanMessage(const std::string& interface_name, CanMessage& message, int timeout_ms = 1000) override;
    
    bool HasTSNSupport(const std::string& interface_name) override;
    bool EnableTSN(const std::string& interface_name) override;
    bool ConfigureTSNStream(const std::string& interface_name, const TSNStreamConfig& config) override;
    bool SetTSNTimeSync(const std::string& interface_name, bool enable) override;
    
    bool RegisterInterruptHandler(int interrupt_number, std::function<void()> handler) override;
    bool UnregisterInterruptHandler(int interrupt_number) override;
};

class WindowsHardware : public HardwareAbstraction {
public:
    std::vector<NetworkInterfaceInfo> GetNetworkInterfaces() override;
    bool SetNetworkInterfaceUp(const std::string& interface_name) override;
    bool SetNetworkInterfaceDown(const std::string& interface_name) override;
    
    bool OpenCanInterface(const std::string& interface_name, int bitrate) override;
    bool CloseCanInterface(const std::string& interface_name) override;
    bool SendCanMessage(const std::string& interface_name, const CanMessage& message) override;
    bool ReceiveCanMessage(const std::string& interface_name, CanMessage& message, int timeout_ms = 1000) override;
    
    bool HasTSNSupport(const std::string& interface_name) override;
    bool EnableTSN(const std::string& interface_name) override;
    bool ConfigureTSNStream(const std::string& interface_name, const TSNStreamConfig& config) override;
    bool SetTSNTimeSync(const std::string& interface_name, bool enable) override;
    
    bool RegisterInterruptHandler(int interrupt_number, std::function<void()> handler) override;
    bool UnregisterInterruptHandler(int interrupt_number) override;
};

class QNXHardware : public HardwareAbstraction {
public:
    std::vector<NetworkInterfaceInfo> GetNetworkInterfaces() override;
    bool SetNetworkInterfaceUp(const std::string& interface_name) override;
    bool SetNetworkInterfaceDown(const std::string& interface_name) override;
    
    bool OpenCanInterface(const std::string& interface_name, int bitrate) override;
    bool CloseCanInterface(const std::string& interface_name) override;
    bool SendCanMessage(const std::string& interface_name, const CanMessage& message) override;
    bool ReceiveCanMessage(const std::string& interface_name, CanMessage& message, int timeout_ms = 1000) override;
    
    bool HasTSNSupport(const std::string& interface_name) override;
    bool EnableTSN(const std::string& interface_name) override;
    bool ConfigureTSNStream(const std::string& interface_name, const TSNStreamConfig& config) override;
    bool SetTSNTimeSync(const std::string& interface_name, bool enable) override;
    
    bool RegisterInterruptHandler(int interrupt_number, std::function<void()> handler) override;
    bool UnregisterInterruptHandler(int interrupt_number) override;
};

class HardwareManager {
public:
    static HardwareManager& instance();
    void init();
    HardwareAbstraction* getHardware();
    
private:
    HardwareManager() = default;
    std::unique_ptr<HardwareAbstraction> hardware_;
};

} // namespace platform
} // namespace aurorart

#endif // AURORART_HARDWARE_ABSTRACTION_H
