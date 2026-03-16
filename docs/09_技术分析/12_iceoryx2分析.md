我已将整篇文章**完整整理为标准 Markdown 格式**，你直接复制保存为 `.md` 文件即可使用：

# 零拷贝通信框架iceoryx2原理解析

发表于 2026-01-21  更新于 2026-03-09  分类于 架构

## 简介

在之前的文章[SOA框架iceoryx原理解析]()中，我们讲解了iceoryx的架构、原理、交互，并进行源码的分析，在那篇文章最后我提到要关注iceoryx2这款通信框架，现在我们来把iceoryx2的分析给补上。
iceoryx2，缩写为iox2，中文名叫冰羚2，所以当后面提到冰羚2、iox2、iceoryx2的时候，他们都指代的是iceoryx2。
冰羚2与冰羚一样，是一款基于共享内存的零拷贝的通信框架，他通过对共享内存文件读写的封装和操作，实现了同一主机内的进程之间的数据传输。
他与冰羚最主要的差别是冰羚2采用了去中心化的架构设计，消除了单点故障分险。

早期的冰羚2只支持rust语言，只支持linux系统，随着版本迭代，如今已经支持`c`/`c++`/`rust`/`python`/`c#`。

下面是iceoryx2的整体架构图：

![iox2-architecture]() 图片来自[Introduction]()

从图中可以看出，iceoryx2支持各种操作系统，支持各种编程语言，同时既支持iox2的app之间的通信，也支持通过扩展来接入到DDS和ROS等通信网络。

这是iceoryx2的整体架构，也可以说是架构愿景，因为其中有些是还没有实现的，就比如支持的语言目前2026年3月只有`c`/`c++`/`rust`/`python`/`c#`，操作系统也只是支持`linux`/`macos`/`qnx`/`win`。
在与外部网络的接入方面，ros2和dds和zenoh在2025年都已经有方案来实现对iox2的接入支持，图中其他的`autosar`和`smoltcp`对iox2的使用我并不了解。

尽管如此，随着不断迭代，更多特性被加入，iceoryx2的代码已经很庞大了。下面我们基于最新的代码结构来深入分析iceoryx2的设计架构。

## 整体代码结构

我们基于最新的iceoryx2代码结构进行分析，当前版本已经包含了更多功能和组件。

iceoryx2的目录结构如下：

```
iceoryx2/
├── iceoryx2-bb/        # Building Blocks - 基础工具和组件
├── iceoryx2-cal/        # Concept Abstraction Layer - 概念抽象层
├── iceoryx2-c/          # C语言接口
├── iceoryx2-cxx/        # C++接口
├── iceoryx2-cli/        # 命令行工具
├── benchmarks/          # 性能基准测试
├── component-tests/     # 组件测试
├── examples/            # 示例代码
├── doc/                 # 文档
└── config/              # 配置文件
```

### 核心分层架构

![layered-architecture]()

图片来自[Layered Architecture]()

#### 1. iceoryx2-bb (Building Blocks)
这一层实现了基础工具和组件，包括：
- **concurrency**：并发相关工具，如原子操作、无锁数据结构
- **container**：容器实现，如vector、queue、slotmap
- **memory**：内存管理相关功能
- **posix**：POSIX系统调用封装
- **loggers**：日志系统
- **lock-free**：无锁数据结构实现
- **system-types**：系统类型定义

这些组件是构建更高层功能的基础，可以类比为"建筑砖块"。

#### 2. iceoryx2-cal (Concept Abstraction Layer)
这一层实现了通信相关的概念抽象：
- **shared_memory**：共享内存实现
- **shm_allocator**：共享内存分配器
- **zero_copy_connection**：零拷贝连接
- **event**：事件机制
- **serialize**：序列化实现
- **reactor**：反应器模式实现
- **hash**：哈希算法

这一层将基础组件组合成更高级的通信模块，可以类比为"建筑墙体"。

#### 3. 语言接口层
- **iceoryx2-c**：C语言接口
- **iceoryx2-cxx**：C++接口
- **Python/Java/C#**：其他语言绑定

#### 4. 工具和测试
- **iceoryx2-cli**：命令行工具，用于管理服务和节点
- **benchmarks**：性能基准测试
- **component-tests**：组件测试
- **examples**：各种语言的示例代码

iceoryx2的核心通信组件（publisher、subscriber、service等）构建在cal层之上，同时提供了多语言接口，方便不同语言的应用程序使用。

接下来，我会选择其中最重要的几个点进行讲解，分别是pool_allocator内存分配器，share_memory内存对象，service服务组件，publisher发布者组件，subscriber订阅者组件，以及zero_copy_connection零拷贝连接，通过了解这些对象，我们能够知晓通信框架是如何被建立的，以及通信是如何实现的。

## pool_allocator内存分配器

### 内存布局

rust语言中有个 use std::alloc::Layout;这个是做什么的。

```rust
use std::alloc::Layout;
// 创建一个布局：大小为 16 字节，对齐要求为 8 字节
let layout = Layout::from_size_align(16, 8).unwrap();
// 为特定类型创建布局（常用）
let int_layout = Layout::new::<i32>();      // 4 字节，对齐 4
let vec_layout = Layout::new::<Vec<u8>>();   // 24 字节（64位系统）
```

