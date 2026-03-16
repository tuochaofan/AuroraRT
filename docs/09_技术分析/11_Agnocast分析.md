# Agnocast详细分析：架构设计、核心组件与实现原理

## 一、Agnocast 概述

### 1.1 什么是 Agnocast

Agnocast 是由 Tier IV 开发的**ROS2 兼容的真正零拷贝 IPC 中间件**，专为自动驾驶场景设计。与传统零拷贝方案（如 iceoryx）不同，Agnocast 支持所有 ROS2 消息类型，包括动态大小的消息（如 PointCloud2、Image），完全消除了序列化和数据拷贝开销，为 ROS2 系统带来了革命性的性能提升。

Agnocast 的核心设计目标是在保持 ROS2 API 兼容性的前提下，实现真正的零拷贝进程间通信，解决传统 ROS2 通信在处理大数据量（如点云、图像）时的性能瓶颈问题。

### 1.2 核心优势

- **全消息类型支持**：支持所有 ROS2 消息类型，包括动态大小的消息（如`std::vector`、PointCloud2、Image），无需修改消息定义

- **ROS2 API 兼容**：与 rclcpp API 完全兼容，现有 ROS2 节点只需少量修改即可迁移

- **真正零拷贝**：完全消除序列化和数据拷贝开销，消息传输延迟降低 90% 以上

- **进程内 / 进程间统一**：无论是进程内还是进程间通信，都能实现零拷贝

- **选择性零拷贝**：可以选择性地对特定节点间通信启用零拷贝，不影响其他节点的通信机制

- **跨主机兼容**：跨主机通信仍然使用传统 ROS2 通信栈，保证兼容性

### 1.3 发展历程

- 2025 年 3 月：Tier IV 在 Autoware 社区首次提出 Agnocast 项目，发布初步设计方案

- 2025 年 6 月：发布 Agnocast 论文《ROS 2 Agnocast: Supporting Unsized Message Types for True Zero-Copy Publish/Subscribe IPC》，并向 ISORC 2025 投稿

- 2025 年 9 月：Agnocast 正式集成到 Autoware 项目中，开始社区测试

- 2026 年 2 月：Agnocast 发布稳定版本，支持 ROS2 Humble、Iron 等长期支持版本

## 二、架构设计

### 2.1 整体架构

Agnocast 采用**内存映射 + 内核模块**的架构，实现跨进程的零拷贝通信，整体架构分为四层：

```Plain

应用层（ROS2节点）
    ↓
Agnocast API层
    ↓
内存映射层（heaphook模块）
    ↓
内核模块层（agnocast_kmod）
```

### 2.2 分层架构设计

#### 2.2.1 应用层

ROS2 节点通过 Agnocast API 进行通信，API 与 rclcpp 完全兼容，只需修改命名空间即可迁移。应用层无需关心底层实现细节，只需使用 Agnocast 提供的发布者、订阅者和智能指针即可实现零拷贝通信。

#### 2.2.2 Agnocast API 层

提供与 rclcpp 兼容的 API，包括发布者、订阅者、执行器等：

- `agnocast::Publisher`：零拷贝发布者，用于发布消息
- `agnocast::Subscriber`：零拷贝订阅者，用于订阅消息
- `agnocast::Executor`：Agnocast 专用执行器，支持零拷贝消息处理
- `agnocast::ipc_shared_ptr`：专用智能指针，用于管理共享内存中的对象
- `agnocast::Node`：兼容 ROS2 节点接口的零拷贝节点

#### 2.2.3 内存映射层（heaphook模块）

通过 LD_PRELOAD 拦截进程的堆分配，将其重定向到共享内存区域：

- **堆分配拦截**：拦截 `malloc`、`free`、`calloc` 等堆分配函数
- **共享内存管理**：使用 TLSF（Two-Level Segregated Fit）内存分配器管理共享内存
- **内存对齐**：确保分配的内存满足基本对齐要求（x86_64 上为 16 字节）
- **智能切换**：根据进程状态智能切换使用共享内存或系统堆

#### 2.2.4 内核模块层（agnocast_kmod）

提供内核级的内存管理和引用计数：

- **共享内存管理**：管理共享内存区域的创建、映射和回收
- **引用计数管理**：通过内核级引用计数跟踪共享内存对象的使用情况
- **服务发现**：管理发布者和订阅者的注册与发现
- **消息传递**：处理消息的发布和订阅逻辑
- **进程管理**：跟踪参与通信的进程状态

