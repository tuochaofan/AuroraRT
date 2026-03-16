# 接口定义

## 1. 文档概述

本文档详细说明了AuroraRT车载实时性消息中间件的接口定义，包括业务接入层、统一API层、核心调度层、通信核心层等各层级的接口定义、参数说明、返回值和使用示例。通过本文档，开发者可以了解如何使用AuroraRT的接口进行应用开发，为系统的集成和使用提供指导。

## 2. 业务接入层接口

### 2.1 Publisher接口

```cpp
class Publisher {
public:
    // 创建Publisher实例
    static Publisher* create(const std::string& topic, const PublisherOptions& options);
    
    // 发布消息
    template <typename T>
    bool publish(const T& message);
    
    // 发布消息（带超时）
    template <typename T>
    bool publish(const T& message, std::chrono::milliseconds timeout);
    
    // 关闭Publisher
    void close();
    
    // 获取状态
    PublisherState getState() const;
};
```

**参数说明**
- `topic`：消息主题
- `options`：发布者选项，包括QoS参数、可靠性级别等
- `message`：要发布的消息
- `timeout`：发布超时

**返回值：**
- `publish`：成功返回true，失败返回false
- `getState`：返回发布者当前状态

### 2.2 Subscriber接口

```cpp
class Subscriber {
public:
    // 创建Subscriber实例
    static Subscriber* create(const std::string& topic, const SubscriberOptions& options);
    
    // 设置消息回调函数
    template <typename T>
    void setCallback(std::function<void(const T&)> callback);
    
    // 订阅消息
    bool subscribe();
    
    // 取消订阅
    bool unsubscribe();
    
    // 关闭Subscriber
    void close();
    
    // 获取状态
    SubscriberState getState() const;
};
```

**参数说明**
- `topic`：消息主题
- `options`：订阅者选项，包括QoS参数、队列大小等
- `callback`：消息回调函数

**返回值：**
- `subscribe`：成功返回true，失败返回false
- `unsubscribe`：成功返回true，失败返回false
- `getState`：返回订阅者当前状态

### 2.3 Request接口

```cpp
class Request {
public:
    // 创建Request实例
    static Request* create(const std::string& service, const RequestOptions& options);
    
    // 发送请求
    template <typename Req, typename Resp>
    bool send(const Req& request, Resp& response);
    
    // 发送请求（带超时）
    template <typename Req, typename Resp>
    bool send(const Req& request, Resp& response, std::chrono::milliseconds timeout);
    
    // 关闭Request
    void close();
    
    // 获取状态
    RequestState getState() const;
};
```

**参数说明**
- `service`：服务名
- `options`：请求选项，包括QoS参数、超时时间等
- `request`：请求消息
- `response`：响应消息
- `timeout`：请求超时

**返回值：**
- `send`：成功返回true，失败返回false
- `getState`：返回请求方当前状态

### 2.4 Response接口

```cpp
class Response {
public:
    // 创建Response实例
    static Response* create(const std::string& service, const ResponseOptions& options);
    
    // 设置响应回调函数
    template <typename Req, typename Resp>
    void setCallback(std::function<void(const Req&, Resp&)> callback);
    
    // 启动响应服务
    bool start();
    
    // 停止响应服务
    bool stop();
    
    // 关闭Response
    void close();
    
    // 获取状态
    ResponseState getState() const;
};
```

**参数说明**
- `service`：服务名
- `options`：响应选项，包括QoS参数、线程池大小
- `callback`：响应回调函数

**返回值：**
- `start`：成功返回true，失败返回false
- `stop`：成功返回true，失败返回false
- `getState`：返回响应方当前状态

### 2.5 Service接口

```cpp
class Service {
public:
    // 创建Service实例
    static Service* create(const std::string& name, const ServiceOptions& options);
    
    // 注册服务方法
    template <typename Req, typename Resp>
    bool registerMethod(const std::string& method, std::function<void(const Req&, Resp&)> callback);
    
    // 启动服务
    bool start();
    
    // 停止服务
    bool stop();
    
    // 关闭Service
    void close();
    
    // 获取状态
    ServiceState getState() const;
};
```

**参数说明**
- `name`：服务名
- `options`：服务选项，包括QoS参数、线程池大小
- `method`：方法名
- `callback`：方法回调函数

**返回值：**
- `registerMethod`：成功返回true，失败返回false
- `start`：成功返回true，失败返回false
- `stop`：成功返回true，失败返回false
- `getState`：返回服务当前状态

### 2.6 Client接口

```cpp
class Client {
public:
    // 创建Client实例
    static Client* create(const std::string& service, const ClientOptions& options);
    
    // 调用服务方法
    template <typename Req, typename Resp>
    bool call(const std::string& method, const Req& request, Resp& response);
    
    // 调用服务方法（带超时）
    template <typename Req, typename Resp>
    bool call(const std::string& method, const Req& request, Resp& response, std::chrono::milliseconds timeout);
    
    // 关闭Client
    void close();
    
    // 获取状态
    ClientState getState() const;
};
```

