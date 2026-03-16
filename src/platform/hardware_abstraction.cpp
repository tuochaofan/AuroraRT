#include "aurorart/platform/hardware_abstraction.h"
#include "aurorart/utils/logger.h"

#if defined(__linux__)
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#elif defined(__QNX__)
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <fstream>
#include <sstream>
#include <vector>
#include <functional>
#include <unordered_map>

namespace aurorart {
namespace platform {

// LinuxHardware implementation

std::vector<NetworkInterfaceInfo> LinuxHardware::GetNetworkInterfaces() {
    std::vector<NetworkInterfaceInfo> interfaces;
    
    // 打开/proc/net/dev文件获取网络接口信息
    std::ifstream dev_file("/proc/net/dev");
    if (!dev_file) {
        AURORA_LOG_ERROR("Failed to open /proc/net/dev");
        return interfaces;
    }
    
    std::string line;
    // 跳过前两行
    std::getline(dev_file, line);
    std::getline(dev_file, line);
    
    while (std::getline(dev_file, line)) {
        std::istringstream iss(line);
        std::string name;
        iss >> name;
        // 移除冒号
        if (!name.empty() && name.back() == ':') {
            name.pop_back();
        }
        
        NetworkInterfaceInfo info;
        info.name = name;
        
        // 获取接口状态
        struct ifreq ifr;
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd >= 0) {
            strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ);
            if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == 0) {
                info.is_up = (ifr.ifr_flags & IFF_UP) != 0;
            }
            
            // 获取IP地址
            struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
            if (ioctl(sockfd, SIOCGIFADDR, &ifr) == 0) {
                info.ip_address = inet_ntoa(addr->sin_addr);
            }
            
            // 获取MAC地址
            if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == 0) {
                unsigned char* mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;
                char mac_str[18];
                snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                info.mac_address = mac_str;
            }
            
            // 检查TSN支持
            std::string tsn_path = "/sys/class/net/" + name + "/tsn";
            struct stat stat_buf;
            info.has_tsn_support = (stat(tsn_path.c_str(), &stat_buf) == 0 && S_ISDIR(stat_buf.st_mode));
            
            close(sockfd);
        }
        
        interfaces.push_back(info);
    }
    
    return interfaces;
}

bool LinuxHardware::SetNetworkInterfaceUp(const std::string& interface_name) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        AURORA_LOG_ERROR("Failed to create socket: {}", strerror(errno));
        return false;
    }
    
    struct ifreq ifr;
    strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ);
    
    // 获取当前标志
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
        AURORA_LOG_ERROR("Failed to get interface flags: {}", strerror(errno));
        close(sockfd);
        return false;
    }
    
    // 设置UP标志
    ifr.ifr_flags |= IFF_UP;
    ifr.ifr_flags |= IFF_RUNNING;
    
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        AURORA_LOG_ERROR("Failed to set interface up: {}", strerror(errno));
        close(sockfd);
        return false;
    }
    
    close(sockfd);
    AURORA_LOG_INFO("Interface {} is now up", interface_name);
    return true;
}

bool LinuxHardware::SetNetworkInterfaceDown(const std::string& interface_name) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        AURORA_LOG_ERROR("Failed to create socket: {}", strerror(errno));
        return false;
    }
    
    struct ifreq ifr;
    strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ);
    
    // 获取当前标志
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
        AURORA_LOG_ERROR("Failed to get interface flags: {}", strerror(errno));
        close(sockfd);
        return false;
    }
    
    // 清除UP标志
    ifr.ifr_flags &= ~IFF_UP;
    
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        AURORA_LOG_ERROR("Failed to set interface down: {}", strerror(errno));
        close(sockfd);
        return false;
    }
    
    close(sockfd);
    AURORA_LOG_INFO("Interface {} is now down", interface_name);
    return true;
}

bool LinuxHardware::OpenCanInterface(const std::string& interface_name, int bitrate) {
    // 实现CAN接口打开
    AURORA_LOG_INFO("Opening CAN interface {} with bitrate {}", interface_name, bitrate);
    // 这里需要实现具体的CAN接口打开逻辑
    return true;
}