### 2.3 核心组件交互流程

1. **初始化流程**：
   - 进程启动时，heaphook 模块通过 `__libc_start_main` 钩子初始化
   - 调用内核模块的 `initialize_agnocast` 函数创建共享内存区域
   - 初始化 TLSF 内存分配器

2. **发布流程**：
   - 应用创建 `agnocast::Publisher`
   - 通过 ioctl 向内核模块注册发布者
   - 创建 `agnocast::ipc_shared_ptr` 管理的消息对象
   - 调用 `publish` 方法，通过 ioctl 通知内核模块
   - 内核模块更新引用计数并通知订阅者
   - 通过消息队列（mq）通知订阅者进程

3. **订阅流程**：
   - 应用创建 `agnocast::Subscriber`
   - 通过 ioctl 向内核模块注册订阅者
   - 监听消息队列，等待发布者通知
   - 收到通知后，通过 ioctl 从共享内存中获取消息
   - 使用 `agnocast::ipc_shared_ptr` 管理消息生命周期
   - 消息处理完成后，通过 ioctl 释放引用计数

## 三、核心组件

### 3.1 agnocast::ipc_shared_ptr

Agnocast 专用的智能指针，用于管理共享内存中的对象，实现内核级的引用计数：

```cpp

#include <agnocast/agnocast.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>

// 创建共享内存中的PointCloud2消息
agnocast::ipc_shared_ptr<sensor_msgs::msg::PointCloud2> msg = 
    agnocast::make_ipc_shared<sensor_msgs::msg::PointCloud2>();

// 设置消息属性
msg->width = 640;
msg->height = 480;
msg->data.resize(msg->width * msg->height * 3);
```

`agnocast::ipc_shared_ptr`的核心特性：

- **内核级引用计数**：通过内核模块跟踪跨进程的引用计数
- **自动内存管理**：当引用计数为 0 时，内核模块自动回收内存
- **支持动态大小对象**：通过堆分配拦截，支持 `std::vector`、`std::string` 等动态大小对象
- **跨进程安全**：在不同进程中保持有效，实现真正的零拷贝

### 3.2 heaphook 模块

通过 `LD_PRELOAD` 拦截进程的堆分配，将其重定向到共享内存区域：

```bash

# 在启动ROS2节点时加载heaphook模块
LD_PRELOAD=/usr/lib/libagnocast_heaphook.so ros2 run my_node my_node
```

heaphook 模块的详细实现：

- **堆分配拦截**：拦截 `malloc`、`free`、`calloc`、`realloc` 等堆分配函数
- **TLSF内存分配器**：使用 Two-Level Segregated Fit 内存分配器管理共享内存
- **内存对齐**：确保分配的内存满足基本对齐要求（x86_64 上为 16 字节）
- **智能切换**：根据进程状态智能切换使用共享内存或系统堆
- **fork 处理**：通过 `pthread_atfork` 处理 fork 场景，避免子进程使用父进程的共享内存

### 3.3 共享内存管理器

管理共享内存区域的分配和回收：

- **共享内存创建**：通过内核模块创建共享内存区域
- **内存映射**：将共享内存映射到进程地址空间，确保所有进程的地址偏移相同
- **内存分配**：使用 TLSF 内存分配器在共享内存中分配对象
- **内存回收**：当引用计数为 0 时，自动回收内存块
- **动态扩展**：支持共享内存的动态扩展，适应不同大小的消息

### 3.4 内核级引用计数模块

在内核空间实现引用计数管理：

- **引用计数跟踪**：为每个共享内存对象维护引用计数
- **跨进程同步**：确保不同进程对同一对象的引用计数操作正确同步
- **内存回收**：当引用计数为 0 时，自动回收内存，避免内存泄漏
- **进程退出处理**：当进程异常退出时，自动清理该进程持有的引用

### 3.5 发布者/订阅者机制

Agnocast 的发布者/订阅者机制实现：

- **发布者实现**：
  - 通过 ioctl 向内核模块注册发布者
  - 创建共享内存中的消息对象
  - 调用 ioctl 通知内核模块发布消息
  - 通过消息队列通知订阅者进程

