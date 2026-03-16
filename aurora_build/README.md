# AuroraRT 编译系统使用指南

## 快速开始

### 1. 构建项目

```bash
# 进入项目目录
cd aurora_build

# 构建所有项目（自动检测并生成消息代码）
aurora build

# 或指定 Release 版本
aurora build --release

# 或指定 Debug 版本
aurora build --debug

# 选择要构建的包
aurora build --packages-select aurorart core

# 并行构建（默认使用 CPU 核心数）
aurora build --parallel 8

# 显示详细信息
aurora build --verbose
```

### 2. 清理构建产物

```bash
# 清理所有（构建产物和生成的代码）
aurora clean --all

# 仅清理生成的代码
aurora clean --code

# 仅清理构建产物
aurora clean --build
```

### 3. 环境设置

```bash
# 生成环境设置脚本
aurora env

# Windows PowerShell
.\install\setup.ps1

# Windows CMD
.\install\setup.bat

# Linux/macOS
source install/setup.bash
```

### 4. 列出项目

```bash
# 列出所有发现的项目
aurora list
```

## 特性说明

### 自动消息生成

编译系统会在构建前**自动检测** `msg/` 目录下的所有 `.msg` 文件，并自动生成对应的 C++ 头文件到 `include/` 目录。无需手动执行 `generate` 命令。

### 可观测的构建过程

构建过程分为 5 个清晰的步骤，每一步都有详细的状态显示：

1. **自动检测并生成消息代码** - 搜索并生成消息代码
2. **发现项目** - 扫描工作空间中的项目
3. **解析项目依赖** - 分析项目间的依赖关系
4. **构建项目** - 逐个构建项目（显示进度）
5. **生成环境设置脚本** - 创建环境设置脚本

### 日志系统

每次构建都会生成详细的日志文件，保存在 `build/` 目录下，文件名格式为 `aurora_build_YYYYMMDD_HHMMSS.log`。

日志包含：
- 构建开始时间和工作空间信息
- 消息自动生成过程
- 项目发现和依赖解析
- 每个项目的配置、构建、安装过程
- 错误和警告信息

### 多平台支持

编译系统支持以下平台：
- **Windows**: 自动检测并使用 MSVC 或 MinGW
- **Linux**: 自动检测并使用 GCC 或 Clang
- **QNX**: 支持交叉编译
- **macOS**: 自动检测并使用 Clang
- **VxWorks**: 支持交叉编译

## 硬件特性支持

编译系统会自动检测并利用以下硬件特性：
- **TSN (Time-Sensitive Networking)**: 支持时间敏感网络
- **SSE2/AVX**: 支持 SIMD 指令集加速
- **NEON**: ARM 平台支持 NEON 指令集
- **High Precision Timer**: 支持高精度定时器
- **Zero Copy**: 支持零拷贝共享内存通信

### 与参考工具的使用方法对比

| 操作 | colcon | catkin_make | aurora |
|------|--------|-------------|--------|
| 构建所有 | `colcon build` | `catkin_make` | `aurora build` |
| 选择包 | `colcon build --packages-select pkg` | `catkin_make --pkg pkg` | `aurora build --packages-select pkg` |
| 清理 | `colcon clean` | `catkin_make clean` | `aurora clean` |
| 生成代码 | 自动 | 自动 | **自动** |
| 日志 | `log/` 目录 | `build/` 目录 | `build/aurora_build_*.log` |

## 项目结构

```
aurora_build/
├── aurora              # 编译工具主程序（Python 脚本）
├── aurora.cmd          # Windows 命令脚本（可直接执行）
├── msg/                # ROS .msg 消息文件目录
│   └── MySensor.msg
├── include/            # 自动生成的头文件目录
│   └── MySensor.h
├── build/              # 构建产物和日志目录
│   └── aurora_build_*.log
├── install/            # 安装目录
│   └── setup.*
└── README.md           # 本文档
```

## 消息文件示例

创建 `msg/MySensor.msg`：

```
# 传感器数据消息
float32 temperature
float32 humidity
int32 pressure
bool is_active
string sensor_id
float32[] readings
string[] tags
```

编译系统会自动生成 `include/MySensor.h`：

```cpp
#ifndef MYSENSOR_H
#define MYSENSOR_H

#include <cstdint>
#include <string>
#include <vector>

struct MySensor {
    float temperature;
    float humidity;
    int32_t pressure;
    bool is_active;
    std::string sensor_id;
    std::vector<float> readings;
    std::vector<std::string> tags;
};

#endif // MYSENSOR_H
```

## 高级用法

### 自定义 xmake 参数

```bash
# 传递参数给 xmake
aurora build --xmake-args --mode=debug
```

### 指定工作空间

```bash
# 在其他目录构建
aurora build --workspace /path/to/workspace
```

### 并行构建

```bash
# 使用指定数量的并行任务
aurora build --parallel 16
```

## AuroraCLI 命令行工具

AuroraRT 提供了统一的命令行接口 `aurora`，支持以下命令：

```bash
# 查看平台信息和硬件特性
aurora platform

# 节点管理
aurora node list
aurora node info <node_name>
aurora node start <node_name>
aurora node stop <node_name>
aurora node restart <node_name>
aurora node kill <node_name>
aurora node monitor <node_name>

# 话题管理
aurora topic list
aurora topic info <topic_name>
aurora topic echo <topic_name>
aurora topic pub <topic_name> <message>
aurora topic hz <topic_name>
aurora topic bw <topic_name>
aurora topic delay <topic_name>

# 服务管理
aurora service list
aurora service info <service_name>
aurora service call <service_name> <request>
aurora service type <service_name>
aurora service wait <service_name>

# 参数管理
aurora param list
aurora param get <param_name>
aurora param set <param_name> <value>
aurora param delete <param_name>
aurora param load <file>
aurora param save <file>
aurora param history <param_name>

# 数据记录与回放
aurora bag record <topics>
aurora bag play <bag_file>
aurora bag info <bag_file>
aurora bag filter <bag_file> <output_file> <filter>
aurora bag convert <bag_file> <output_format>

# 构建与测试
aurora build
aurora test
aurora deploy
```

## 故障排除

### 构建失败

1. 检查日志文件：`build/aurora_build_*.log`
2. 确保已安装 xmake：`xmake --version`
3. 检查依赖项是否满足

### 消息生成失败

1. 检查 `.msg` 文件格式是否正确
2. 确保 `msg/` 目录存在
3. 手动执行生成命令：`aurora generate`

### 清理不干净

使用强制清理：
```bash
aurora clean --all
```

然后手动删除剩余目录：
```bash
# Windows
rmdir /s /q build install include

# Linux/macOS
rm -rf build install include
```

## 系统要求

- Python 3.6+
- xmake 2.7.0+
- C++ 编译器（支持 C++17）

## 许可证

本编译系统是 AuroraRT 项目的一部分，遵循 AuroraRT 的许可协议。
