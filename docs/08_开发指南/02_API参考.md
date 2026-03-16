# API参考

## 1. 域管理模块

### 1.1 DomainOpcode 枚举

| 枚举值 | 十六进制值 | 描述 |
|-------|-----------|------|
| CREATE_DOMAIN | 0x01 | 创建域 |
| DELETE_DOMAIN | 0x02 | 删除域 |
| JOIN_DOMAIN | 0x03 | 加入域 |
| LEAVE_DOMAIN | 0x04 | 离开域 |
| CREATE_PARTITION | 0x05 | 创建分区 |
| DELETE_PARTITION | 0x06 | 删除分区 |
| JOIN_PARTITION | 0x07 | 加入分区 |
| LEAVE_PARTITION | 0x08 | 离开分区 |
| LIST_DOMAINS | 0x09 | 列出所有域 |
| LIST_PARTITIONS | 0x0A | 列出域内所有分区 |
| GET_DOMAIN_INFO | 0x0B | 获取域信息 |
| GET_PARTITION_INFO | 0x0C | 获取分区信息 |

### 1.2 DomainInfo 类

#### 1.2.1 构造函数

```cpp
DomainInfo(const std::string& name, const std::string& description = "");
```
- **参数**：
  - `name`：域名称
  - `description`：域描述（可选）

#### 1.2.2 方法

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `getName()` | `std::string` | 获取域名称 | 无 |
| `getDescription()` | `std::string` | 获取域描述 | 无 |
| `isActive()` | `bool` | 检查域是否活跃 | 无 |
| `setDescription(const std::string& description)` | `void` | 设置域描述 | `description`：域描述 |
| `setActive(bool active)` | `void` | 设置域活跃状态 | `active`：活跃状态 |
| `addPartition(const std::string& partitionName)` | `bool` | 添加分区 | `partitionName`：分区名称 |
| `removePartition(const std::string& partitionName)` | `bool` | 移除分区 | `partitionName`：分区名称 |
| `getPartitions()` | `std::vector<std::string>` | 获取所有分区 | 无 |
| `hasPartition(const std::string& partitionName)` | `bool` | 检查是否存在分区 | `partitionName`：分区名称 |
| `addNode(const std::string& nodeId)` | `bool` | 添加节点 | `nodeId`：节点ID |
| `removeNode(const std::string& nodeId)` | `bool` | 移除节点 | `nodeId`：节点ID |
| `getNodes()` | `std::vector<std::string>` | 获取所有节点 | 无 |
| `hasNode(const std::string& nodeId)` | `bool` | 检查是否存在节点 | `nodeId`：节点ID |

### 1.3 PartitionInfo 类

#### 1.3.1 构造函数

```cpp
PartitionInfo(const std::string& name, const std::string& domainName, const std::string& description = "");
```
- **参数**：
  - `name`：分区名称
  - `domainName`：域名称
  - `description`：分区描述（可选）

#### 1.3.2 方法

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `getName()` | `std::string` | 获取分区名称 | 无 |
| `getDomainName()` | `std::string` | 获取域名称 | 无 |
| `getDescription()` | `std::string` | 获取分区描述 | 无 |
| `isActive()` | `bool` | 检查分区是否活跃 | 无 |
| `setDescription(const std::string& description)` | `void` | 设置分区描述 | `description`：分区描述 |
| `setActive(bool active)` | `void` | 设置分区活跃状态 | `active`：活跃状态 |
| `addNode(const std::string& nodeId)` | `bool` | 添加节点 | `nodeId`：节点ID |
| `removeNode(const std::string& nodeId)` | `bool` | 移除节点 | `nodeId`：节点ID |
| `getNodes()` | `std::vector<std::string>` | 获取所有节点 | 无 |
| `hasNode(const std::string& nodeId)` | `bool` | 检查是否存在节点 | `nodeId`：节点ID |

### 1.4 DomainManager 类

#### 1.4.1 静态方法

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `instance()` | `DomainManager&` | 获取域管理器单例实例 | 无 |