layout代表的是内存块的信息，包括内存块的大小，和这块内存的字节对齐要求。
上面`Vec<T>`本质上是一个三元组（胖指针结构）：

```rust
pub struct Vec<T> {
    ptr: *mut T,      // 指向堆上数据的指针
    len: usize,       // 当前元素数量
    cap: usize,       // 分配的总容量
}
```

在 64 位系统上：
- `*mut T` (指针)：8 字节
- `len` (usize)：8 字节
- `cap` (usize)：8 字节
总计：8 + 8 + 8 = 24 字节.

layout在alloc和dealloc的时候都要传递进去，alloc的时候传递比较好理解，allocator需要知道内存对齐以及需要知道要分配多大的内存。
那为什么dealloc的时候也需要传递layout呢？
我们知道c语言中malloc出来的内存，可以直接free，不需要传递layout这种类似的结构。
这是因为c语言的分配器把分配出来的内存大小隐藏在在分配出来的内存的头部：

```
// 带头部信息的分配器
// +------------+-----------+
// | Header     | User data |
// +------------+-----------+
// ^            ^
// |            |
// 原始ptr      返回给用户的ptr
```

因此dealloc的时候通过把用户指针左移Header大小后就可以知道实际分配的内存大小了。
但是rust没有做这种内存布局的假设，他允许allocator不需要Header前缀，因此就需要额外来传递layout，来获取这个实际分配的内存大小。
比如我们可以实现一个基于内存块大小的内存池，申请的时候，申请32-size就返回32大小的内存块，而不必返回一个实际是32+x的内存块。
比如实现内存池，按需返回block块，申请1k就返回1k的，而不是因为要加x额外空间就返回2k的block：

```rust
// 按大小分类的分配器
// 小对象池：16字节块、32字节块、64字节块...
// 需要知道大小来决定放回哪个池
// 这么使用
use std::alloc::{GlobalAlloc, Layout, System};
unsafe {
    let layout = Layout::new::<[u8; 1024]>();
    let ptr = System.alloc(layout);
    // 使用内存...
    System.dealloc(ptr, layout);
}
```

内存分配器的接口都会伴随着layout的传递。

### 内存分配器

`接口定义` BaseAllocator 接口, 定义allocate和deallocate接口：

```rust
pub trait BaseAllocator {
    fn allocate(&self, layout: Layout) -> Result<NonNull<[u8]>, AllocationError>;
    fn allocate_zeroed(&self, layout: Layout) -> Result<NonNull<[u8]>, AllocationError>;
    unsafe fn deallocate(&self, ptr: NonNull<u8>, layout: Layout) -> Result<(), DeallocationError>;
}
```

Allocator接口，定义grow和shrink接口：

```rust
/// Allocator with grow and shrink features.
pub trait Allocator: BaseAllocator {
    unsafe fn grow(
        &self,
        ptr: NonNull<u8>,
        old_layout: Layout,
        new_layout: Layout,
    ) -> Result<NonNull<[u8]>, AllocationGrowError>;
    unsafe fn grow_zeroed(
        &self,
        ptr: NonNull<u8>,
        old_layout: Layout,
        new_layout: Layout,
    ) -> Result<NonNull<[u8]>, AllocationGrowError>;
    unsafe fn shrink(
        &self,
        ptr: NonNull<u8>,
        old_layout: Layout,
        new_layout: Layout,
    ) -> Result<NonNull<[u8]>, AllocationShrinkError>;
}
```

`接口实现` 基于内存池并实现内存分配器接口的内存分配器实现`iceoryx2_bb_memory::pool_allocator::PoolAllocator`如下：

```rust
pub struct PoolAllocator {
    buckets: UniqueIndexSet,
    bucket_size: usize,
    bucket_alignment: usize,
    start: usize,
    size: usize,
    is_memory_initialized: AtomicBool,
}
impl BaseAllocator for PoolAllocator {
    fn allocate(&self, layout: Layout) -> Result<NonNull<[u8]>, AllocationError> {
        ...
    }
    unsafe fn deallocate(
        &self,
        ptr: NonNull<u8>,
        _layout: Layout,
    ) -> Result<(), DeallocationError> {
        ...
    }
}
impl Allocator for PoolAllocator {
    /// always returns the input ptr on success but with an increased size
    unsafe fn grow(
        &self,
        ptr: NonNull<u8>,
        old_layout: Layout,
        new_layout: Layout,
    ) -> Result<NonNull<[u8]>, AllocationGrowError> {
        ...
    }
    unsafe fn shrink(
        &self,
        ptr: NonNull<u8>,
        old_layout: Layout,
        new_layout: Layout,
    ) -> Result<NonNull<[u8]>, AllocationShrinkError> {
        ...
    }
}
```

所以可以看出`iceoryx2_bb_memory::pool_allocator::PoolAllocator`实现了内存分配/释放和内存增长/缩减的功能，是一个满足功能要求的对象。

但是这还不止，我们从前面分层架构中可知，bb层的对象还不能被iceoryx2层直接使用，而是需要经过cal层的封装，因此cal层进行了如下封装：