bool LinuxHardware::CloseCanInterface(const std::string& interface_name) {
    // 实现CAN接口关闭
    AURORA_LOG_INFO("Closing CAN interface {}", interface_name);
    // 这里需要实现具体的CAN接口关闭逻辑
    return true;
}

bool LinuxHardware::SendCanMessage(const std::string& interface_name, const CanMessage& message) {
    // 实现CAN消息发送
    AURORA_LOG_DEBUG("Sending CAN message on interface {}", interface_name);
    // 这里需要实现具体的CAN消息发送逻辑
    return true;
}

bool LinuxHardware::ReceiveCanMessage(const std::string& interface_name, CanMessage& message, int timeout_ms) {
    // 实现CAN消息接收
    AURORA_LOG_DEBUG("Receiving CAN message on interface {}", interface_name);
    // 这里需要实现具体的CAN消息接收逻辑
    return true;
}

bool LinuxHardware::HasTSNSupport(const std::string& interface_name) {
    std::string tsn_path = "/sys/class/net/" + interface_name + "/tsn";
    struct stat stat_buf;
    return (stat(tsn_path.c_str(), &stat_buf) == 0 && S_ISDIR(stat_buf.st_mode));
}

bool LinuxHardware::EnableTSN(const std::string& interface_name) {
    // 实现TSN启用
    AURORA_LOG_INFO("Enabling TSN on interface {}", interface_name);
    // 这里需要实现具体的TSN启用逻辑
    return true;
}

bool LinuxHardware::ConfigureTSNStream(const std::string& interface_name, const TSNStreamConfig& config) {
    // 实现TSN流配置
    AURORA_LOG_INFO("Configuring TSN stream on interface {}", interface_name);
    // 这里需要实现具体的TSN流配置逻辑
    return true;
}

bool LinuxHardware::SetTSNTimeSync(const std::string& interface_name, bool enable) {
    // 实现TSN时间同步设置
    AURORA_LOG_INFO("Setting TSN time sync on interface {} to {}", interface_name, enable);
    // 这里需要实现具体的TSN时间同步设置逻辑
    return true;
}

bool LinuxHardware::RegisterInterruptHandler(int interrupt_number, std::function<void()> handler) {
    // 实现中断处理程序注册
    AURORA_LOG_INFO("Registering interrupt handler for interrupt {}", interrupt_number);
    // 这里需要实现具体的中断处理程序注册逻辑
    return true;
}

bool LinuxHardware::UnregisterInterruptHandler(int interrupt_number) {
    // 实现中断处理程序注销
    AURORA_LOG_INFO("Unregistering interrupt handler for interrupt {}", interrupt_number);
    // 这里需要实现具体的中断处理程序注销逻辑
    return true;
}

// WindowsHardware implementation

std::vector<NetworkInterfaceInfo> WindowsHardware::GetNetworkInterfaces() {
    std::vector<NetworkInterfaceInfo> interfaces;
    
    // 使用Windows API获取网络接口信息
    ULONG bufSize = 0;
    GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, NULL, NULL, &bufSize);
    
    PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufSize);
    if (!pAddresses) {
        AURORA_LOG_ERROR("Failed to allocate memory for adapter addresses");
        return interfaces;
    }
    
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, NULL, pAddresses, &bufSize) == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES pCurrAddress = pAddresses;
        while (pCurrAddress) {
            NetworkInterfaceInfo info;
            info.name = pCurrAddress->AdapterName;
            info.is_up = (pCurrAddress->OperStatus == IfOperStatusUp);
            
            // 获取MAC地址
            char mac_str[18];
            for (ULONG i = 0; i < pCurrAddress->PhysicalAddressLength; i++) {
                if (i == 0) {
                    sprintf_s(mac_str, sizeof(mac_str), "%02X", (int)pCurrAddress->PhysicalAddress[i]);
                } else {
                    sprintf_s(mac_str + strlen(mac_str), sizeof(mac_str) - strlen(mac_str), ":%02X", (int)pCurrAddress->PhysicalAddress[i]);
                }
            }
            info.mac_address = mac_str;
            
            // 获取IP地址
            PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddress->FirstUnicastAddress;
            if (pUnicast) {
                char ip_str[INET6_ADDRSTRLEN];
                sockaddr_in* addr = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                inet_ntop(AF_INET, &(addr->sin_addr), ip_str, INET_ADDRSTRLEN);
                info.ip_address = ip_str;
            }
            
            // 检查TSN支持（Windows不原生支持TSN，这里返回false）
            info.has_tsn_support = false;
            
            interfaces.push_back(info);
            pCurrAddress = pCurrAddress->Next;
        }
    } else {
        AURORA_LOG_ERROR("Failed to get adapter addresses");
    }
    
    free(pAddresses);
    return interfaces;
}