- **订阅者实现**：
  - 通过 ioctl 向内核模块注册订阅者
  - 创建消息队列接收发布者通知
  - 收到通知后，通过 ioctl 获取共享内存中的消息
  - 使用智能指针管理消息生命周期

### 3.6 消息队列机制

用于发布者通知订阅者的消息队列实现：

- **POSIX消息队列**：使用 `mq_open`、`mq_send` 等 POSIX 消息队列 API
- **非阻塞模式**：消息队列以非阻塞模式打开，避免阻塞发布者
- **轻量级通知**：只发送通知消息，不传输实际数据
- **自动清理**：定期清理不再使用的消息队列

## 四、设计原理

### 4.1 内存映射共享原理

Agnocast 通过内存映射实现跨进程的内存共享：

1. **共享内存创建**：内核模块创建共享内存区域，所有参与通信的进程映射到同一物理内存
2. **地址偏移一致性**：确保所有进程映射到相同的虚拟地址，使得指针在不同进程中保持有效
3. **直接内存访问**：发布者在共享内存中直接构造消息，订阅者直接读取，无需数据拷贝
4. **零拷贝传输**：数据在内存中只有一份，所有进程都可以直接访问，完全消除了数据拷贝的开销

这种机制使得 Agnocast 能够实现真正的零拷贝通信，相比传统 ROS2 通信，性能提升 10-100 倍。

### 4.2 堆分配拦截原理

通过 `LD_PRELOAD` 拦截进程的堆分配：

1. **函数拦截**：拦截 `malloc`、`free`、`calloc`、`realloc` 等堆分配函数
2. **内存重定向**：将堆分配请求重定向到共享内存区域
3. **TLSF 分配器**：使用 Two-Level Segregated Fit 内存分配器管理共享内存，提供高效的内存分配和回收
4. **对齐保证**：确保分配的内存满足基本对齐要求，与标准 C++ 库完全兼容
5. **智能切换**：根据进程状态和发布者数量智能切换使用共享内存或系统堆

堆分配拦截机制使得现有的 ROS2 消息类型，包括动态大小的消息（如 `std::vector`、PointCloud2、Image）可以直接在共享内存中使用，无需修改消息定义。

### 4.3 内核级引用计数原理

通过内核模块实现内存的引用计数管理：

1. **引用计数跟踪**：为每个共享内存对象维护一个引用计数
2. **跨进程同步**：使用内核级同步原语确保不同进程对同一对象的引用计数操作正确同步
3. **自动内存回收**：当引用计数为 0 时，自动回收内存，避免内存泄漏
4. **进程退出处理**：当进程异常退出时，内核模块自动清理该进程持有的引用，确保内存能够正确回收
5. **安全性保证**：内核级引用计数确保了共享内存的安全管理，即使在进程异常退出的情况下也能避免内存泄漏和野指针问题

### 4.4 消息传递原理

Agnocast 的消息传递机制：

1. **发布流程**：
   - 发布者在共享内存中创建消息对象
   - 通过 ioctl 通知内核模块发布消息
   - 内核模块更新消息的引用计数
   - 通过消息队列通知订阅者进程

2. **订阅流程**：
   - 订阅者监听消息队列，等待发布者通知
   - 收到通知后，通过 ioctl 从共享内存中获取消息
   - 使用 `agnocast::ipc_shared_ptr` 管理消息生命周期
   - 消息处理完成后，通过 ioctl 释放引用计数

3. **消息队列**：
   - 使用 POSIX 消息队列实现轻量级通知
   - 非阻塞模式，避免阻塞发布者
   - 只发送通知，不传输实际数据，减少开销

### 4.5 选择性零拷贝通信原理

Agnocast 支持选择性零拷贝通信：

1. **精细控制**：可以对特定节点间的通信启用零拷贝
2. **兼容性保证**：其他节点间的通信仍然使用传统 ROS2 通信栈
3. **跨主机兼容**：跨主机通信保持与传统 ROS2 通信的兼容性
4. **渐进式集成**：无需修改整个系统，只需升级需要性能提升的节点

这种设计使得 Agnocast 可以逐步集成到现有 ROS2 系统中，无需一次性替换所有节点，降低了迁移成本。

## 五、代码实现

### 5.1 核心模块实现细节

#### 5.1.1 heaphook 模块实现

heaphook 模块通过 `LD_PRELOAD` 拦截进程的堆分配函数，将其重定向到共享内存区域：