```rust
// 接口 iceoryx2_cal::shm_allocator::ShmAllocator
pub trait ShmAllocator: Send + Sync + 'static {
    unsafe fn new_uninit(
        max_supported_alignment_by_memory: usize,
        base_address: NonNull<[u8]>,
        config: &Self::Configuration,
    ) -> Self;
    unsafe fn init<Allocator: BaseAllocator>(
        &self,
        allocator: &Allocator,
    ) -> Result<(), ShmAllocatorInitError>;
    unsafe fn allocate(&self, layout: Layout) -> Result<PointerOffset, ShmAllocationError>;
    unsafe fn deallocate(
        &self,
        distance: PointerOffset,
        layout: Layout,
    ) -> Result<(), DeallocationError>;
}
```

```rust
// 实现：iceoryx2_cal::shm_allocator::pool_allocator::PoolAllocator
impl ShmAllocator for PoolAllocator {
    unsafe fn new_uninit(
        max_supported_alignment_by_memory: usize,
        base_address: NonNull<[u8]>,
        config: &Self::Configuration,
    ) -> Self { ... }
    unsafe fn init<Allocator: BaseAllocator>(
        &self,
        allocator: &Allocator,
    ) -> Result<(), ShmAllocatorInitError> { ... }
    unsafe fn allocate(&self, layout: Layout) -> Result<PointerOffset, ShmAllocationError> { ... }
    unsafe fn deallocate(
        &self,
        offset: PointerOffset,
        layout: Layout,
    ) -> Result<(), DeallocationError> { ... }
}
```

`iceoryx2_cal::shm_allocator::pool_allocator::PoolAllocator`本质上就是对`iceoryx2_bb_memory::pool_allocator::PoolAllocator`进行了封装，并重新暴露了类似的接口，把内存分配器从bb层提升到了cal层。
bb层和cal层的内存分配接口有个很大的差别，bb层申请和释放的是内存绝对地址（还是虚拟地址不是指物理地址），cal层申请和释放的是内存相对地址，即相对一个base地址的offset。
offset的设计是有用的，因为当同一个共享内存文件被多个进程打开时，他们的虚拟地址是不同的，他们需要通过base地址加offset才能定位到同一个内存位置。

同时从`new_uninit`和`init`两个函数来说，`iceoryx2_cal::shm_allocator::pool_allocator::PoolAllocator`需要从外面输入内存基址`base_address`以及内存分配器`allocator`，这个`base_address`也就是待分配的连续内存块，`allocator`则是用于创建辅助数据结构的内存分配器。
辅助数据结构的内存分配器也从外面传入，就可以实现`base_address`和辅助数据结构都在同一个共享内存对象/文件中。
这样不同进程之间共享内存对象和内存分配器，只要共享一个内存文件，就可以是完整的。

### 分配器原理

内存分配器是怎么实现内存分配的呢？如果你不感兴趣，可以跳过。但是这是个有意思的环节，能了解底层实现细节。

![pool-allocator-inter]()

PoolAllocator内部有两个关键部分，第一个是bucket列表，bucket的每一个格子是字节对齐后的内存块列表，内存块是连续分布的，总共有Capacity块。

第二个部分是一个index列表+header指针，他们组成一个index分配器，分配到哪个index，就代表PoolAllocator分配的内存块是bucket列表中的第index个。

index分配器要展开来讲讲，他初始的长度是Capacity+1, 每个格子是U32类型，正常只要Capacity个格子就够表达需要的index，多出来的一个是为了操作的安全和便捷。
这个U32的数组被模拟成一个链表，比如初始状态，header的值是0，表示链表指向数据的第0个格子，然后第0个格子中的值是1，表示这个链表的next节点是下标1，下标1的格子中内容是2，表示1的next是2，以此类推。
这样初始状态下，这就是一个从0到Capacity的顺序的链表。
你会发现初始状态下，没有一个格子的next指向第0个格子，为什么呢？因为已经有header指向了0了，所以不会有第二个指针指向0了。

每次分配一个index的时候，其实就是把header的值分配出去，然后 header就指向了这个模拟链表的下一个节点。
分配出去一个index的时候，这个bucket[index]指向的内存块就相当于分配出去了。
比如，初始状态下，首次分配显然会分配0，header会指向1。

再比如极端情况，我分配Capacity次，header就指向了index列表的第Capacity格（base 0），也就是我们前面多分配的一个格子，最后一个格子，示意图如下，已分配的格子被赋值成Capacity+1，指向链表尾，表示分配出去了：

![pool-allocator-allocated]()

当内存释放给PoolAllocator的时候，会传递要释放的内存块的index，这个index会被插入模拟链表的表头，具体操作的话是把当前header的值填入这个第index个格子，然后把index赋值给header，新header就指向了第index格子。

假设刚才我们从0到Capacity-1（base 0）分配出去了index值，现在又从0到Capacity-1（base 0）释放index，这时候新的header会依次经历指向0，指向1，指向2 … 最终指向Capacity-1（base 0）格子。最终示意图如下：

![pool-allocator-backall]()