**参数说明**
- `service`：服务名
- `options`：客户端选项，包括QoS参数、超时时间等
- `method`：方法名
- `request`：请求消息
- `response`：响应消息
- `timeout`：调用超时

**返回值：**
- `call`：成功返回true，失败返回false
- `getState`：返回客户端当前状态

## 3. 统一API层接口

### 3.1 Socket接口

```cpp
class Socket {
public:
    // 创建Socket实例
    static Socket* create(SocketType type, const SocketOptions& options);
    
    // 连接到目标地址
    bool connect(const std::string& address, uint16_t port);
    
    // 绑定到本地地址
    bool bind(const std::string& address, uint16_t port);
    
    // 发送数据
    size_t send(const void* data, size_t length);
    
    // 接收数据
    size_t recv(void* buffer, size_t length);
    
    // 发送数据（带超时）
    size_t send(const void* data, size_t length, std::chrono::milliseconds timeout);
    
    // 接收数据（带超时）
    size_t recv(void* buffer, size_t length, std::chrono::milliseconds timeout);
    
    // 关闭Socket
    void close();
    
    // 获取状态
    SocketState getState() const;
};
```

**参数说明**
- `type`：套接字类型（TCP/UDP/QUIC等）
- `options`：套接字选项
- `address`：目标地址
- `port`：端口号
- `data`：要发送的数据
- `length`：数据长度
- `buffer`：接收缓冲区
- `timeout`：超时

**返回值：**
- `connect`：成功返回true，失败返回false
- `bind`：成功返回true，失败返回false
- `send`：返回发送的字节数，失败返回-1
- `recv`：返回接收的字节数，失败返回-1
- `getState`：返回套接字当前状态

### 3.2 PublisherImpl接口

```cpp
class PublisherImpl {
public:
    // 初始化Publisher
    bool init(const std::string& topic, const PublisherOptions& options);
    
    // 发布消息
    bool publish(const void* data, size_t length);
    
    // 发布消息（带超时）
    bool publish(const void* data, size_t length, std::chrono::milliseconds timeout);
    
    // 关闭Publisher
    void close();
    
    // 获取状态
    PublisherState getState() const;
};
```

**参数说明**
- `topic`：消息主题
- `options`：发布者选项
- `data`：消息数据
- `length`：数据长度
- `timeout`：超时

**返回值：**
- `init`：成功返回true，失败返回false
- `publish`：成功返回true，失败返回false
- `getState`：返回发布者当前状态

### 3.3 SubscriberImpl接口

```cpp
class SubscriberImpl {
public:
    // 初始化Subscriber
    bool init(const std::string& topic, const SubscriberOptions& options);
    
    // 设置消息回调函数
    void setCallback(std::function<void(const void*, size_t)> callback);
    
    // 订阅消息
    bool subscribe();
    
    // 取消订阅
    bool unsubscribe();
    
    // 关闭Subscriber
    void close();
    
    // 获取状态
    SubscriberState getState() const;
};
```

**参数说明**
- `topic`：消息主题
- `options`：订阅者选项
- `callback`：消息回调函数

**返回值：**
- `init`：成功返回true，失败返回false
- `subscribe`：成功返回true，失败返回false
- `unsubscribe`：成功返回true，失败返回false
- `getState`：返回订阅者当前状态

### 3.4 RequestImpl接口

```cpp
class RequestImpl {
public:
    // 初始化Request
    bool init(const std::string& service, const RequestOptions& options);
    
    // 发送请求
    bool send(const void* request, size_t requestLength, void* response, size_t responseLength);
    
    // 发送请求（带超时）
    bool send(const void* request, size_t requestLength, void* response, size_t responseLength, std::chrono::milliseconds timeout);
    
    // 关闭Request
    void close();
    
    // 获取状态
    RequestState getState() const;
};
```

**参数说明**
- `service`：服务名
- `options`：请求选项
- `request`：请求数据
- `requestLength`：请求数据长度
- `response`：响应缓冲区
- `responseLength`：响应缓冲区长度
- `timeout`：超时

**返回值：**
- `init`：成功返回true，失败返回false
- `send`：成功返回true，失败返回false
- `getState`：返回请求方当前状态

### 3.5 ResponseImpl接口

```cpp
class ResponseImpl {
public:
    // 初始化Response
    bool init(const std::string& service, const ResponseOptions& options);
    
    // 设置响应回调函数
    void setCallback(std::function<void(const void*, size_t, void*, size_t)> callback);
    
    // 启动响应服务
    bool start();
    
    // 停止响应服务
    bool stop();
    
    // 关闭Response
    void close();
    
    // 获取状态
    ResponseState getState() const;
};
```

**参数说明**
- `service`：服务名
- `options`：响应选项
- `callback`：响应回调函数

**返回值：**
- `init`：成功返回true，失败返回false
- `start`：成功返回true，失败返回false
- `stop`：成功返回true，失败返回false
- `getState`：返回响应方当前状态

## 4. 核心调度层接口

### 4.1 ProtocolScheduler接口