bool WindowsHardware::SetNetworkInterfaceUp(const std::string& interface_name) {
    // 实现Windows网络接口启用
    AURORA_LOG_INFO("Enabling network interface {}", interface_name);
    // 这里需要实现具体的Windows网络接口启用逻辑
    return true;
}

bool WindowsHardware::SetNetworkInterfaceDown(const std::string& interface_name) {
    // 实现Windows网络接口禁用
    AURORA_LOG_INFO("Disabling network interface {}", interface_name);
    // 这里需要实现具体的Windows网络接口禁用逻辑
    return true;
}

bool WindowsHardware::OpenCanInterface(const std::string& interface_name, int bitrate) {
    // 实现Windows CAN接口打开
    AURORA_LOG_INFO("Opening CAN interface {} with bitrate {}", interface_name, bitrate);
    // 这里需要实现具体的Windows CAN接口打开逻辑
    return true;
}

bool WindowsHardware::CloseCanInterface(const std::string& interface_name) {
    // 实现Windows CAN接口关闭
    AURORA_LOG_INFO("Closing CAN interface {}", interface_name);
    // 这里需要实现具体的Windows CAN接口关闭逻辑
    return true;
}

bool WindowsHardware::SendCanMessage(const std::string& interface_name, const CanMessage& message) {
    // 实现Windows CAN消息发送
    AURORA_LOG_DEBUG("Sending CAN message on interface {}", interface_name);
    // 这里需要实现具体的Windows CAN消息发送逻辑
    return true;
}

bool WindowsHardware::ReceiveCanMessage(const std::string& interface_name, CanMessage& message, int timeout_ms) {
    // 实现Windows CAN消息接收
    AURORA_LOG_DEBUG("Receiving CAN message on interface {}", interface_name);
    // 这里需要实现具体的Windows CAN消息接收逻辑
    return true;
}

bool WindowsHardware::HasTSNSupport(const std::string& interface_name) {
    // Windows不原生支持TSN
    return false;
}

bool WindowsHardware::EnableTSN(const std::string& interface_name) {
    // Windows不原生支持TSN
    AURORA_LOG_WARN("TSN is not supported on Windows");
    return false;
}

bool WindowsHardware::ConfigureTSNStream(const std::string& interface_name, const TSNStreamConfig& config) {
    // Windows不原生支持TSN
    AURORA_LOG_WARN("TSN is not supported on Windows");
    return false;
}

bool WindowsHardware::SetTSNTimeSync(const std::string& interface_name, bool enable) {
    // Windows不原生支持TSN
    AURORA_LOG_WARN("TSN is not supported on Windows");
    return false;
}

bool WindowsHardware::RegisterInterruptHandler(int interrupt_number, std::function<void()> handler) {
    // 实现Windows中断处理程序注册
    AURORA_LOG_INFO("Registering interrupt handler for interrupt {}", interrupt_number);
    // 这里需要实现具体的Windows中断处理程序注册逻辑
    return true;
}

bool WindowsHardware::UnregisterInterruptHandler(int interrupt_number) {
    // 实现Windows中断处理程序注销
    AURORA_LOG_INFO("Unregistering interrupt handler for interrupt {}", interrupt_number);
    // 这里需要实现具体的Windows中断处理程序注销逻辑
    return true;
}

// QNXHardware implementation

std::vector<NetworkInterfaceInfo> QNXHardware::GetNetworkInterfaces() {
    std::vector<NetworkInterfaceInfo> interfaces;
    
    // 实现QNX网络接口获取
    AURORA_LOG_INFO("Getting network interfaces on QNX");
    // 这里需要实现具体的QNX网络接口获取逻辑
    return interfaces;
}