#### 1.4.2 方法

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `init()` | `bool` | 初始化域管理器 | 无 |
| `shutdown()` | `bool` | 关闭域管理器 | 无 |
| `createDomain(const std::string& domainName, const std::string& description = "")` | `bool` | 创建域 | `domainName`：域名称<br>`description`：域描述（可选） |
| `deleteDomain(const std::string& domainName)` | `bool` | 删除域 | `domainName`：域名称 |
| `joinDomain(const std::string& domainName, const std::string& nodeId)` | `bool` | 加入域 | `domainName`：域名称<br>`nodeId`：节点ID |
| `leaveDomain(const std::string& domainName, const std::string& nodeId)` | `bool` | 离开域 | `domainName`：域名称<br>`nodeId`：节点ID |
| `listDomains()` | `std::vector<std::string>` | 列出所有域 | 无 |
| `getDomain(const std::string& domainName)` | `DomainInfo*` | 获取域信息 | `domainName`：域名称 |
| `createPartition(const std::string& domainName, const std::string& partitionName, const std::string& description = "")` | `bool` | 创建分区 | `domainName`：域名称<br>`partitionName`：分区名称<br>`description`：分区描述（可选） |
| `deletePartition(const std::string& domainName, const std::string& partitionName)` | `bool` | 删除分区 | `domainName`：域名称<br>`partitionName`：分区名称 |
| `joinPartition(const std::string& domainName, const std::string& partitionName, const std::string& nodeId)` | `bool` | 加入分区 | `domainName`：域名称<br>`partitionName`：分区名称<br>`nodeId`：节点ID |
| `leavePartition(const std::string& domainName, const std::string& partitionName, const std::string& nodeId)` | `bool` | 离开分区 | `domainName`：域名称<br>`partitionName`：分区名称<br>`nodeId`：节点ID |
| `listPartitions(const std::string& domainName)` | `std::vector<std::string>` | 列出域内所有分区 | `domainName`：域名称 |
| `getPartition(const std::string& domainName, const std::string& partitionName)` | `PartitionInfo*` | 获取分区信息 | `domainName`：域名称<br>`partitionName`：分区名称 |
| `getNodeDomains(const std::string& nodeId)` | `std::vector<std::string>` | 获取节点所在的域 | `nodeId`：节点ID |
| `getNodePartitions(const std::string& nodeId, const std::string& domainName)` | `std::vector<std::string>` | 获取节点在域中所在的分区 | `nodeId`：节点ID<br>`domainName`：域名称 |
| `processOpcode(DomainOpcode opcode, const std::string& data, std::string& response)` | `bool` | 处理操作码 | `opcode`：操作码<br>`data`：操作数据<br>`response`：响应数据（输出参数） |

## 2. 其他模块API

### 2.1 节点管理模块

#### 2.1.1 NodeManager 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `instance()` | `NodeManager&` | 获取节点管理器单例实例 | 无 |
| `init()` | `bool` | 初始化节点管理器 | 无 |
| `start()` | `bool` | 启动节点管理器 | 无 |
| `stop()` | `bool` | 停止节点管理器 | 无 |
| `createNode(const std::string& name)` | `std::shared_ptr<Node>` | 创建节点 | `name`：节点名称 |
| `getNode(const std::string& id)` | `std::shared_ptr<Node>` | 获取节点 | `id`：节点ID |
| `listNodes()` | `std::vector<std::shared_ptr<Node>>` | 列出所有节点 | 无 |

### 2.2 通信模式模块

#### 2.2.1 CommunicationPatternFactory 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `createPattern(PatternType type)` | `std::unique_ptr<CommunicationPattern>` | 创建通信模式 | `type`：模式类型 |

#### 2.2.2 PubSubPattern 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `createPublisher<T>(const std::string& topic)` | `std::unique_ptr<Publisher<T>>` | 创建发布者 | `topic`：主题名称 |
| `createSubscriber<T>(const std::string& topic, std::function<void(const T&)> callback)` | `std::unique_ptr<Subscriber<T>>` | 创建订阅者 | `topic`：主题名称<br>`callback`：回调函数 |