```cpp
class ProtocolScheduler {
public:
    // 获取ProtocolScheduler实例
    static ProtocolScheduler* getInstance();
    
    // 选择协议
    ProtocolType selectProtocol(const MessageProperties& properties, const NetworkState& networkState);
    
    // 注册协议
    bool registerProtocol(ProtocolType type, Protocol* protocol);
    
    // 卸载协议
    bool unregisterProtocol(ProtocolType type);
    
    // 获取协议状态
    ProtocolState getProtocolState(ProtocolType type) const;
};
```

**参数说明**
- `properties`：消息属性，包括大小、优先级
- `networkState`：网络状态，包括带宽、延迟等
- `type`：协议类型
- `protocol`：协议实例

**返回值：**
- `selectProtocol`：返回选择的协议类型
- `registerProtocol`：成功返回true，失败返回false
- `unregisterProtocol`：成功返回true，失败返回false
- `getProtocolState`：返回协议当前状态

### 4.2 NodeScheduler接口

```cpp
class NodeScheduler {
public:
    // 获取NodeScheduler实例
    static NodeScheduler* getInstance();
    
    // 注册节点
    bool registerNode(const NodeInfo& nodeInfo);
    
    // 注销节点
    bool unregisterNode(const std::string& nodeId);
    
    // 获取节点信息
    NodeInfo getNodeInfo(const std::string& nodeId) const;
    
    // 获取所有节点
    std::vector<NodeInfo> getAllNodes() const;
    
    // 触发节点热重启
    bool triggerHotRestart(const std::string& nodeId);
};
```

**参数说明**
- `nodeInfo`：节点信息
- `nodeId`：节点ID

**返回值：**
- `registerNode`：成功返回true，失败返回false
- `unregisterNode`：成功返回true，失败返回false
- `getNodeInfo`：返回节点信息
- `getAllNodes`：返回所有节点信息
- `triggerHotRestart`：成功返回true，失败返回false

### 4.3 QoSManager接口

```cpp
class QoSManager {
public:
    // 获取QoSManager实例
    static QoSManager* getInstance();
    
    // 设置QoS策略
    bool setQoSPolicy(const std::string& topic, const QoSPolicy& policy);
    
    // 获取QoS策略
    QoSPolicy getQoSPolicy(const std::string& topic) const;
    
    // 检查QoS合规性
    bool checkQoSCompliance(const std::string& topic, const MessageProperties& properties) const;
    
    // 调整流量控制参数
    bool adjustFlowControl(const std::string& topic, const FlowControlParams& params);
};
```

**参数说明**
- `topic`：消息主题
- `policy`：QoS策略
- `properties`：消息属性
- `params`：流量控制参数

**返回值：**
- `setQoSPolicy`：成功返回true，失败返回false
- `getQoSPolicy`：返回QoS策略
- `checkQoSCompliance`：合规返回true，不合规返回false
- `adjustFlowControl`：成功返回true，失败返回false

## 5. 通信核心层接口

### 5.1 InprocCom接口

```cpp
class InprocCom {
public:
    // 创建InprocCom实例
    static InprocCom* create(const InprocOptions& options);
    
    // 发送消息
    bool send(const void* data, size_t length);
    
    // 接收消息
    bool recv(void* buffer, size_t length, size_t& received);
    
    // 发送消息（带超时）
    bool send(const void* data, size_t length, std::chrono::milliseconds timeout);
    
    // 接收消息（带超时）
    bool recv(void* buffer, size_t length, size_t& received, std::chrono::milliseconds timeout);
    
    // 关闭通信
    void close();
};
```

**参数说明**
- `options`：进程内通信选项
- `data`：要发送的数据
- `length`：数据长度
- `buffer`：接收缓冲区
- `received`：实际接收的字节数
- `timeout`：超时

**返回值：**
- `send`：成功返回true，失败返回false
- `recv`：成功返回true，失败返回false

### 5.2 IpcCom接口

```cpp
class IpcCom {
public:
    // 创建IpcCom实例
    static IpcCom* create(const std::string& channel, const IpcOptions& options);
    
    // 发送消息
    bool send(const void* data, size_t length);
    
    // 接收消息
    bool recv(void* buffer, size_t length, size_t& received);
    
    // 发送消息（带超时）
    bool send(const void* data, size_t length, std::chrono::milliseconds timeout);
    
    // 接收消息（带超时）
    bool recv(void* buffer, size_t length, size_t& received, std::chrono::milliseconds timeout);
    
    // 关闭通信
    void close();
};
```

**参数说明**
- `channel`：IPC通道名称
- `options`：IPC通信选项
- `data`：要发送的数据
- `length`：数据长度
- `buffer`：接收缓冲区
- `received`：实际接收的字节数
- `timeout`：超时

**返回值：**
- `send`：成功返回true，失败返回false
- `recv`：成功返回true，失败返回false

### 5.3 TcpCom接口

```cpp
class TcpCom {
public:
    // 创建TcpCom实例
    static TcpCom* create(const TcpOptions& options);
    
    // 连接到目标地址
    bool connect(const std::string& address, uint16_t port);
    
    // 绑定到本地地址
    bool bind(const std::string& address, uint16_t port);
    
    // 监听连接
    bool listen(int backlog);
    
    // 接受连接
    TcpCom* accept();
    
    // 发送数据
    size_t send(const void* data, size_t length);
    
    // 接收数据
    size_t recv(void* buffer, size_t length);
    
    // 关闭连接
    void close();
};
```