你会发现格子里面有0出现了，但是没有了7，同样原因，这是因为这个时候header就是7.

### 关系图

总结一下这一堆Allocator之间的对应关系，如下:

![allocator-relationship]()

即`iceoryx2_cal::shm_allocator::pool_allocator::PoolAllocator`依赖`iceoryx2_bb_memory::pool_allocator::PoolAllocator`,`iceoryx2_bb_memory::pool_allocator::PoolAllocator`又依赖一个`UniqueIndexSet`结构。

`UniqueIndexSet`结构实现index分配（也就是上面介绍的分配器原理），`iceoryx2_bb_memory::pool_allocator::PoolAllocator`因此能实现payload的内存地址的分配，`iceoryx2_cal::shm_allocator::pool_allocator::PoolAllocator`又因此能实现payload的offset值的分配。

## share_memory内存对象

### bb层SharedMemory对象

现在先暂时忘记前面内存分配器，来从底往上梳理share_memory对象。
先是定义个`iceoryx2_bb_posix::shared_memory::SharedMemory`对象:

```rust
pub struct SharedMemory {
    name: FileName,
    size: usize,
    base_address: *mut u8,
    has_ownership: bool,
    file_descriptor: FileDescriptor,
    memory_lock: Option<MemoryLock>,
}
```

大概可以想到，这个SharedMemory对应的就是一个共享内存文件。
他具有如下接口：

```rust
impl Drop for SharedMemory {
    fn drop(&mut self) {...}
}
impl SharedMemory {
    pub fn does_exist(name: &FileName) -> bool {...}
    pub fn remove(name: &FileName) -> Result<bool, SharedMemoryRemoveError> {...}
    pub fn name(&self) -> &FileName {...}
    pub fn base_address(&self) -> NonNull<u8> {...}
    pub fn size(&self) -> usize {...}
    fn shm_create(
        name: &FileName,
        config: &SharedMemoryBuilder,
    ) -> Result<FileDescriptor, SharedMemoryCreationError> {...}
    fn shm_open(
        name: &FileName,
        config: &SharedMemoryBuilder,
    ) -> Result<FileDescriptor, SharedMemoryCreationError> {...}
    fn mmap(
        file_descriptor: &FileDescriptor,
        config: &SharedMemoryBuilder,
    ) -> Result<*mut posix::void, SharedMemoryCreationError> {...}
    fn shm_unlink(name: &FileName) -> Result<bool, SharedMemoryRemoveError> {...}
}
```

也就是对一个共享内存文件的实现。

### cal层Memory对象

接着定义了一个`iceoryx2-cal::shared_memory::posix::Memory`对象, 这个对象很重要，他把bb层的SharedMemory对象和cal层的ShmAllocator对象囊括在了一起。

```rust
#[derive(Debug)]
pub struct Memory<Allocator: ShmAllocator> {
    shared_memory: iceoryx2_bb_posix::shared_memory::SharedMemory,
    name: FileName,
    allocator: NonNull<AllocatorDetails<Allocator>>,
}
#[repr(C)]
struct AllocatorDetails<Allocator: ShmAllocator> {
    state: AtomicU64,
    allocator_id: u8,
    allocator: Allocator,
    mgmt_size: usize,
}
```

shared_memory字段就是bb层的一个SharedMemory对象，allocator就是cal层的一个ShmAllocator接口，我们从前面已经知道cal层的PoolAllocator就是实现了ShmAllocator接口的。
可见，cal层的Memory对象是一个自包含了内存和分配器的一个完备的内存对象。
这个整合了内存分配器和内存对象的结构，对外表现的就是一块可以分配数据的内存，因此他实现了以下SharedMemory接口：

```rust
/// Abstract concept of a memory shared between multiple processes. Can be created with the
/// [`SharedMemoryBuilder`].
pub trait SharedMemory<Allocator: ShmAllocator>:
    Sized + Debug + NamedConcept + NamedConceptMgmt
{
    fn allocate(&self, layout: std::alloc::Layout) -> Result<ShmPointer, ShmAllocationError>;
    unsafe fn deallocate(
        &self,
        offset: PointerOffset,
        layout: std::alloc::Layout,
    ) -> Result<(), DeallocationError>;
}
```

可以想象的是，为了实现内存分配，需要实现一个关键的步骤，就是把SharedMemory对象的地址传递给ShmAllocator。
另外再来看我们前面提到的一段话：

```
同时从new_uninit和init两个函数来说，iceoryx2_cal::shm_allocator::pool_allocator::PoolAllocator需要从外面输入内存基址base_address以及内存分配器allocator，这个base_address也就是待分配的连续内存块，allocator则是用于创建辅助数据结构的内存分配器。辅助数据结构的内存分配器也从外面传入，就可以实现base_address和辅助数据结构都在同一个共享内存对象/文件中。这样不同进程之间共享内存对象和内存分配器，就是完整的。
```

可以知道除了传递payload的地址，还要传递辅助数据结构的内存分配器，这个辅助数据结构也是我们前面分析过的，一段index数组，有多少块payload就有（多少+1）格的index。
那么这个辅助数据结构和payload数据，是如何在这块共享内存上布局的呢？是这样的：