```rust

// 拦截 malloc 函数
#[no_mangle]
pub extern "C" fn malloc(size: usize) -> *mut c_void {
    if should_use_heap() {
        return unsafe { (*ORIGINAL_MALLOC.get_or_init(init_original_malloc))(size) };
    }

    let layout = match Layout::from_size_align(size, MIN_ALIGN) {
        Ok(layout) => layout,
        Err(_) => return ptr::null_mut(),
    };

    match AGNOCAST_SHARED_MEMORY_ALLOCATOR
        .get()
        .unwrap()
        .inner
        .allocate(layout)
    {
        Some(non_null_ptr) => non_null_ptr.as_ptr().cast(),
        None => ptr::null_mut(),
    }
}

// 拦截 free 函数
#[no_mangle]
pub unsafe extern "C" fn free(ptr: *mut c_void) {
    if ptr.is_null() {
        return;
    }

    if !is_shared(ptr.cast()) {
        return (*ORIGINAL_FREE.get_or_init(init_original_free))(ptr);
    }

    if IS_FORKED_CHILD.load(Ordering::Relaxed) {
        // Ignore unexpected calls to `free`.
        return;
    }

    let non_null_ptr = unsafe { NonNull::new_unchecked(ptr.cast()) };

    AGNOCAST_SHARED_MEMORY_ALLOCATOR
        .get()
        .unwrap()
        .inner
        .deallocate(non_null_ptr);
}
```

#### 5.1.2 发布者实现

发布者通过 ioctl 与内核模块通信，实现零拷贝消息发布：

```cpp

union ioctl_publish_msg_args publish_core(
  [[maybe_unused]] const void * publisher_handle /* for CARET */, const std::string & topic_name,
  const topic_local_id_t publisher_id, const uint64_t msg_virtual_address,
  std::unordered_map<topic_local_id_t, std::tuple<mqd_t, bool>> & opened_mqs)
{
  std::array<topic_local_id_t, MAX_SUBSCRIBER_NUM> subscriber_ids_buffer{};

  union ioctl_publish_msg_args publish_msg_args = {};
  publish_msg_args.topic_name = {topic_name.c_str(), topic_name.size()};
  publish_msg_args.publisher_id = publisher_id;
  publish_msg_args.msg_virtual_address = msg_virtual_address;
  publish_msg_args.subscriber_ids_buffer_addr =
    reinterpret_cast<uint64_t>(subscriber_ids_buffer.data());
  publish_msg_args.subscriber_ids_buffer_size = MAX_SUBSCRIBER_NUM;

  if (ioctl(agnocast_fd, AGNOCAST_PUBLISH_MSG_CMD, &publish_msg_args) < 0) {
    RCLCPP_ERROR(logger, "AGNOCAST_PUBLISH_MSG_CMD failed: %s", strerror(errno));
    close(agnocast_fd);
    exit(EXIT_FAILURE);
  }

  // 通过消息队列通知订阅者
  for (uint32_t i = 0; i < publish_msg_args.ret_subscriber_num; i++) {
    const topic_local_id_t subscriber_id = subscriber_ids_buffer[i];
    // 打开消息队列并发送通知
    // ...
  }

  return publish_msg_args;
}
```

### 5.2 ROS2 节点迁移示例

#### 原 rclcpp 代码

```cpp

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>

class PointCloudPublisher : public rclcpp::Node {
public:
    PointCloudPublisher() : Node("point_cloud_publisher") {
        publisher_ = this->create_publisher<sensor_msgs::msg::PointCloud2>(
            "point_cloud", 10);
    }

private:
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr publisher_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<PointCloudPublisher>());
    rclcpp::shutdown();
    return 0;
}
```

#### 迁移到 Agnocast 的代码

```cpp

#include <agnocast/agnocast.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>

class PointCloudPublisher : public agnocast::Node {
public:
    PointCloudPublisher() : agnocast::Node("point_cloud_publisher") {
        publisher_ = this->create_publisher<sensor_msgs::msg::PointCloud2>(
            "point_cloud", 10);
    }

private:
    agnocast::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr publisher_;
};

int main(int argc, char * argv[]) {
    agnocast::init(argc, argv);
    agnocast::spin(std::make_shared<PointCloudPublisher>());
    agnocast::shutdown();
    return 0;
}
```

### 5.3 零拷贝发布者示例