**参数说明**
- `options`：TCP通信选项
- `address`：目标地址
- `port`：端口号
- `backlog`：监听队列大小
- `data`：要发送的数据
- `length`：数据长度
- `buffer`：接收缓冲区

**返回值：**
- `connect`：成功返回true，失败返回false
- `bind`：成功返回true，失败返回false
- `listen`：成功返回true，失败返回false
- `accept`：成功返回新的TcpCom实例，失败返回nullptr
- `send`：返回发送的字节数，失败返回-1
- `recv`：返回接收的字节数，失败返回-1

### 5.4 UdpCom接口

```cpp
class UdpCom {
public:
    // 创建UdpCom实例
    static UdpCom* create(const UdpOptions& options);
    
    // 绑定到本地地址
    bool bind(const std::string& address, uint16_t port);
    
    // 发送数据
    size_t send(const void* data, size_t length, const std::string& address, uint16_t port);
    
    // 接收数据
    size_t recv(void* buffer, size_t length, std::string& address, uint16_t& port);
    
    // 加入组播
    bool joinMulticastGroup(const std::string& groupAddress);
    
    // 离开组播
    bool leaveMulticastGroup(const std::string& groupAddress);
    
    // 关闭通信
    void close();
};
```

**参数说明**
- `options`：UDP通信选项
- `address`：目标地址或源地址
- `port`：端口号
- `data`：要发送的数据
- `length`：数据长度
- `buffer`：接收缓冲区
- `groupAddress`：组播组地址

**返回值：**
- `bind`：成功返回true，失败返回false
- `send`：返回发送的字节数，失败返回-1
- `recv`：返回接收的字节数，失败返回-1
- `joinMulticastGroup`：成功返回true，失败返回false
- `leaveMulticastGroup`：成功返回true，失败返回false

### 5.5 QuicCom接口

```cpp
class QuicCom {
public:
    // 创建QuicCom实例
    static QuicCom* create(const QuicOptions& options);
    
    // 连接到目标地址
    bool connect(const std::string& address, uint16_t port);
    
    // 绑定到本地地址
    bool bind(const std::string& address, uint16_t port);
    
    // 监听连接
    bool listen(int backlog);
    
    // 接受连接
    QuicCom* accept();
    
    // 发送数据
    size_t send(const void* data, size_t length);
    
    // 接收数据
    size_t recv(void* buffer, size_t length);
    
    // 关闭连接
    void close();
};
```

**参数说明**
- `options`：QUIC通信选项
- `address`：目标地址
- `port`：端口号
- `backlog`：监听队列大小
- `data`：要发送的数据
- `length`：数据长度
- `buffer`：接收缓冲区

**返回值：**
- `connect`：成功返回true，失败返回false
- `bind`：成功返回true，失败返回false
- `listen`：成功返回true，失败返回false
- `accept`：成功返回新的QuicCom实例，失败返回nullptr
- `send`：返回发送的字节数，失败返回-1
- `recv`：返回接收的字节数，失败返回-1

## 6. 基础抽象层接口

### 6.1 PlatformThread接口

```cpp
class PlatformThread {
public:
    // 创建线程
    static PlatformThread* create(std::function<void()> func, const ThreadOptions& options);
    
    // 启动线程
    bool start();
    
    // 等待线程结束
    bool join();
    
    // 分离线程
    bool detach();
    
    // 获取线程ID
    uint64_t getId() const;
    
    // 设置线程优先级
    bool setPriority(int priority);
    
    // 设置CPU亲和性
    bool setCpuAffinity(int cpuId);
};
```

**参数说明**
- `func`：线程函数
- `options`：线程选项，包括优先级、栈大小
- `priority`：线程优先级
- `cpuId`：CPU核心ID

**返回值：**
- `start`：成功返回true，失败返回false
- `join`：成功返回true，失败返回false
- `detach`：成功返回true，失败返回false
- `setPriority`：成功返回true，失败返回false
- `setCpuAffinity`：成功返回true，失败返回false

### 6.2 PlatformMemory接口

```cpp
class PlatformMemory {
public:
    // 分配内存
    static void* allocate(size_t size, MemoryType type);
    
    // 释放内存
    static void deallocate(void* ptr);
    
    // 创建内存池
    static MemoryPool* createMemoryPool(size_t blockSize, size_t blockCount);
    
    // 获取内存使用情况
    static MemoryUsage getMemoryUsage();
    
    // 锁定内存
    static bool lockMemory(void* ptr, size_t size);
    
    // 解锁内存
    static bool unlockMemory(void* ptr, size_t size);
};
```

**参数说明**
- `size`：内存大小
- `type`：内存类型
- `ptr`：内存指针
- `blockSize`：内存块大小
- `blockCount`：内存块数量

**返回值：**
- `allocate`：返回分配的内存指针，失败返回nullptr
- `createMemoryPool`：返回创建的内存池实例，失败返回nullptr
- `lockMemory`：成功返回true，失败返回false
- `unlockMemory`：成功返回true，失败返回false

