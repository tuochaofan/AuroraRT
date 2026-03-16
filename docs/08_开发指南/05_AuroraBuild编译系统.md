# AuroraBuild 编译系统

## 1. 概述

AuroraBuild 是 AuroraRT 项目的自研编译系统，专为多语言混合编译和跨平台构建而设计。它提供了简单易用的配置方式，支持 C++、C、Python、Java 等多种语言的混合编译，同时具备并行构建和依赖管理能力。

### 1.1 核心特性

- **多语言支持**：支持 C++、C、Python、Java 等多种语言混合编译
- **跨平台兼容**：支持 QNX、Linux、Windows、VxWorks 等多种操作系统
- **并行构建**：支持多线程并行构建，提高构建速度
- **依赖管理**：自动解析和管理包依赖关系
- **简洁配置**：采用 YAML 格式的配置文件，易于理解和维护
- **独立轻量**：不依赖 Python 等外部运行时，提高跨平台兼容性

## 2. 安装与配置

### 2.1 安装 AuroraBuild

#### 2.1.1 从源码编译

```bash
# 克隆 AuroraBuild 代码
git clone https://github.com/your-org/aurorabuild.git
cd aurorabuild

# 编译安装
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

#### 2.1.2 预编译包安装

对于支持的平台，可以直接下载预编译包进行安装：

```bash
# 下载预编译包
wget https://github.com/your-org/aurorabuild/releases/download/v1.0.0/aurorabuild-1.0.0-linux-x86_64.tar.gz

# 解压并安装
tar -xzf aurorabuild-1.0.0-linux-x86_64.tar.gz
sudo mv aurorabuild /usr/local/bin/
```

### 2.2 环境配置

安装完成后，确保 `aurorabuild` 命令在系统 PATH 中：

```bash
# 添加到 PATH
echo 'export PATH=$PATH:/usr/local/bin' >> ~/.bashrc
source ~/.bashrc

# 验证安装
aurorabuild --version
```

## 3. 配置文件

AuroraBuild 使用 YAML 格式的配置文件，默认名称为 `aurora.yml`，位于项目根目录。

### 3.1 基本配置

```yaml
# aurora.yml
name: my_project
type: mixed  # 项目类型：cpp, python, java, mixed
version: 1.0.0
dependencies:
  - name: eigen
    version: ^3.4.0
  - name: numpy
    version: ^1.21.0
languages:
  cpp:
    sources: src/cpp
    include_dirs:
      - include
    libraries:
      - pthread
    flags:
      - -std=c++17
      - -O2
  python:
    sources: src/python
    packages: my_package
    dependencies:
      - numpy
      - matplotlib
  java:
    sources: src/java
    main_class: com.example.Main
```

### 3.2 配置选项

| 选项 | 描述 | 示例 |
|------|------|------|
| `name` | 项目名称 | `my_project` |
| `type` | 项目类型 | `cpp`, `python`, `java`, `mixed` |
| `version` | 项目版本 | `1.0.0` |
| `dependencies` | 项目依赖 | 见上面示例 |
| `languages` | 语言配置 | 见上面示例 |
| `languages.cpp.sources` | C++ 源代码目录 | `src/cpp` |
| `languages.cpp.include_dirs` | C++ 包含目录 | `[include]` |
| `languages.cpp.libraries` | C++ 链接库 | `[pthread]` |
| `languages.cpp.flags` | C++ 编译标志 | `[-std=c++17]` |
| `languages.python.sources` | Python 源代码目录 | `src/python` |
| `languages.python.packages` | Python 包名 | `my_package` |
| `languages.python.dependencies` | Python 依赖 | `[numpy]` |
| `languages.java.sources` | Java 源代码目录 | `src/java` |
| `languages.java.main_class` | Java 主类 | `com.example.Main` |

## 4. 命令行使用

### 4.1 基本命令

| 命令 | 描述 | 示例 |
|------|------|------|
| `aurorabuild build` | 构建项目 | `aurorabuild build` |
| `aurorabuild clean` | 清理构建产物 | `aurorabuild clean` |
| `aurorabuild install` | 安装项目 | `aurorabuild install` |
| `aurorabuild test` | 运行测试 | `aurorabuild test` |
| `aurorabuild init` | 初始化项目 | `aurorabuild init my_project` |
| `aurorabuild --help` | 显示帮助信息 | `aurorabuild --help` |
| `aurorabuild --version` | 显示版本信息 | `aurorabuild --version` |

### 4.2 高级选项

| 选项 | 描述 | 示例 |
|------|------|------|
| `--config` | 指定配置文件 | `aurorabuild build --config custom.yml` |
| `--jobs` | 指定并行构建任务数 | `aurorabuild build --jobs 8` |
| `--verbose` | 显示详细构建信息 | `aurorabuild build --verbose` |
| `--prefix` | 指定安装前缀 | `aurorabuild install --prefix /opt/aurora` |
| `--build-type` | 指定构建类型 | `aurorabuild build --build-type Release` |

## 5. 多语言混合编译

AuroraBuild 支持多语言混合编译，允许在同一个项目中使用不同的编程语言。

### 5.1 C++ 与 Python 混合编译

```yaml
# aurora.yml
name: mixed_project
type: mixed
version: 1.0.0
dependencies:
  - name: eigen
    version: ^3.4.0
