# AuroraCLI 工具系统

## 1. 概述

AuroraCLI 是 AuroraRT 项目的统一命令行工具，为 AuroraRT 的开发、测试、部署和运维提供了便捷的命令行接口。它集成了多个功能模块，包括项目管理、节点管理、话题管理、服务管理、数据记录与回放、系统监控等。

### 1.1 核心特性

- **统一接口**：提供统一的命令行入口，简化工具使用
- **模块化设计**：采用模块化设计，便于扩展和维护
- **全流程支持**：覆盖开发、测试、部署全流程，提供完整工具链
- **跨平台兼容**：支持 QNX、Linux、Windows、VxWorks 等多种平台
- **插件机制**：支持动态加载插件，便于功能扩展

## 2. 安装与配置

### 2.1 安装 AuroraCLI

AuroraCLI 通常与 AuroraRT 一起安装，也可以单独安装：

```bash
# 从源码编译安装
git clone https://github.com/your-org/auroracli.git
cd auroracli
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install

# 或使用 AuroraBuild 安装
aurorabuild install --component=cli
```

### 2.2 环境配置

安装完成后，确保 `aurora` 命令在系统 PATH 中：

```bash
# 添加到 PATH
echo 'export PATH=$PATH:/usr/local/bin' >> ~/.bashrc
source ~/.bashrc

# 验证安装
aurora --version
```

## 3. 基本命令结构

AuroraCLI 的命令结构采用分层设计，基本格式为：

```bash
aurora <module> <command> [options] [arguments]
```

其中：
- `<module>`：功能模块，如 `core`、`node`、`topic`、`service` 等
- `<command>`：具体命令，如 `start`、`stop`、`list` 等
- `[options]`：命令选项，如 `--help`、`--verbose` 等
- `[arguments]`：命令参数，如节点名称、话题名称等

## 4. 核心模块

### 4.1 核心模块（core）

核心模块用于管理 AuroraRT 核心服务：

| 命令 | 描述 | 示例 |
|------|------|------|
| `aurora core start` | 启动 AuroraRT 核心服务 | `aurora core start` |
| `aurora core stop` | 停止 AuroraRT 核心服务 | `aurora core stop` |
| `aurora core status` | 查看 AuroraRT 核心服务状态 | `aurora core status` |
| `aurora core restart` | 重启 AuroraRT 核心服务 | `aurora core restart` |
| `aurora core logs` | 查看 AuroraRT 核心服务日志 | `aurora core logs` |
| `aurora core config` | 管理 AuroraRT 核心配置 | `aurora core config --section=memory` |
| `aurora core memory` | 查看内存使用情况 | `aurora core memory` |

### 4.2 节点管理模块（node）

节点管理模块用于管理 AuroraRT 节点：

| 命令 | 描述 | 示例 |
|------|------|------|
| `aurora node list` | 列出所有节点 | `aurora node list` |
| `aurora node info <node_name>` | 查看节点详细信息 | `aurora node info sensor_node` |
| `aurora node start <node_name>` | 启动节点 | `aurora node start sensor_node` |
| `aurora node stop <node_name>` | 停止节点 | `aurora node stop sensor_node` |
| `aurora node restart <node_name>` | 重启节点 | `aurora node restart sensor_node` |
| `aurora node status <node_name>` | 查看节点状态 | `aurora node status sensor_node` |
| `aurora node delete <node_name>` | 删除节点 | `aurora node delete sensor_node` |
| `aurora node create <node_name>` | 创建节点 | `aurora node create sensor_node` |

### 4.3 话题管理模块（topic）

话题管理模块用于管理 AuroraRT 话题：

| 命令 | 描述 | 示例 |
|------|------|------|
| `aurora topic list` | 列出所有话题 | `aurora topic list` |
| `aurora topic info <topic_name>` | 查看话题详细信息 | `aurora topic info /sensor/camera` |
| `aurora topic echo <topic_name>` | 查看话题数据 | `aurora topic echo /sensor/camera` |
| `aurora topic pub <topic_name> <message>` | 发布话题消息 | `aurora topic pub /sensor/camera '{"frame_id": 1, "timestamp": 1234567890}'` |
| `aurora topic hz <topic_name>` | 查看话题发布频率 | `aurora topic hz /sensor/camera` |
| `aurora topic bw <topic_name>` | 查看话题带宽 | `aurora topic bw /sensor/camera` |

### 4.4 服务管理模块（service）

服务管理模块用于管理 AuroraRT 服务：