### 6.3 PlatformNetwork接口

```cpp
class PlatformNetwork {
public:
    // 创建套接字
    static int createSocket(int domain, int type, int protocol);
    
    // 关闭套接字
    static void closeSocket(int sockfd);
    
    // 设置套接字选项
    static bool setSocketOption(int sockfd, int level, int optname, const void* optval, socklen_t optlen);
    
    // 获取套接字选项
    static bool getSocketOption(int sockfd, int level, int optname, void* optval, socklen_t* optlen);
    
    // 获取网络接口信息
    static std::vector<NetworkInterface> getNetworkInterfaces();
    
    // 创建事件循环
    static EventLoop* createEventLoop();
};
```

**参数说明**
- `domain`：地址域
- `type`：套接字类型
- `protocol`：协议
- `sockfd`：套接字文件描述符
- `level`：选项级别
- `optname`：选项名称
- `optval`：选项值
- `optlen`：选项长度

**返回值：**
- `createSocket`：成功返回套接字文件描述符，失败返回-1
- `setSocketOption`：成功返回true，失败返回false
- `getSocketOption`：成功返回true，失败返回false
- `getNetworkInterfaces`：返回网络接口信息列表
- `createEventLoop`：返回创建的事件循环实例，失败返回nullptr

### 6.4 RealTimeScheduler接口

```cpp
class RealTimeScheduler {
public:
    // 获取RealTimeScheduler实例
    static RealTimeScheduler* getInstance();
    
    // 设置线程实时性优先级
    bool setThreadPriority(pthread_t thread, int priority);
    
    // 设置线程CPU亲和性
    bool setThreadCpuAffinity(pthread_t thread, int cpuId);
    
    // 获取系统实时性能力
    RealTimeCapabilities getRealTimeCapabilities() const;
    
    // 检查实时性能
    bool checkRealTimePerformance(std::chrono::milliseconds duration);
};
```

**参数说明**
- `thread`：线程ID
- `priority`：优先级
- `cpuId`：CPU核心ID
- `duration`：检查持续时间

**返回值：**
- `setThreadPriority`：成功返回true，失败返回false
- `setThreadCpuAffinity`：成功返回true，失败返回false
- `getRealTimeCapabilities`：返回系统实时性能
- `checkRealTimePerformance`：通过返回true，失败返回false

## 7. 基础支撑层接口

### 7.1 Serialization接口

```cpp
class Serialization {
public:
    // 序列化数据
    template <typename T>
    static std::vector<uint8_t> serialize(const T& data);
    
    // 反序列化数据
    template <typename T>
    static T deserialize(const std::vector<uint8_t>& data);
    
    // 序列化数据到缓冲区
    template <typename T>
    static size_t serialize(const T& data, void* buffer, size_t bufferSize);
    
    // 从缓冲区反序列化数据
    template <typename T>
    static T deserialize(const void* buffer, size_t bufferSize);
};
```

**参数说明**
- `data`：要序列化的数据
- `buffer`：缓冲区
- `bufferSize`：缓冲区大小

**返回值：**
- `serialize`：返回序列化后的数据或字节数
- `deserialize`：返回反序列化后的数据

### 7.2 Logging接口

```cpp
class Logging {
public:
    // 初始化日志系统
    static bool init(const LogOptions& options);
    
    // 记录调试日志
    static void debug(const char* format, ...);
    
    // 记录信息日志
    static void info(const char* format, ...);
    
    // 记录警告日志
    static void warn(const char* format, ...);
    
    // 记录错误日志
    static void error(const char* format, ...);
    
    // 记录致命日志
    static void fatal(const char* format, ...);
    
    // 设置日志级别
    static void setLogLevel(LogLevel level);
    
    // 获取日志级别
    static LogLevel getLogLevel();
};
```

**参数说明**
- `options`：日志选项，包括输出方式、日志文件等
- `format`：日志格式
- `level`：日志级别

### 7.3 Monitoring接口

```cpp
class Monitoring {
public:
    // 获取Monitoring实例
    static Monitoring* getInstance();
    
    // 记录性能指标
    void recordMetric(const std::string& name, double value);
    
    // 获取性能指标
    double getMetric(const std::string& name) const;
    
    // 注册健康检查
    bool registerHealthCheck(const std::string& name, std::function<bool()> check);
    
    // 执行健康检查
    bool runHealthChecks();
    
    // 获取系统状态
    SystemState getSystemState() const;
};
```

**参数说明**
- `name`：指标名称或健康检查名称
- `value`：指标值
- `check`：健康检查回调函数

**返回值：**
- `recordMetric`：成功返回true，失败返回false
- `getMetric`：返回指标值
- `registerHealthCheck`：成功返回true，失败返回false
- `runHealthChecks`：所有检查通过返回true，否则返回false
- `getSystemState`：返回系统状态

### 7.4 Security接口