languages:
  cpp:
    sources: src/cpp
    include_dirs:
      - include
  python:
    sources: src/python
    packages: my_package
    dependencies:
      - numpy
```

### 5.2 C++ 与 Java 混合编译

```yaml
# aurora.yml
name: cpp_java_project
type: mixed
version: 1.0.0
languages:
  cpp:
    sources: src/cpp
  java:
    sources: src/java
    main_class: com.example.Main
```

## 6. 依赖管理

AuroraBuild 提供了强大的依赖管理能力，可以自动解析和管理项目依赖。

### 6.1 依赖声明

在 `aurora.yml` 文件中声明依赖：

```yaml
dependencies:
  - name: eigen
    version: ^3.4.0
  - name: spdlog
    version: ^1.9.2
  - name: protobuf
    version: ^3.19.0
```

### 6.2 依赖解析

AuroraBuild 会自动解析依赖关系，并按照正确的顺序构建依赖项。

### 6.3 本地依赖

对于本地开发的依赖，可以使用相对路径：

```yaml
dependencies:
  - name: my_library
    path: ../my_library
```

## 7. 跨平台构建

AuroraBuild 支持跨平台构建，可以在不同的操作系统上构建相同的项目。

### 7.1 平台特定配置

可以为不同平台指定特定的配置：

```yaml
platforms:
  linux:
    cpp:
      flags:
        - -pthread
  windows:
    cpp:
      flags:
        - /MT
  qnx:
    cpp:
      flags:
        - -DQNX
```

### 7.2 交叉编译

AuroraBuild 支持交叉编译，可以为目标平台构建项目：

```bash
# 为 QNX 平台交叉编译
aurorabuild build --platform qnx --toolchain /path/to/qnx/toolchain
```

## 8. 与其他构建系统集成

AuroraBuild 可以与其他构建系统集成，如 CMake、Make 等。

### 8.1 与 CMake 集成

```yaml
# aurora.yml
name: cmake_project
type: cpp
version: 1.0.0
build_system: cmake
cmake:
  options:
    - -DCMAKE_BUILD_TYPE=Release
    - -DBUILD_TESTS=ON
```

### 8.2 与 Make 集成

```yaml
# aurora.yml
name: make_project
type: cpp
version: 1.0.0
build_system: make
make:
  targets:
    - all
    - test
```

## 9. 最佳实践

### 9.1 项目结构

```
my_project/
├── aurora.yml        # AuroraBuild 配置文件
├── src/
│   ├── cpp/          # C++ 源代码
│   ├── python/       # Python 源代码
│   └── java/         # Java 源代码
├── include/          # 头文件
├── tests/            # 测试代码
└── README.md         # 项目说明
```

### 9.2 配置建议

- **合理组织依赖**：将依赖分为必要依赖和可选依赖
- **使用语义化版本**：为依赖指定语义化版本号
- **平台特定配置**：为不同平台提供适当的配置
- **并行构建**：使用 `--jobs` 选项提高构建速度
- **定期更新依赖**：及时更新依赖到最新版本

### 9.3 常见问题

#### 9.3.1 依赖解析失败

**问题**：依赖解析失败，找不到指定版本的依赖

**解决方案**：检查依赖名称和版本是否正确，确保依赖源可访问

#### 9.3.2 编译错误

**问题**：编译过程中出现错误

**解决方案**：检查源代码和编译配置，使用 `--verbose` 选项查看详细错误信息

#### 9.3.3 跨平台构建失败

**问题**：在目标平台上构建失败

**解决方案**：检查平台特定配置，确保工具链设置正确

## 10. 总结

AuroraBuild 是一个功能强大、易于使用的编译系统，专为多语言混合编译和跨平台构建而设计。它提供了简洁的配置方式、强大的依赖管理能力和高效的并行构建功能，为 AuroraRT 项目的开发和部署提供了有力支持。

通过 AuroraBuild，开发人员可以更专注于代码开发，而不是构建系统的配置和管理，从而提高开发效率和代码质量。同时，AuroraBuild 的跨平台支持也使得 AuroraRT 可以在不同的操作系统上无缝运行，为项目的广泛应用提供了保障。