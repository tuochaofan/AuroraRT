# AuroraRT 消息定义与代码生成工具

## 1. 概述

本工具用于为 AuroraRT 中间件生成消息定义和序列化代码，对标 ROS2 的消息系统，支持以下功能：

- 消息定义文件 (.aurora.msg) 的解析
- 自动生成 C++/Python 代码
- 序列化/反序列化实现
- 消息落盘与回灌
- 与 ROS2 消息格式兼容

## 2. 消息定义格式

### 2.1 基本语法

AuroraRT 使用 `.aurora.msg` 文件定义消息格式，语法与 ROS2 的 `.msg` 文件类似：

```
# 注释行
int32 id
string name
float32 value
bool enabled
```

### 2.2 支持的类型

- 基本类型：
  - `bool`
  - `int8`, `int16`, `int32`, `int64`
  - `uint8`, `uint16`, `uint32`, `uint64`
  - `float32`, `float64`
  - `string`
  - `time`
  - `duration`

- 复合类型：
  - 数组：`int32[] values`
  - 固定大小数组：`int32[5] fixed_values`
  - 嵌套消息：`MyMessage nested_msg`

## 3. 代码生成

### 3.1 命令行工具

```bash
# 生成 C++ 代码
aurora_msg_gen --cpp --input path/to/messages --output path/to/generated

# 生成 Python 代码
aurora_msg_gen --python --input path/to/messages --output path/to/generated

# 同时生成 C++ 和 Python 代码
aurora_msg_gen --cpp --python --input path/to/messages --output path/to/generated
```

### 3.2 生成的文件结构

```
generated/
├── include/
│   └── aurorart/
│       └── msg/
│           ├── message1.hpp
│           └── message2.hpp
├── src/
│   ├── message1.cpp
│   └── message2.cpp
└── python/
    └── aurorart/
        └── msg/
            ├── __init__.py
            ├── message1.py
            └── message2.py
```

## 4. 消息序列化

AuroraRT 支持多种序列化格式：

- CDR (Common Data Representation) - 与 DDS 兼容
- Protobuf - 与 Google Protocol Buffers 兼容
- JSON - 用于调试和跨平台通信

## 5. 消息落盘与回灌

### 5.1 消息落盘

```cpp
// 保存消息到文件
aurorart::msg::MyMessage msg;
msg.id = 1;
msg.name = "test";
msg.value = 3.14;

// 保存为 CDR 格式
aurorart::msg::saveMessage(msg, "message.cdr");

// 保存为 JSON 格式
aurorart::msg::saveMessageAsJSON(msg, "message.json");
```

### 5.2 消息回灌

```cpp
// 从文件加载消息
aurorart::msg::MyMessage msg;

// 从 CDR 文件加载
if (aurorart::msg::loadMessage(msg, "message.cdr")) {
    // 使用消息
}

// 从 JSON 文件加载
if (aurorart::msg::loadMessageFromJSON(msg, "message.json")) {
    // 使用消息
}
```

## 6. 与 ROS2 兼容

AuroraRT 消息系统设计为与 ROS2 消息格式兼容，可以：

- 读取和解析 ROS2 的 `.msg` 文件
- 生成与 ROS2 消息结构兼容的代码
- 支持 ROS2 消息的序列化/反序列化
- 与 ROS2 节点进行通信

## 7. 可视化工具

AuroraRT 提供消息可视化工具 `aurora_msg_viewer`，用于查看和分析消息内容：

```bash
# 查看消息文件
aurora_msg_viewer message.cdr

# 实时查看消息流
aurora_msg_viewer --topic /test_topic
```

## 8. 集成到构建系统

在 CMakeLists.txt 中添加：

```cmake
find_package(aurorart_msg REQUIRED)
aurorart_generate_messages(
    DIRECTORY msg
    FILES
        MyMessage.aurora.msg
        AnotherMessage.aurora.msg
)

target_link_libraries(${PROJECT_NAME} 
    aurorart::msg
)
```

## 9. 示例

### 9.1 定义消息

创建 `msg/SystemStatus.aurora.msg`：

```
int32 node_id
string node_name
float32 cpu_usage
float32 memory_usage
bool is_healthy
time timestamp
```

### 9.2 生成代码

```bash
aurora_msg_gen --cpp --input msg --output generated
```

### 9.3 使用消息

```cpp
#include "aurorart/msg/SystemStatus.hpp"

// 创建消息
aurorart::msg::SystemStatus status;
status.node_id = 1;
status.node_name = "controller";
status.cpu_usage = 45.5;
status.memory_usage = 60.2;
status.is_healthy = true;
status.timestamp = aurorart::msg::now();

// 序列化
std::vector<uint8_t> serialized = status.serialize();

// 反序列化
aurorart::msg::SystemStatus status2;
status2.deserialize(serialized);

// 保存到文件
aurorart::msg::saveMessage(status, "status.cdr");
```

## 10. 扩展功能

- **自定义类型支持**：可以扩展支持自定义类型
- **服务和动作定义**：支持类似 ROS2 的服务 (.srv) 和动作 (.action) 定义
- **多语言支持**：除 C++ 和 Python 外，可扩展支持其他语言
- **消息转换**：支持与其他中间件消息格式的转换