```cpp
class Security {
public:
    // 初始化安全系统
    static bool init(const SecurityOptions& options);
    
    // 加密数据
    static std::vector<uint8_t> encrypt(const void* data, size_t length, const std::string& key);
    
    // 解密数据
    static std::vector<uint8_t> decrypt(const void* data, size_t length, const std::string& key);
    
    // 生成签名
    static std::vector<uint8_t> sign(const void* data, size_t length, const std::string& privateKey);
    
    // 验证签名
    static bool verify(const void* data, size_t length, const void* signature, size_t signatureLength, const std::string& publicKey);
    
    // 生成密钥对
    static bool generateKeyPair(std::string& publicKey, std::string& privateKey);
};
```

**参数说明**
- `options`：安全选项
- `data`：要加密或签名的数据
- `length`：数据长度
- `key`：加密密钥
- `privateKey`：私钥
- `signature`：签名数据
- `signatureLength`：签名长度
- `publicKey`：公钥

**返回值：**
- `init`：成功返回true，失败返回false
- `encrypt`：返回加密后的数据
- `decrypt`：返回解密后的数据
- `sign`：返回生成的签名
- `verify`：验证通过返回true，失败返回false
- `generateKeyPair`：成功返回true，失败返回false

## 8. 编译系统接口

### 8.1 AuroraBuild接口

```cpp
class AuroraBuild {
public:
    // 获取AuroraBuild实例
    static AuroraBuild* getInstance();
    
    // 初始化编译系统
    bool init(const BuildOptions& options);
    
    // 构建项目
    bool build(const std::string& projectPath);
    
    // 清理构建产物
    bool clean(const std::string& projectPath);
    
    // 运行测试
    bool test(const std::string& projectPath);
    
    // 安装构建产物
    bool install(const std::string& projectPath);
    
    // 列出项目和依赖
    std::vector<ProjectInfo> listProjects(const std::string& path);
    
    // 分析依赖关系
    DependencyGraph analyzeDependencies(const std::string& projectPath);
    
    // 初始化新项目
    bool initProject(const std::string& projectPath, const ProjectOptions& options);
};
```

**参数说明**
- `options`：编译选项或项目选项
- `projectPath`：项目路径
- `path`：路径

**返回值：**
- `init`：成功返回true，失败返回false
- `build`：成功返回true，失败返回false
- `clean`：成功返回true，失败返回false
- `test`：成功返回true，失败返回false
- `install`：成功返回true，失败返回false
- `listProjects`：返回项目信息列表
- `analyzeDependencies`：返回依赖关系图
- `initProject`：成功返回true，失败返回false

### 8.2 BuildConfig接口

```cpp
class BuildConfig {
public:
    // 加载配置文件
    bool load(const std::string& configPath);
    
    // 保存配置文件
    bool save(const std::string& configPath);
    
    // 获取项目名称
    std::string getProjectName() const;
    
    // 设置项目名称
    void setProjectName(const std::string& name);
    
    // 获取项目版本
    std::string getProjectVersion() const;
    
    // 设置项目版本
    void setProjectVersion(const std::string& version);
    
    // 添加依赖
    bool addDependency(const Dependency& dependency);
    
    // 获取依赖列表
    std::vector<Dependency> getDependencies() const;
    
    // 设置构建选项
    void setBuildOption(const std::string& key, const std::string& value);
    
    // 获取构建选项
    std::string getBuildOption(const std::string& key) const;
};
```

**参数说明**
- `configPath`：配置文件路径
- `name`：项目名称
- `version`：项目版本
- `dependency`：依赖信息
- `key`：选项键
- `value`：选项值

**返回值：**
- `load`：成功返回true，失败返回false
- `save`：成功返回true，失败返回false
- `addDependency`：成功返回true，失败返回false

## 9. 工具系统接口

### 9.1 AuroraCLI接口

```cpp
class AuroraCLI {
public:
    // 获取AuroraCLI实例
    static AuroraCLI* getInstance();
    
    // 初始化命令行接口
    bool init();
    
    // 执行命令
    int executeCommand(int argc, char* argv[]);
    
    // 注册命令
    bool registerCommand(const std::string& name, Command* command);
    
    // 获取命令列表
    std::vector<std::string> getCommands() const;
};
```

**参数说明**
- `argc`：命令行参数数量
- `argv`：命令行参数
- `name`：命令名称
- `command`：命令实例

**返回值：**
- `init`：成功返回true，失败返回false
- `executeCommand`：返回命令执行结果
- `registerCommand`：成功返回true，失败返回false
- `getCommands`：返回命令列表

### 9.2 AuroraNode工具接口

```cpp
class AuroraNodeTool {
public:
    // 列出所有节点
    std::vector<NodeInfo> listNodes();
    
    // 获取节点详细信息
    NodeInfo getNodeInfo(const std::string& nodeName);
    
    // 启动节点
    bool startNode(const std::string& nodeName);
    
    // 停止节点
    bool stopNode(const std::string& nodeName);
    
    // 重启节点
    bool restartNode(const std::string& nodeName);
    
    // 强制终止节点
    bool killNode(const std::string& nodeName);
    
    // 监控节点状态
    bool monitorNode(const std::string& nodeName);
};
```

**参数说明**
- `nodeName`：节点名称