bool QNXHardware::SetNetworkInterfaceUp(const std::string& interface_name) {
    // 实现QNX网络接口启用
    AURORA_LOG_INFO("Enabling network interface {}", interface_name);
    // 这里需要实现具体的QNX网络接口启用逻辑
    return true;
}

bool QNXHardware::SetNetworkInterfaceDown(const std::string& interface_name) {
    // 实现QNX网络接口禁用
    AURORA_LOG_INFO("Disabling network interface {}", interface_name);
    // 这里需要实现具体的QNX网络接口禁用逻辑
    return true;
}

bool QNXHardware::OpenCanInterface(const std::string& interface_name, int bitrate) {
    // 实现QNX CAN接口打开
    AURORA_LOG_INFO("Opening CAN interface {} with bitrate {}", interface_name, bitrate);
    // 这里需要实现具体的QNX CAN接口打开逻辑
    return true;
}

bool QNXHardware::CloseCanInterface(const std::string& interface_name) {
    // 实现QNX CAN接口关闭
    AURORA_LOG_INFO("Closing CAN interface {}", interface_name);
    // 这里需要实现具体的QNX CAN接口关闭逻辑
    return true;
}

bool QNXHardware::SendCanMessage(const std::string& interface_name, const CanMessage& message) {
    // 实现QNX CAN消息发送
    AURORA_LOG_DEBUG("Sending CAN message on interface {}", interface_name);
    // 这里需要实现具体的QNX CAN消息发送逻辑
    return true;
}

bool QNXHardware::ReceiveCanMessage(const std::string& interface_name, CanMessage& message, int timeout_ms) {
    // 实现QNX CAN消息接收
    AURORA_LOG_DEBUG("Receiving CAN message on interface {}", interface_name);
    // 这里需要实现具体的QNX CAN消息接收逻辑
    return true;
}

bool QNXHardware::HasTSNSupport(const std::string& interface_name) {
    // QNX支持TSN
    return true;
}

bool QNXHardware::EnableTSN(const std::string& interface_name) {
    // 实现QNX TSN启用
    AURORA_LOG_INFO("Enabling TSN on interface {}", interface_name);
    // 这里需要实现具体的QNX TSN启用逻辑
    return true;
}

bool QNXHardware::ConfigureTSNStream(const std::string& interface_name, const TSNStreamConfig& config) {
    // 实现QNX TSN流配置
    AURORA_LOG_INFO("Configuring TSN stream on interface {}", interface_name);
    // 这里需要实现具体的QNX TSN流配置逻辑
    return true;
}

bool QNXHardware::SetTSNTimeSync(const std::string& interface_name, bool enable) {
    // 实现QNX TSN时间同步设置
    AURORA_LOG_INFO("Setting TSN time sync on interface {} to {}", interface_name, enable);
    // 这里需要实现具体的QNX TSN时间同步设置逻辑
    return true;
}

bool QNXHardware::RegisterInterruptHandler(int interrupt_number, std::function<void()> handler) {
    // 实现QNX中断处理程序注册
    AURORA_LOG_INFO("Registering interrupt handler for interrupt {}", interrupt_number);
    // 这里需要实现具体的QNX中断处理程序注册逻辑
    return true;
}

bool QNXHardware::UnregisterInterruptHandler(int interrupt_number) {
    // 实现QNX中断处理程序注销
    AURORA_LOG_INFO("Unregistering interrupt handler for interrupt {}", interrupt_number);
    // 这里需要实现具体的QNX中断处理程序注销逻辑
    return true;
}

// HardwareManager implementation

HardwareManager& HardwareManager::instance() {
    static HardwareManager instance;
    return instance;
}

void HardwareManager::init() {
#if defined(__QNX__)
    hardware_ = std::make_unique<QNXHardware>();
#elif defined(__linux__)
    hardware_ = std::make_unique<LinuxHardware>();
#elif defined(_WIN32)
    hardware_ = std::make_unique<WindowsHardware>();
#else
    #error "Unsupported platform"
#endif
    AURORA_LOG_INFO("Hardware abstraction initialized for current platform");
}

HardwareAbstraction* HardwareManager::getHardware() {
    return hardware_.get();
}

} // namespace platform
} // namespace aurorart