```cpp

#include <agnocast/agnocast.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>

int main(int argc, char * argv[]) {
    agnocast::init(argc, argv);
    auto node = agnocast::Node::make_shared("point_cloud_publisher");
    
    auto publisher = node->create_publisher<sensor_msgs::msg::PointCloud2>(
        "point_cloud", 10);
    
    // 创建共享内存中的消息
    auto msg = agnocast::make_ipc_shared<sensor_msgs::msg::PointCloud2>();
    msg->width = 640;
    msg->height = 480;
    msg->data.resize(msg->width * msg->height * 3);
    
    // 填充点云数据
    for (size_t i = 0; i < msg->data.size(); ++i) {
        msg->data[i] = static_cast<uint8_t>(i % 256);
    }
    
    // 发布消息（零拷贝）
    publisher->publish(msg);
    
    agnocast::spin_some(node);
    agnocast::shutdown();
    return 0;
}
```

### 5.4 零拷贝订阅者示例

```cpp

#include <agnocast/agnocast.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>

void point_cloud_callback(
    agnocast::ipc_shared_ptr<sensor_msgs::msg::PointCloud2> msg) {
    RCLCPP_INFO(rclcpp::get_logger("point_cloud_subscriber"),
        "Received PointCloud2: width=%d, height=%d, size=%zu",
        msg->width, msg->height, msg->data.size());
}

int main(int argc, char * argv[]) {
    agnocast::init(argc, argv);
    auto node = agnocast::Node::make_shared("point_cloud_subscriber");
    
    auto subscriber = node->create_subscription<sensor_msgs::msg::PointCloud2>(
        "point_cloud", 10, point_cloud_callback);
    
    agnocast::spin(node);
    agnocast::shutdown();
    return 0;
}
```

### 5.5 启动文件配置

```xml

<launch>
  <node name="point_cloud_publisher" pkg="my_package" exec="my_node" output="screen">
    <env name="LD_PRELOAD" value="/usr/lib/libagnocast_heaphook.so"/>
  </node>
  <node name="point_cloud_subscriber" pkg="my_package" exec="my_subscriber" output="screen">
    <env name="LD_PRELOAD" value="/usr/lib/libagnocast_heaphook.so"/>
  </node>
</launch>
```

## 六、性能优化

### 6.1 零拷贝优化

- **完全消除序列化开销**：传统 ROS2 通信需要将消息序列化为二进制格式，Agnocast 直接传递内存指针，完全消除了序列化和反序列化开销
- **消除数据拷贝**：数据在内存中只有一份，所有进程都可以直接访问，消除了数据拷贝的开销
- **支持动态大小消息**：对于 PointCloud2、Image 等动态大小的消息，传统零拷贝方案需要额外的拷贝，Agnocast 可以直接在共享内存中构造动态大小的消息
- **跨进程指针有效性**：通过统一的内存映射地址，确保指针在不同进程中保持有效

### 6.2 内存管理优化

- **TLSF 内存分配器**：使用 Two-Level Segregated Fit 内存分配器，提供高效的内存分配和回收
- **内存对齐优化**：确保分配的内存满足基本对齐要求，提高缓存命中率
- **智能内存切换**：根据进程状态和发布者数量智能切换使用共享内存或系统堆
- **内存池机制**：复用内存块，减少内存分配和回收的开销
- **内核级引用计数**：自动管理内存生命周期，避免内存泄漏

### 6.3 通信优化

- **轻量级通知机制**：使用 POSIX 消息队列实现轻量级通知，减少通信开销
- **非阻塞模式**：消息队列以非阻塞模式打开，避免阻塞发布者
- **ioctl 优化**：通过 ioctl 与内核模块高效通信，减少系统调用开销
- **消息队列缓存**：缓存已打开的消息队列，减少重复打开的开销

### 6.4 性能对比

|对比维度|传统 ROS2 通信|Agnocast|性能提升|
|---|---|---|---|
|**延迟**|毫秒级|微秒级|10-100 倍|
|**吞吐量**|万级 / 秒|十万级 / 秒|10 倍 +|
|**CPU 使用率**|较高|较低|减少 50%+|
|**内存开销**|较高|较低|减少 30%+|
|**大数据传输性能**|较差（需要拷贝）|优秀（零拷贝）|100 倍 +|

### 6.5 实际测试结果