![memory-layout]()

Memory对象内部会把内存分成3段，
第一段是AllocatorDetails结构，填充该共享内存本身的一些信息，包括allocator id， allocator指针，和payload起始地址的偏移量。
第二段是辅助数据结构index列表，他以一个分配器的形式传给allocator。
第三段是待分配的内存块，他把起始的地址传递给allocator。
这样allocator如愿得到了index数组的内存分配器和内存块的地址。

补充一下，“第二段是index列表，他以一个分配器的形式传给allocator”， 这一段index列表也就是辅助数据结构是通过一个分配器的形式传给allocator的，这个分配器实现的也是BaseAllocator接口（包含allocate和deallocate接口），其内部分配策略是一个相当简单的策略，就是线性向上分配内存，分配完为止，内存无法重复分配，释放内存则只释放一次，用来重置整段内存。
对于index列表的场景来说，index列表是一次性分配整个数组，且不做释放，所以这个简单的分配器就够用。

### 关系图

把内存的定义跟分配器的定义画到一起，大概是这样：

![memory-allocator-relationship]()

(右键-在新标签页中打开图片，可以看得更清晰)

cal层的Memory对象整合了bb层的ShareMemory对象和cal层的PoolAllocator，组合成了一个自包含内存和分配器的完整的内存对象。

## service服务组件

service在通信框架中关联了一个通信实体的集合，他有一个service name与他关联，也有若干个publisher和subscriber与他关联。
实际应用的时候我们通常把一个service与一个topic关联，把topic name等于service name。
这个topic的publisher和subscriber就是这个service的publisher和subscriber。

那么service跟前面的内存对象有什么关系吗，关系在于，与service关联的publisher和subscriber都会使用内存对象来进行数据传递。

### 关系图

与service相关的对象有点多，便于理解，先把关系图画出来如下：

![service-relationship]()

### Service

1.Service拥有静态函数来创建Builder对象，Builder负载创建出PortFactory，PortFactory再最终负责创建出publisher和subscriber。
2.持有ServiceState对象包含service相关的全部配置信息。
第一点是一个静态接口，Service只是提供了一个命名空间的作用，第二点的作用更重要。
Service对象最核心的作用其实是持有Service相关的配置信息。

### ServiceState

ServiceState拥有service相关的全部配置信息，分别有StaticConfig， GlobalConfig，DynamicStorage， StatisStorage。
static_config存的是服务名，由服务名计算出的uuid，以及代表通信参数的messaging_pattern。

```rust
// service::static_config::StaticConfig
pub struct StaticConfig {
    uuid: String,
    service_name: ServiceName,
    pub(crate) messaging_pattern: MessagingPattern,
}
pub enum MessagingPattern {
    PublishSubscribe(publish_subscribe::StaticConfig),
    Event(event::StaticConfig),
}
// service::static_config::publish_subscribe::StaticConfig
pub struct StaticConfig {
    pub(crate) max_subscribers: usize,
    pub(crate) max_publishers: usize,
    pub(crate) history_size: usize,
    pub(crate) subscriber_max_buffer_size: usize,
    pub(crate) subscriber_max_borrowed_samples: usize,
    pub(crate) enable_safe_overflow: bool,
    pub(crate) type_name: String,
}
```

global_config包含了iceoryx2的整体的一些配置：

```
[global]
root_path                                   = '/tmp/iceoryx2/'
prefix                                      = 'iox2_'
[global.service]
directory                                   = 'services'
publisher_data_segment_suffix               = '.publisher_data'
static_config_storage_suffix                = '.service'
dynamic_config_storage_suffix               = '.dynamic'
connection_suffix                           = '.connection'
creation_timeout.secs                       = 0
creation_timeout.nanos                      = 500000000
[defaults.publish_subscribe]
max_subscribers                             = 8
max_publishers                              = 2
publisher_history_size                      = 1
subscriber_max_buffer_size                  = 2
subscriber_max_borrowed_samples             = 2
publisher_max_loaned_samples                = 2
enable_safe_overflow                        = true
unable_to_deliver_strategy                  = 'block' # or 'discard_sample'
[defaults.event]
max_listeners                               = 2
max_notifiers                               = 16
```

dynamic_storage保存了引用计数以及这个service对应的publisher们和subscriber们。
他之所以是动态的，就是因为他的内容是会被后续修改的，当有新的publisher或者subscriber上线的时候他就会往里面追加新上线的id值。
从代码看，DynamicConfig实现了新增publisher或者subscriber的id的接口，没有实现移除id的接口，所以这个初版应该是没有实现下线时候移除id的功能的, 在新版本上是有实现移除功能的。

```rust
// service::dynamic_config::DynamicConfig
pub struct DynamicConfig {
    messaging_pattern: MessagingPattern,
    reference_counter: AtomicU64,
}
pub(crate) enum MessagingPattern {
    PublishSubscribe(publish_subscribe::DynamicConfig),
    Event(event::DynamicConfig),
}
// service::dynamic_config::publish_subscribe::DynamicConfig
pub struct DynamicConfig {
    pub(crate) subscribers: Container<UniqueSubscriberId>,
    pub(crate) publishers: Container<UniquePublisherId>,
}
impl DynamicConfig {
    // ...
    pub(crate) fn add_subscriber_id(&self, id: UniqueSubscriberId) -> Option<UniqueIndex> {...}
    pub(crate) fn add_publisher_id(&self, id: UniquePublisherId) -> Option<UniqueIndex> {...}
}
```