**返回值：**
- `listNodes`：返回节点信息列表
- `getNodeInfo`：返回节点详细信息
- `startNode`：成功返回true，失败返回false
- `stopNode`：成功返回true，失败返回false
- `restartNode`：成功返回true，失败返回false
- `killNode`：成功返回true，失败返回false
- `monitorNode`：成功返回true，失败返回false

### 9.3 AuroraTopic工具接口

```cpp
class AuroraTopicTool {
public:
    // 列出所有话题
    std::vector<TopicInfo> listTopics();
    
    // 获取话题详细信息
    TopicInfo getTopicInfo(const std::string& topicName);
    
    // 查看话题数据
    bool echoTopic(const std::string& topicName);
    
    // 发布话题数据
    bool publishTopic(const std::string& topicName, const std::string& message);
    
    // 查看话题发布频率
    double getTopicHz(const std::string& topicName);
    
    // 查看话题带宽
    double getTopicBandwidth(const std::string& topicName);
    
    // 查看话题延迟
    double getTopicDelay(const std::string& topicName);
};
```

**参数说明**
- `topicName`：话题名称
- `message`：消息内容

**返回值：**
- `listTopics`：返回话题信息列表
- `getTopicInfo`：返回话题详细信息
- `echoTopic`：成功返回true，失败返回false
- `publishTopic`：成功返回true，失败返回false
- `getTopicHz`：返回话题发布频率
- `getTopicBandwidth`：返回话题带宽
- `getTopicDelay`：返回话题延迟

### 9.4 AuroraService工具接口

```cpp
class AuroraServiceTool {
public:
    // 列出所有服务
    std::vector<ServiceInfo> listServices();
    
    // 获取服务详细信息
    ServiceInfo getServiceInfo(const std::string& serviceName);
    
    // 调用服务
    bool callService(const std::string& serviceName, const std::string& request);
    
    // 查看服务类型
    std::string getServiceType(const std::string& serviceName);
    
    // 等待服务可用
    bool waitForService(const std::string& serviceName, std::chrono::milliseconds timeout);
};
```

**参数说明**
- `serviceName`：服务名称
- `request`：请求内容
- `timeout`：超时时间

**返回值：**
- `listServices`：返回服务信息列表
- `getServiceInfo`：返回服务详细信息
- `callService`：成功返回true，失败返回false
- `getServiceType`：返回服务类型
- `waitForService`：成功返回true，失败返回false

## 10. 平台适配层接口

### 10.1 PlatformDetector接口

```cpp
class PlatformDetector {
public:
    // 获取PlatformDetector实例
    static PlatformDetector* getInstance();
    
    // 检测当前平台
    PlatformType detectPlatform();
    
    // 检测平台特性
    std::vector<PlatformFeature> detectFeatures();
    
    // 检查平台是否支持某个特性
    bool hasFeature(PlatformFeature feature);
    
    // 获取平台信息
    PlatformInfo getPlatformInfo() const;
};
```

**参数说明**
- `feature`：平台特性

**返回值：**
- `detectPlatform`：返回检测到的平台类型
- `detectFeatures`：返回检测到的平台特性
- `hasFeature`：支持返回true，不支持返回false
- `getPlatformInfo`：返回平台信息

### 10.2 QnxAdapter接口

```cpp
class QnxAdapter {
public:
    // 获取QnxAdapter实例
    static QnxAdapter* getInstance();
    
    // 初始化QNX适配
    bool init();
    
    // 创建QNX线程
    pthread_t createThread(std::function<void()> func, int priority);
    
    // 创建QNX共享内存
    void* createSharedMemory(const std::string& name, size_t size);
    
    // 打开QNX共享内存
    void* openSharedMemory(const std::string& name, size_t size);
    
    // 关闭QNX共享内存
    void closeSharedMemory(void* ptr);
    
    // 获取QNX特定信息
    QnxInfo getQnxInfo() const;
};
```

**参数说明**
- `func`：线程函数
- `priority`：线程优先级
- `name`：共享内存名称
- `size`：共享内存大小
- `ptr`：共享内存指针

**返回值：**
- `init`：成功返回true，失败返回false
- `createThread`：成功返回线程ID，失败返回0
- `createSharedMemory`：成功返回共享内存指针，失败返回nullptr
- `openSharedMemory`：成功返回共享内存指针，失败返回nullptr
- `closeSharedMemory`：成功返回true，失败返回false
- `getQnxInfo`：返回QNX特定信息

### 10.3 UbuntuAdapter接口

```cpp
class UbuntuAdapter {
public:
    // 获取UbuntuAdapter实例
    static UbuntuAdapter* getInstance();
    
    // 初始化Ubuntu适配
    bool init();
    
    // 创建Linux线程
    pthread_t createThread(std::function<void()> func, int priority);
    
    // 创建Linux共享内存
    void* createSharedMemory(const std::string& name, size_t size);
    
    // 打开Linux共享内存
    void* openSharedMemory(const std::string& name, size_t size);
    
    // 关闭Linux共享内存
    void closeSharedMemory(void* ptr);
    
    // 获取Linux特定信息
    LinuxInfo getLinuxInfo() const;
};
```