| 命令 | 描述 | 示例 |
|------|------|------|
| `aurora service list` | 列出所有服务 | `aurora service list` |
| `aurora service info <service_name>` | 查看服务详细信息 | `aurora service info /control/steering` |
| `aurora service call <service_name> <request>` | 调用服务 | `aurora service call /control/steering '{"angle": 10.5}'` |
| `aurora service type <service_name>` | 查看服务类型 | `aurora service type /control/steering` |

### 4.5 数据记录与回放模块（bag）

数据记录与回放模块用于记录和回放 AuroraRT 数据：

| 命令 | 描述 | 示例 |
|------|------|------|
| `aurora bag record <topics>` | 记录话题数据 | `aurora bag record /sensor/camera /sensor/lidar` |
| `aurora bag play <bag_file>` | 回放话题数据 | `aurora bag play data.bag` |
| `aurora bag info <bag_file>` | 查看 bag 文件信息 | `aurora bag info data.bag` |
| `aurora bag filter <input_bag> <output_bag> <filter>` | 过滤 bag 文件 | `aurora bag filter input.bag output.bag '/sensor/camera'` |
| `aurora bag convert <input_bag> <output_format>` | 转换 bag 文件格式 | `aurora bag convert data.bag csv` |

### 4.6 监控模块（monitor）

监控模块用于监控 AuroraRT 系统状态：

| 命令 | 描述 | 示例 |
|------|------|------|
| `aurora monitor start` | 启动监控服务 | `aurora monitor start` |
| `aurora monitor stop` | 停止监控服务 | `aurora monitor stop` |
| `aurora monitor status` | 查看监控服务状态 | `aurora monitor status` |
| `aurora monitor metrics` | 查看系统指标 | `aurora monitor metrics` |
| `aurora monitor nodes` | 查看节点状态 | `aurora monitor nodes` |
| `aurora monitor alerts` | 查看告警信息 | `aurora monitor alerts` |
| `aurora monitor dashboard` | 启动监控面板 | `aurora monitor dashboard` |
| `aurora monitor zero-copy` | 查看零拷贝状态 | `aurora monitor zero-copy` |
| `aurora monitor protocols` | 查看协议状态 | `aurora monitor protocols` |

### 4.7 工具模块（tools）

工具模块用于管理 AuroraRT 工具：

| 命令 | 描述 | 示例 |
|------|------|------|
| `aurora tools list` | 列出所有工具 | `aurora tools list` |
| `aurora tools start <tool_name>` | 启动工具 | `aurora tools start viz` |
| `aurora tools stop <tool_name>` | 停止工具 | `aurora tools stop viz` |
| `aurora tools status <tool_name>` | 查看工具状态 | `aurora tools status viz` |

## 5. 高级功能

### 5.1 可视化工具（AuroraViz）

AuroraViz 是 AuroraRT 的 3D 可视化工具，用于展示系统状态和数据：

```bash
# 启动 AuroraViz
aurora viz start

# 访问 Web 界面
# http://localhost:8081
```

### 5.2 调试工具（AuroraDebug）

AuroraDebug 是 AuroraRT 的图形化调试工具，用于故障排查：

```bash
# 启动 AuroraDebug
aurora debug start

# 访问 Web 界面
# http://localhost:8082
```

### 5.3 测试工具（AuroraTest）

AuroraTest 是 AuroraRT 的自动化测试工具，用于测试系统功能：

```bash
# 运行单元测试
aurora test unit

# 运行集成测试
aurora test integration

# 运行性能测试
aurora test performance

# 运行可靠性测试
aurora test reliability
```

### 5.4 仿真工具（AuroraSim）

AuroraSim 是 AuroraRT 的物理仿真引擎，用于模拟系统行为：

```bash
# 启动 AuroraSim
aurora sim start

# 运行仿真场景
aurora sim run <scenario>
```

### 5.5 容器化部署工具（AuroraDocker）

AuroraDocker 是 AuroraRT 的容器化部署工具，用于容器化部署：

```bash
# 构建容器镜像
aurora docker build

# 运行容器
aurora docker run

# 管理容器
aurora docker ps
```

### 5.6 边缘部署工具（AuroraEdgeDeploy）

AuroraEdgeDeploy 是 AuroraRT 的边缘部署工具，用于边缘设备部署：

```bash
# 部署到边缘设备
aurora edge deploy <device> <app>

# 管理边缘设备
aurora edge devices

# 监控边缘设备
aurora edge monitor <device>
```