static_storage存的内容就是static_config相同的内容，差别是这里把他存到了文件中去，文件默认为`/tmp/iceoryx2/services/iox2_{uuid}.static_storage`。

### Builder

Builder拥有StaticConfig和GlobalConfig，这两个也就是ServiceState当中的StaticConfig和GlobalConfig，ServiceState中DynamicStorage和StatisStorage也是从StaticConfig和GlobalConfig派生出来的。
Builder构建Service的构建过程，先get到global_config和service名，随后根据这两信息，创建出static_config，然后再根据static_config创建出dynamic_storage和static_storage，最后把global_config|static_config|dynamic_storage|static_storage打包成ServiceState对象，并根据ServiceState再创建出Service，再根据Service创建出PortFactory。

这是一个我整理的Service-ServiceState-Builder三者之间的类关系图，黄色部分是Builder，紫色部分是ServiceState，绿色部分是Service：

![service-builder-state-relationship]()

(右键-在新标签页中打开图片，可以看得更清晰)

### PortFactory

PortFactory持有Service对象，但他不负责直接创建publisher和subscriber，而是负责创建PortFactoryPublisher和PortFactorySubscriber，PortFactoryPublisher和PortFactorySubscriber再负责创建publisher和subscriber。

### PortFactoryPublisher

PortFactoryPublisher反向持有PortFactory，间接持有了Service对象，他负责创建Publisher。

### PortFactorySubscriber

PortFactorySubscriber反向持有PortFactory，间接持有了Service对象，他负责创建Subscriber。

## Publisher组件

```rust
/// Sending endpoint of a publish-subscriber based communication.
#[derive(Debug)]
pub struct Publisher<'a, 'config: 'a, Service: service::Details<'config>, MessageType: Debug> {
    port_id: UniquePublisherId,
    pub(crate) sample_reference_counter: Vec<AtomicU64>,
    pub(crate) data_segment: Service::SharedMemory,
    config: LocalPublisherConfig,
    subscriber_connections: SubscriberConnections<'config, Service>,
    subscriber_list_state: UnsafeCell<ContainerState<'a, UniqueSubscriberId>>,
    history: Option<UnsafeCell<Queue<usize>>>,
    service: &'a Service,
    degration_callback: Option<DegrationCallback<'a>>,
    pub(crate) loan_counter: AtomicUsize,
    _dynamic_config_guard: UniqueIndex<'a>,
    _phantom_message_type: PhantomData<MessageType>,
}
```

port_id表示publisher对象的唯一id值，service表示此subscriber所属的Service。

### data_segment内存对象

Publisher的data_segment字段表示存储payload的共享内存数据，包括loan还未send的和已经send的到异步队列中的，包括任一消费端正在消费的和待消费的, 从data_segment的数据片段个数的代码能看出来。

```rust
// 计算publisher需要预留多少个payload片段
pub(crate) fn required_amount_of_samples_per_data_segment(
    &self,
    publisher_max_loaned_samples: usize,
) -> usize {
    match self {
        MessagingPattern::PublishSubscribe(v) => {
            v.max_subscribers
                * (v.subscriber_max_buffer_size + v.subscriber_max_borrowed_samples)
                + v.history_size
                + publisher_max_loaned_samples
                + 1   // 这里加1我不知道原因
        }
        _ => 0
    }
}
```

data_segment的类型Service::SharedMemory是通过类型重定义的，实际上就是上面章节中的`iceoryx2-cal::shared_memory::posix::Memory`内存对象类型。

### subscriber_connections连接对象

subscriber_connections表示持有的与同一个Service下的Subscriber的连接，连接是一个抽象的说法，底层是一种共同持有一个共享内存文件的机制来关联在一起。
关于connection的细节后面再单独小节展开。
subscriber_list_state表示SubsciberId的列表，数据来自Service下面的dynamic storage。
这个列表也是subscriber_connections构建的依据。

### subscriber_connections的动态更新

publisher在每次send数据之前，会去更新连接列表，之后才去遍历连接，尝试向每一条连接写入数据。

```rust
fn send_impl(&self, address_to_chunk: usize) -> Result<usize, ZeroCopyCreationError> {
    fail!(from self, when self.update_connections(),
        'Unable to send sample since the connections could not be updated.');
    self.add_to_history(address_to_chunk);
    Ok(self.deliver_sample(address_to_chunk))
}
/// Explicitly updates all connections to the [`crate::port::subscriber::Subscriber`]s. This is
/// required to be called whenever a new [`crate::port::subscriber::Subscriber`] connected to
/// the service. It is done implicitly whenever [`Publisher::send()`] or [`Publisher::send_copy()`]
/// is called.
pub fn update_connections(&self) -> Result<(), ZeroCopyCreationError> {
    if unsafe { (*self.subscriber_list_state.get()).update() } {
        fail!(from self, when self.populate_subscriber_channels(),
            'Connections were updated only partially since at least one connection to a Subscriber port failed.');
    }
    Ok(())
}
```