#### 2.2.3 ReqRespPattern 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `createClient<TReq, TResp>(const std::string& service)` | `std::unique_ptr<Client<TReq, TResp>>` | 创建客户端 | `service`：服务名称 |
| `createServer<TReq, TResp>(const std::string& service, std::function<TResp(const TReq&)> handler)` | `std::unique_ptr<Server<TReq, TResp>>` | 创建服务端 | `service`：服务名称<br>`handler`：处理函数 |

#### 2.2.4 EventPattern 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `createNotifier(const std::string& event)` | `std::unique_ptr<Notifier>` | 创建通知器 | `event`：事件名称 |
| `createListener(const std::string& event, std::function<void()> callback)` | `std::unique_ptr<Listener>` | 创建监听器 | `event`：事件名称<br>`callback`：回调函数 |

#### 2.2.5 PushPullPattern 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `createPusher(const std::string& channel)` | `std::unique_ptr<Pusher>` | 创建推送器 | `channel`：通道名称 |
| `createPuller<T>(const std::string& channel, std::function<void(const T&)> callback)` | `std::unique_ptr<Puller<T>>` | 创建拉取器 | `channel`：通道名称<br>`callback`：回调函数 |

### 2.3 内存管理模块

#### 2.3.1 MemoryManager 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `instance()` | `MemoryManager&` | 获取内存管理器单例实例 | 无 |
| `init()` | `bool` | 初始化内存管理器 | 无 |
| `shutdown()` | `bool` | 关闭内存管理器 | 无 |
| `allocate(size_t size)` | `void*` | 分配内存 | `size`：内存大小 |
| `deallocate(void* ptr)` | `void` | 释放内存 | `ptr`：内存指针 |
| `getMemoryUsage()` | `size_t` | 获取内存使用情况 | 无 |

### 2.4 传输模块

#### 2.4.1 TransportManager 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `instance()` | `TransportManager&` | 获取传输管理器单例实例 | 无 |
| `init()` | `bool` | 初始化传输管理器 | 无 |
| `start()` | `bool` | 启动传输管理器 | 无 |
| `stop()` | `bool` | 停止传输管理器 | 无 |
| `getTransport(TransportType type)` | `std::shared_ptr<Transport>` | 获取传输实例 | `type`：传输类型 |

### 2.5 服务发现模块

#### 2.5.1 ServiceDiscoveryManager 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `instance()` | `ServiceDiscoveryManager&` | 获取服务发现管理器单例实例 | 无 |
| `init(DiscoveryMode mode)` | `bool` | 初始化服务发现管理器 | `mode`：发现模式 |
| `start()` | `bool` | 启动服务发现管理器 | 无 |
| `stop()` | `bool` | 停止服务发现管理器 | 无 |
| `registerService(const ServiceInfo& service)` | `bool` | 注册服务 | `service`：服务信息 |
| `unregisterService(const std::string& serviceName)` | `bool` | 注销服务 | `serviceName`：服务名称 |
| `discoverServices()` | `std::vector<ServiceInfo>` | 发现服务 | 无 |

### 2.6 调度模块

#### 2.6.1 SchedulerManager 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `instance()` | `SchedulerManager&` | 获取调度管理器单例实例 | 无 |
| `init()` | `bool` | 初始化调度管理器 | 无 |
| `start()` | `bool` | 启动调度管理器 | 无 |
| `stop()` | `bool` | 停止调度管理器 | 无 |
| `scheduleTask(std::function<void()> task)` | `uint64_t` | 调度任务 | `task`：任务函数 |
| `cancelTask(uint64_t taskId)` | `bool` | 取消任务 | `taskId`：任务ID |

### 2.7 安全模块