## 6. 配置管理

AuroraCLI 的配置文件位于：
- Linux/QNX/macOS：`~/.aurora/config.yaml`
- Windows：`%USERPROFILE%\.aurora\config.yaml`

### 6.1 配置示例

```yaml
# ~/.aurora/config.yaml
general:
  log_level: info
  log_file: ~/.aurora/aurora.log
  default_domain: default
tools:
  viz:
    port: 8081
  debug:
    port: 8082
  monitor:
    port: 8080
  sim:
    port: 8083
network:
  timeout: 30
  retries: 3
```

### 6.2 环境变量

AuroraCLI 支持以下环境变量：

| 环境变量 | 描述 | 默认值 |
|---------|------|--------|
| `AURORA_CONFIG` | 配置文件路径 | 见上文 |
| `AURORA_LOG_LEVEL` | 日志级别 | info |
| `AURORA_LOG_FILE` | 日志文件路径 | 见上文 |
| `AURORA_TOOL_PATH` | 工具路径 | /usr/local/bin |
| `AURORA_PLUGIN_PATH` | 插件路径 | /usr/local/lib/aurora/plugins |

## 7. 插件系统

AuroraCLI 支持插件系统，可以通过插件扩展功能：

### 7.1 插件结构

```
plugin_name/
├── manifest.yaml    # 插件清单
├── plugin.py        # Python 插件
└── plugin.so        # C++ 插件
```

### 7.2 插件清单

```yaml
# manifest.yaml
name: my_plugin
version: 1.0.0
description: My custom plugin
author: Your Name
commands:
  - name: my_command
    description: My custom command
    arguments:
      - name: arg1
        type: string
        required: true
    options:
      - name: --option1
        type: bool
        default: false
```

### 7.3 加载插件

```bash
# 安装插件
aurora plugin install /path/to/plugin

# 列出插件
aurora plugin list

# 卸载插件
aurora plugin uninstall my_plugin
```

## 8. 最佳实践

### 8.1 日常使用

- **查看系统状态**：`aurora core status`
- **管理节点**：`aurora node list`、`aurora node start <node>`
- **监控话题**：`aurora topic echo <topic>`
- **调用服务**：`aurora service call <service> <request>`
- **记录数据**：`aurora bag record <topics>`
- **查看监控**：`aurora monitor dashboard`

### 8.2 故障排查

- **查看日志**：`aurora core logs`
- **检查节点状态**：`aurora node status <node>`
- **查看内存使用**：`aurora core memory`
- **查看零拷贝状态**：`aurora monitor zero-copy`
- **查看协议状态**：`aurora monitor protocols`
- **启动调试工具**：`aurora debug start`

### 8.3 性能优化

- **监控性能指标**：`aurora monitor metrics`
- **查看话题频率**：`aurora topic hz <topic>`
- **查看话题带宽**：`aurora topic bw <topic>`
- **运行性能测试**：`aurora test performance`
- **分析性能瓶颈**：`aurora debug profile`

## 9. 常见问题

### 9.1 命令未找到

**问题**：执行 `aurora` 命令时提示未找到

**解决方案**：确保 AuroraCLI 已正确安装并添加到系统 PATH 中

### 9.2 连接失败

**问题**：无法连接到 AuroraRT 核心服务

**解决方案**：检查 AuroraRT 核心服务是否已启动，网络连接是否正常

### 9.3 权限不足

**问题**：执行命令时提示权限不足

**解决方案**：使用管理员权限运行命令，或确保当前用户有足够的权限

### 9.4 插件加载失败

**问题**：插件加载失败

**解决方案**：检查插件格式是否正确，依赖是否满足

### 9.5 命令执行超时

**问题**：命令执行超时

**解决方案**：检查网络连接，增加超时时间，或检查目标服务是否正常运行

## 10. 总结

AuroraCLI 是 AuroraRT 项目的统一命令行工具，为 AuroraRT 的开发、测试、部署和运维提供了便捷的命令行接口。它集成了多个功能模块，包括项目管理、节点管理、话题管理、服务管理、数据记录与回放、系统监控等，为 AuroraRT 的全流程提供了有力支持。

通过 AuroraCLI，开发人员和运维人员可以更高效地管理 AuroraRT 系统，快速排查问题，优化系统性能，确保系统的稳定运行。同时，AuroraCLI 的插件机制也为功能扩展提供了灵活的途径，使得 AuroraRT 工具系统可以根据需要不断演进和完善。