正如上面的代码注释所说，subscriber_connections连接对象正常应该是一个subscriber加入这个Service网络的时候被更新，不过现在是在每次send数据之前被更新。
更新的过程呢，就是根据service对象的dynamic_config信息，更新subscriber_list_state字段，之后根据subscriber_list_state字段更新subscriber_connections。

## Subscriber组件

```rust
/// The receiving endpoint of a publish-subscribe communication.
#[derive(Debug)]
pub struct Subscriber<'a, 'config: 'a, Service: service::Details<'config>, MessageType: Debug> {
    dynamic_config_guard: Option<UniqueIndex<'a>>,
    publisher_connections: PublisherConnections<'config, Service>,
    service: &'a Service,
    degration_callback: Option<DegrationCallback<'a>>,
    publisher_list_state: UnsafeCell<ContainerState<'a, UniquePublisherId>>,
    _phantom_message_type: PhantomData<MessageType>,
}
```

service表示此subscriber所属的Service。

### publisher_connections连接对象

publisher_connections表示持有的与同一个Service下的Subscriber的连接，连接是一个抽象的说法，底层是一种共同持有一个共享内存文件的机制来关联在一起。
关于connection的细节后面再单独小节展开。
publisher_list_state表示PublisherId的列表，数据来自Service下面的dynamic storage。
这个列表也是publisher_connections构建的依据。

### publisher_connections的动态更新

subscriber在每次receive数据之前，会去更新连接列表，之后才去遍历连接，尝试从每一条连接处读取数据。

```rust
pub fn receive<'subscriber>(
    &'subscriber self,
) -> Result<Option<Sample<'a, 'subscriber, 'config, Service, Header, MessageType>>, ReceiveError>
{
    if let Err(e) = self.update_connections() {
        fail!(from self,
            with ReceiveError::ConnectionFailure(e),
            'Some samples are not being received since not all connections to publishers could be established.');
    }
    for id in 0..self.publisher_connections.len() {
        match &mut self.publisher_connections.get_mut(id) {
            Some(ref mut connection) => {
                if let Some(sample) = self.receive_from_connection(id, connection)? {
                    return Ok(Some(sample));
                }
            }
            None => (),
        }
    }
    Ok(None)
}
/// Explicitly updates all connections to the [`crate::port::publisher::Publisher`]s. This is
/// required to be called whenever a new [`crate::port::publisher::Publisher`] connected to
/// the service. It is done implicitly whenever [`Subscriber::receive()`]
/// is called.
pub fn update_connections(&self) -> Result<(), ConnectionFailure> {
    if unsafe { (*self.publisher_list_state.get()).update() } {
        fail!(from self, when self.populate_publisher_channels(),
            'Connections were updated only partially since at least one connection to a publisher port failed.');
    }
    Ok(())
}
```

正如上面的代码注释所说，publisher_connections连接对象正常应该是一个publisher加入这个Service网络的时候被更新，不过现在是在每次receive数据之前被更新。
更新的过程呢，就是根据service对象的dynamic_config信息，更新publisher_list_state字段，之后根据publisher_list_state字段更新publisher_connections。

## 零拷贝连接实现

### 最新的零拷贝连接设计

iceoryx2的零拷贝连接实现也已经演进为更加模块化和可扩展的设计。基于最新的代码分析，我们可以看到：

#### 1. 零拷贝连接接口

```rust
pub trait ZeroCopyConnection: Debug + Sized + NamedConceptMgmt {
    type Sender: ZeroCopySender;
    type Receiver: ZeroCopyReceiver;
    type Builder: ZeroCopyConnectionBuilder<Self>;

    /// Removes the [`ZeroCopySender`] forcefully from the [`ZeroCopyConnection`]. This shall only
    /// be called when the [`ZeroCopySender`] died and the connection shall be cleaned up without
    /// causing any problems on the living [`ZeroCopyReceiver`] side.
    ///
    /// # Safety
    ///
    ///  * must ensure that the [`ZeroCopySender`] died while being connected.
    unsafe fn remove_sender(
        name: &FileName,
        config: &Self::Configuration,
    ) -> Result<(), ZeroCopyPortRemoveError>;

    /// Removes the [`ZeroCopyReceiver`] forcefully from the [`ZeroCopyConnection`]. This shall
    /// only be called when the [`ZeroCopySender`] died and the connection shall be cleaned up
    /// without causing any problems on the living [`ZeroCopySender`] side.
    ///
    /// # Safety
    ///
    ///  * must ensure that the [`ZeroCopyReceiver`] died while being connected.
    unsafe fn remove_receiver(
        name: &FileName,
        config: &Self::Configuration,
    ) -> Result<(), ZeroCopyPortRemoveError>;

    /// Returns true if the connection supports safe overflow
    fn does_support_safe_overflow() -> bool {
        false
    }

    /// Returns true if the buffer size of the connection can be configured
    fn has_configurable_buffer_size() -> bool {
        false
    }

    /// The default suffix of every zero copy connection
    fn default_suffix() -> FileName {
        unsafe { FileName::new_unchecked(b".rx") }
    }
}
```

