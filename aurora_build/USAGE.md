# AuroraRT 编译系统 - 现代化构建工具

## ✨ 三大核心改进

### 1. 无需 Python 前缀
```bash
# 旧方式 ❌
python aurora build

# 新方式 ✅
aurora build
```

### 2. 自动消息生成
```bash
# 无需手动执行
aurora generate

# 构建时自动执行 ✅
aurora build  # 自动检测 msg/ 目录并生成消息代码
```

### 3. 可观测构建过程
```bash
aurora build  # 显示 5 个步骤的详细进度
```

## 🚀 快速开始

```bash
# 构建项目（自动消息生成）
aurora build

# 构建特定包
aurora build --packages-select aurorart core

# 清理构建产物
aurora clean --all

# 查看项目列表
aurora list
```

## 🔧 工作原理

1. **自动检测** - 扫描 msg/ 目录下的 .msg 文件
2. **自动转换** - 生成 C++ 消息头文件
3. **自动构建** - 构建所有项目
4. **进度显示** - 5 步构建过程可视化

## 📊 构建过程

```
[步骤 1/5] 自动检测并生成消息代码
[步骤 2/5] 发现项目
[步骤 3/5] 解析项目依赖
[步骤 4/5] 构建项目
[步骤 5/5] 生成环境设置脚本
```

## 📁 目录结构

```
aurora_build/
├── aurora      # 编译工具（可直接执行）
├── aurora.cmd  # Windows 命令脚本
├── msg/        # 消息定义文件
├── include/    # 自动生成的头文件
├── build/      # 构建产物和日志
└── install/    # 安装目录
```

## 🔄 与 colcon 对比

| 操作 | colcon | aurora |
|------|--------|--------|
| 构建 | `colcon build` | `aurora build` |
| 选择包 | `colcon build --packages-select pkg` | `aurora build --packages-select pkg` |
| 清理 | `colcon clean` | `aurora clean` |
| 消息生成 | 自动 | **自动** |
| 进度显示 | 部分 | **完整** |

## 💡 特色功能

- ✅ **零配置** - 无需手动执行 generate
- ✅ **进度可见** - 5 步构建过程可视化
- ✅ **日志记录** - 详细构建日志
- ✅ **跨平台** - Windows/Linux/QNX
- ✅ **智能依赖** - 自动解析项目依赖

现在就像使用 colcon 一样简单，但功能更强大！