在 Autoware 点云预处理模块的测试中，Agnocast 实现了显著的性能提升：

- **平均响应时间**：提升 16%
- **最坏情况响应时间**：提升 25%
- **CPU 使用率**：减少 40%
- **内存开销**：减少 35%

在大数据传输测试中（如点云、图像），Agnocast 的性能优势更加明显：

- **1MB 消息**：延迟从 10ms 减少到 0.1ms，提升 100 倍
- **10MB 消息**：延迟从 100ms 减少到 0.5ms，提升 200 倍
- **吞吐量**：从 10,000 消息/秒提升到 100,000 消息/秒，提升 10 倍

### 6.6 性能优化最佳实践

1. **合理设置共享内存大小**：根据应用需求调整共享内存大小，避免频繁扩展
2. **优化消息大小**：合理设计消息结构，避免过大的消息
3. **使用适当的 QoS 策略**：根据应用需求选择合适的 QoS 策略
4. **避免频繁创建和销毁发布者/订阅者**：复用发布者和订阅者对象
5. **优化回调函数**：减少回调函数的执行时间，避免阻塞事件循环

## 七、应用案例

### 7.1 Autoware 点云预处理

在 Autoware 自动驾驶平台中，Agnocast 用于点云预处理模块：

- 激光雷达点云数据通过 Agnocast 零拷贝传输到预处理模块

- 预处理模块直接读取共享内存中的点云数据，无需拷贝

- 相比传统 ROS2 通信，延迟降低 80% 以上，提高了自动驾驶的实时性

### 7.2 自动驾驶图像传输

在自动驾驶场景中，Agnocast 用于图像数据传输：

- 摄像头图像数据通过 Agnocast 零拷贝传输到图像处理模块

- 图像处理模块直接读取共享内存中的图像数据，减少 CPU 开销

- 支持高分辨率图像的实时传输，满足自动驾驶的实时性要求

### 7.3 工业机器人控制

在工业机器人控制场景中，Agnocast 用于机器人臂控制：

- 关节控制指令通过 Agnocast 零拷贝传输到控制模块

- 传感器数据通过 Agnocast 零拷贝传输到决策模块

- 提高了控制的实时性和准确性，满足工业机器人的控制要求

### 7.4 多机器人协同

在多机器人协同场景中，Agnocast 用于机器人之间的通信：

- 机器人之间的状态数据通过 Agnocast 零拷贝传输

- 减少了通信延迟，提高了协同的实时性

- 支持大规模多机器人系统的协同作业

## 八、总结与未来发展

### 8.1 总结

Agnocast 是一款革命性的 ROS2 零拷贝 IPC 中间件，它解决了传统 ROS2 通信在处理大数据量时的性能瓶颈问题。通过内存映射和内核级引用计数，Agnocast 实现了真正的零拷贝通信，支持所有 ROS2 消息类型，包括动态大小的消息。

Agnocast 的核心优势：

- **高性能**：完全消除序列化和数据拷贝开销，性能提升 10-100 倍

- **兼容性好**：与 ROS2 API 完全兼容，现有节点只需少量修改即可迁移

- **灵活性高**：支持选择性零拷贝通信，可以逐步集成到现有系统中

- **安全性高**：内核级引用计数管理，避免内存泄漏和野指针问题

### 8.2 未来发展趋势

Agnocast 的未来发展趋势主要包括：

#### 8.2.1 云原生支持

- 支持容器化部署，提高系统的可扩展性

- 支持云边协同，实现云端机器人控制

- 支持微服务架构，提高系统的模块化程度

#### 8.2.2 实时性优化

- 支持硬实时系统，满足工业机器人和自动驾驶的实时性要求

- 支持时间敏感网络（TSN），提高通信的确定性

- 提供更精细的 QoS 控制，适应不同的应用场景

#### 8.2.3 多平台支持

- 支持更多的操作系统，如 Windows、macOS、RTOS

- 支持更多的硬件平台，如 ARM、RISC-V

- 支持嵌入式设备，如微控制器

#### 8.2.4 安全性增强

- 加强安全机制，支持身份认证和访问控制

- 支持端到端加密，保障数据安全

- 提供安全审计和监控机制

Agnocast 作为 ROS2 生态系统中的重要创新，将为机器人、自动驾驶、工业自动化等领域带来革命性的性能提升，推动分布式系统的发展和创新。