#### 2. 发送者接口

```rust
pub trait ZeroCopySender: Debug + ZeroCopyPortDetails + NamedConcept + Send {
    fn try_send(
        &self,
        ptr: PointerOffset,
        sample_size: usize,
        channel_id: ChannelId,
    ) -> Result<Option<PointerOffset>, ZeroCopySendError>;

    fn blocking_send(
        &self,
        ptr: PointerOffset,
        sample_size: usize,
        channel_id: ChannelId,
    ) -> Result<Option<PointerOffset>, ZeroCopySendError>;

    fn reclaim(&self, channel_id: ChannelId)
        -> Result<Option<PointerOffset>, ZeroCopyReclaimError>;

    /// # Safety
    ///
    /// * must ensure that no receiver is still holding data, otherwise data races may occur on
    ///   receiver side
    /// * must ensure that [`ZeroCopySender::try_send()`] and [`ZeroCopySender::blocking_send()`]
    ///   are not called after using this method
    unsafe fn acquire_used_offsets<F: FnMut(PointerOffset)>(&self, callback: F);
}
```

#### 3. 接收者接口

```rust
pub trait ZeroCopyReceiver: Debug + ZeroCopyPortDetails + NamedConcept + Send {
    fn has_data(&self, channel_id: ChannelId) -> bool;
    fn receive(&self, channel_id: ChannelId)
        -> Result<Option<PointerOffset>, ZeroCopyReceiveError>;
    fn release(
        &self,
        ptr: PointerOffset,
        channel_id: ChannelId,
    ) -> Result<(), ZeroCopyReleaseError>;
    fn borrow_count(&self, channel_id: ChannelId) -> usize;
}
```

#### 4. 零拷贝连接构建器

```rust
pub trait ZeroCopyConnectionBuilder<C: ZeroCopyConnection>: NamedConceptBuilder<C> {
    fn buffer_size(self, value: usize) -> Self;
    fn enable_safe_overflow(self, value: bool) -> Self;
    fn receiver_max_borrowed_samples_per_channel(self, value: usize) -> Self;
    fn max_supported_shared_memory_segments(self, value: u8) -> Self;
    fn number_of_samples_per_segment(self, value: usize) -> Self;
    fn number_of_channels(self, value: usize) -> Self;
    fn initial_channel_state(self, value: u64) -> Self;
    /// The timeout defines how long the [`ZeroCopyConnectionBuilder`] should wait for
    /// concurrent
    /// [`ZeroCopyConnectionBuilder::create_sender()`] or
    /// [`ZeroCopyConnectionBuilder::create_receiver()`] call to finalize its initialization.
    /// By default it is set to [`Duration::ZERO`] for no timeout.
    fn timeout(self, value: Duration) -> Self;

    fn create_sender(self) -> Result<C::Sender, ZeroCopyCreationError>;
    fn create_receiver(self) -> Result<C::Receiver, ZeroCopyCreationError>;
}
```

### 零拷贝连接原理

在iceoryx2中，publisher和subscriber之间的数据传输是通过zero_copy_connection来实现的，底层原理是：

1. **共享内存文件**：publisher和subscriber共同持有一个共享内存文件，用于传递数据地址
2. **环形队列**：使用环形队列（IndexQueue）在共享内存中实现数据地址的传递
3. **双通道设计**：
   - `receive_channel`：存放publisher新发布的数据地址
   - `retrieve_channel`：存放subscriber消费完待释放的数据地址

### 通信过程

1. **数据发布**：
   - Publisher从data_segment分配内存
   - 填充数据
   - 通过Sender将数据地址写入connection的receive_channel
   - Subscriber从receive_channel读取数据地址

2. **数据接收**：
   - Subscriber从connection的receive_channel获取数据地址
   - 根据地址从data_segment读取实际数据
   - 处理完毕后，将地址写回retrieve_channel

3. **内存回收**：
   - Publisher从retrieve_channel获取已消费的数据地址
   - 减少引用计数
   - 当引用计数为0时，将内存释放回data_segment

### 关系图

![publisher-subscriber-connection]()

可以看到：
1. Publisher和Subscriber都会关联一个Service
2. 一个Publisher和一个Subscriber之间通过两个共享内存文件关联：
   - connection文件：存放传递中的数据地址
   - data_segment文件：存放数据payload本身
3. 一个publisher只有一个data_segment文件，却有多个connection文件（每个subscriber一个）
4. 一个subscriber有相同个数的data_segment文件和connection文件（每个publisher一个）

这种设计实现了真正的零拷贝通信，数据只需要在共享内存中存储一次，发布者和订阅者通过传递地址来访问数据，避免了数据拷贝的开销。

## 参考资料

- Introduction：iceoryx2架构相关
- Layered Architecture：iceoryx2分层架构相关

---

使用方法：
1. 全选复制上面所有内容
2. 新建文本文件，粘贴
3. 将文件后缀改为 `.md`（如 `iceoryx2原理解析.md`）
4. 可直接在 Typora、VSCode、Obsidian 等编辑器中打开预览