**参数说明**
- `func`：线程函数
- `priority`：线程优先级
- `name`：共享内存名称
- `size`：共享内存大小
- `ptr`：共享内存指针

**返回值：**
- `init`：成功返回true，失败返回false
- `createThread`：成功返回线程ID，失败返回0
- `createSharedMemory`：成功返回共享内存指针，失败返回nullptr
- `openSharedMemory`：成功返回共享内存指针，失败返回nullptr
- `closeSharedMemory`：成功返回true，失败返回false
- `getLinuxInfo`：返回Linux特定信息

## 11. 接口使用示例

### 11.1 消息发布示例

```cpp
// 创建发布者
PublisherOptions options;
options.qos.reliability = ReliabilityLevel::RELIABLE;
options.qos.durability = DurabilityLevel::TRANSIENT_LOCAL;
Publisher* publisher = Publisher::create("/sensor/camera", options);

// 定义消息结构
struct CameraData {
    int frame_id;
    uint8_t image_data[640 * 480 * 3];
    double timestamp;
};

// 发布消息
CameraData data;
data.frame_id = 123;
data.timestamp = 1620.0;
// 填充image_data...
publisher->publish(data);

// 关闭发布者
publisher->close();
```

### 11.2 消息订阅示例

```cpp
// 创建订阅者
SubscriberOptions options;
options.qos.reliability = ReliabilityLevel::RELIABLE;
options.queue_size = 10;
Subscriber* subscriber = Subscriber::create("/sensor/camera", options);

// 设置回调函数
subscriber->setCallback<CameraData>([](const CameraData& data) {
    std::cout << "Received frame: " << data.frame_id << " at " << data.timestamp << std::endl;
    // 处理图像数据...
});

// 开始订阅
subscriber->subscribe();

// 运行一段时间后停止订阅
// ...
subscriber->unsubscribe();
subscriber->close();
```

### 11.3 请求/响应示例

```cpp
// 服务端
ResponseOptions respOptions;
respOptions.thread_pool_size = 4;
Response* response = Response::create("/service/navigation", respOptions);

// 定义请求和响应结构
struct NavRequest {
    double start_lat;
    double start_lon;
    double end_lat;
    double end_lon;
};

struct NavResponse {
    bool success;
    double distance;
    double estimated_time;
    std::vector<Waypoint> waypoints;
};

// 设置响应回调
response->setCallback<NavRequest, NavResponse>([](const NavRequest& req, NavResponse& resp) {
    // 计算导航路线
    resp.success = true;
    resp.distance = 10.5; // 公里
    resp.estimated_time = 15.0; // 分钟
    // 填充waypoints...
});

// 启动响应服务
response->start();

// 客户端
RequestOptions reqOptions;
reqOptions.timeout = std::chrono::seconds(5);
Request* request = Request::create("/service/navigation", reqOptions);

// 发送请求
NavRequest req;
req.start_lat = 39.9042;
req.start_lon = 116.4074;
req.end_lat = 39.9142;
req.end_lon = 116.4174;

NavResponse resp;
if (request->send(req, resp)) {
    if (resp.success) {
        std::cout << "Navigation successful! Distance: " << resp.distance << " km, Time: " << resp.estimated_time << " min" << std::endl;
    } else {
        std::cout << "Navigation failed!" << std::endl;
    }
} else {
    std::cout << "Request failed!" << std::endl;
}

// 清理资源
request->close();
response->stop();
response->close();
```

## 12. 接口设计原则

### 12.1 简洁性
- **接口简洁**：接口设计简洁明了，减少不必要的参数和方法
- **命名规范**：使用统一的命名规范，提高代码可读性
- **参数合理**：参数数量适中，避免过多参数导致使用困难

### 12.2 一致性
- **风格一致**：接口风格一致，遵循相同的设计模式
- **返回值一致**：返回值类型一致，错误处理方式统一
- **异常处理**：异常处理机制一致，便于上层应用处理

### 12.3 可扩展性
- **接口抽象**：使用抽象接口，便于后续扩展
- **选项参数**：使用选项参数模式，支持未来功能扩展
- **插件机制**：支持插件机制，便于功能扩展

### 12.4 可靠性
- **错误处理**：完善的错误处理机制，提供详细的错误信息
- **状态管理**：明确的状态管理，便于监控和调试
- **资源管理**：自动资源管理，避免资源泄漏

### 12.5 性能优化
- **零拷贝**：支持零拷贝技术，减少内存拷贝
- **异步接口**：提供异步接口，提高并发性能
- **批量操作**：支持批量操作，减少系统调用

## 13. 总结

本文档详细说明了AuroraRT车载实时性消息中间件的接口定义，包括业务接入层、统一API层、核心调度层、通信核心层等各层级的接口定义、参数说明、返回值和使用示例。通过本文档，开发者可以了解如何使用AuroraRT的接口进行应用开发，为系统的集成和使用提供指导。

AuroraRT的接口设计遵循简洁性、一致性、可扩展性、可靠性和性能优化的原则，为开发者提供了一套易用、高效、可靠的API。开发者可以根据自己的需求选择合适的接口进行开发，构建高性能的车载应用系统。