#### 2.7.1 SecurityManager 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `instance()` | `SecurityManager&` | 获取安全管理器单例实例 | 无 |
| `init()` | `bool` | 初始化安全管理器 | 无 |
| `shutdown()` | `bool` | 关闭安全管理器 | 无 |
| `authenticateNode(const std::string& nodeId, const std::string& credential)` | `bool` | 认证节点 | `nodeId`：节点ID<br>`credential`：凭证 |
| `encryptData(const void* data, size_t size, std::vector<uint8_t>& encryptedData)` | `bool` | 加密数据 | `data`：数据指针<br>`size`：数据大小<br>`encryptedData`：加密数据（输出参数） |
| `decryptData(const void* data, size_t size, std::vector<uint8_t>& decryptedData)` | `bool` | 解密数据 | `data`：加密数据指针<br>`size`：加密数据大小<br>`decryptedData`：解密数据（输出参数） |

### 2.8 监控模块

#### 2.8.1 MonitorManager 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `instance()` | `MonitorManager&` | 获取监控管理器单例实例 | 无 |
| `init()` | `bool` | 初始化监控管理器 | 无 |
| `start()` | `bool` | 启动监控管理器 | 无 |
| `stop()` | `bool` | 停止监控管理器 | 无 |
| `createMonitor(const std::string& name)` | `std::shared_ptr<Monitor>` | 创建监控器 | `name`：监控器名称 |
| `getComponentHealth()` | `std::unordered_map<std::string, HealthStatus>` | 获取组件健康状态 | 无 |

### 2.9 诊断模块

#### 2.9.1 DiagnosticsManager 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `instance()` | `DiagnosticsManager&` | 获取诊断管理器单例实例 | 无 |
| `init()` | `bool` | 初始化诊断管理器 | 无 |
| `start()` | `bool` | 启动诊断管理器 | 无 |
| `stop()` | `bool` | 停止诊断管理器 | 无 |
| `detectFaults()` | `std::vector<Fault>` | 检测故障 | 无 |
| `analyzeFaults()` | `void` | 分析故障 | 无 |
| `recoverFromFault(const Fault& fault)` | `bool` | 从故障中恢复 | `fault`：故障信息 |
| `getDiagnosticStatus()` | `DiagnosticStatus` | 获取诊断状态 | 无 |
| `getDiagnosticAnalysis()` | `std::string` | 获取诊断分析 | 无 |

### 2.10 插件模块

#### 2.10.1 PluginManager 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `instance()` | `PluginManager&` | 获取插件管理器单例实例 | 无 |
| `init()` | `bool` | 初始化插件管理器 | 无 |
| `start()` | `bool` | 启动插件管理器 | 无 |
| `stop()` | `bool` | 停止插件管理器 | 无 |
| `loadPlugin(const std::string& path)` | `bool` | 加载插件 | `path`：插件路径 |
| `unloadPlugin(const std::string& name)` | `bool` | 卸载插件 | `name`：插件名称 |
| `listPlugins()` | `std::vector<std::shared_ptr<Plugin>>` | 列出已加载的插件 | 无 |
| `listAvailablePlugins()` | `std::vector<std::string>` | 列出可用的插件 | 无 |

### 2.11 工具模块

#### 2.11.1 Logger 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `instance()` | `Logger&` | 获取日志器单例实例 | 无 |
| `init()` | `bool` | 初始化日志器 | 无 |
| `log(LogLevel level, const char* format, ...)` | `void` | 记录日志 | `level`：日志级别<br>`format`：日志格式<br>`...`：可变参数 |
| `setLogLevel(LogLevel level)` | `void` | 设置日志级别 | `level`：日志级别 |

#### 2.11.2 Config 类

| 方法 | 返回类型 | 描述 | 参数 |
|------|---------|------|------|
| `instance()` | `Config&` | 获取配置管理器单例实例 | 无 |
| `load(const std::string& path)` | `bool` | 加载配置文件 | `path`：配置文件路径 |
| `get<T>(const std::string& key, const T& defaultValue)` | `T` | 获取配置值 | `key`：配置键<br>`defaultValue`：默认值 |
| `set(const std::string& key, const std::string& value)` | `void` | 设置配置值 | `key`：配置键<br>`value`：配置值 |