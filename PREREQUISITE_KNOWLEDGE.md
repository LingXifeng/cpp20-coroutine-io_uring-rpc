# 📚 理解 RPC 项目的前置知识清单

> 按项目模块组织，每个知识点标注：🟢看博客 / 🟡看+手写 / 🔴必须手写

---

## 一、协程模块（coroutine/）

### 🟢 看博客就够的

| 知识点 | 搜什么关键词 | 时长 | 要搞懂到什么程度 |
|--------|------------|------|----------------|
| 协程 vs 线程 vs 回调 | "协程 线程 回调 区别" | 20min | 各自优缺点，协程为什么轻量 |
| 协程分类：有栈/无栈 | "stackful vs stackless coroutine" | 15min | C++20 是无栈协程，和 Go 有栈协程区别 |
| co_await / co_yield / co_return 语义 | "C++20 coroutine keywords" | 15min | 每个关键字做什么，编译器怎么翻译 |
| 协程的优缺点 | "C++20 coroutine pros cons" | 10min | 零开销但调试难，无栈但需手动管理生命周期 |

### 🟡 看博客 + 手写一遍的

| 知识点 | 手写什么 | 时长 | 要搞懂到什么程度 |
|--------|---------|------|----------------|
| **promise_type 5 个接口** | 手写一个最简协程（返回 int） | 1h | 每个接口什么时候被编译器调用 |
| **co_await 挂起/恢复机制** | 手写一个 awaitable，控制挂起时机 | 1h | await_suspend 返回 false/true/handle 分别什么效果 |
| **协程帧和生命周期** | 手写一个协程，打印各步骤顺序 | 30min | 协程帧在堆上，谁分配谁释放 |

### 🔴 必须手写的

| 知识点 | 手写什么 | 时长 | 要搞懂到什么程度 |
|--------|---------|------|----------------|
| **完整协程调度器** | Task\<T\> + Scheduler + sync_wait | 3h | 能跑通 co_await 链式调用，能脱稿写 |

---

## 二、io_uring 模块（io/）

### 🟢 看博客就够的

| 知识点 | 搜什么关键词 | 时长 | 要搞懂到什么程度 |
|--------|------------|------|----------------|
| io_uring vs epoll | "io_uring vs epoll performance" | 20min | 批量提交省 syscall，共享内存省拷贝 |
| SQE / CQE 结构 | "io_uring SQE CQE structure" | 15min | SQE 是请求，CQE 是结果，字段含义 |
| SQPOLL | "io_uring SQPOLL" | 10min | 内核线程轮询，省 io_uring_enter 调用 |
| register_files / register_buffers | "io_uring registered files buffers" | 10min | 固定表减少内核查表，零拷贝前提 |
| linked SQE / timeout | "io_uring linked timeout" | 10min | 请求关联，超时自动取消 |

### 🟡 看博客 + 手写一遍的

| 知识点 | 手写什么 | 时长 | 要搞懂到什么程度 |
|--------|---------|------|----------------|
| **io_uring 初始化** | 手写 io_uring_setup + mmap | 1h | SQ/CQ 共享内存布局，head/tail 指针 |
| **SQ/CQ 环形缓冲区** | 手写提交一个 read + 取结果 | 1h | 应用写 SQ tail，内核写 CQ tail，acquire/release 同步 |

### 🔴 必须手写的

| 知识点 | 手写什么 | 时长 | 要搞懂到什么程度 |
|--------|---------|------|----------------|
| **io_uring echo server** | accept → read → write 完整循环 | 3h | 不看任何代码，从空文件写出能跑的版本 |

**推荐资料**：
- 博客：搜 "io_uring by example" 或 "io_uring 入门"
- 内核文档：`man io_uring_setup` / `man io_uring_enter`
- 源码：liburing 源码（`github.com/axboe/liburing`），看 `queue.c` 和 `setup.c`

---

## 三、网络模块（net/）

### 🟢 看博客就够的

| 知识点 | 搜什么关键词 | 时长 | 要搞懂到什么程度 |
|--------|------------|------|----------------|
| TCP 三次握手/四次挥手 | "TCP 三次握手 四次挥手" | 20min | 画时序图，能讲清每个状态 |
| TCP 粘包/拆包 | "TCP 粘包 解决方案" | 15min | 长度前缀法，你的项目就是这么做的 |
| Reactor / Proactor 模式 | "Reactor Proactor 区别" | 15min | Reactor 同步，Proactor 异步，io_uring 是 Proactor |
| epoll LT vs ET | "epoll LT ET 区别" | 15min | ET 必须读到 EAGAIN，LT 每次通知 |
| 零拷贝 | "零拷贝 sendfile mmap splice" | 15min | 数据不经过用户态，减少拷贝 |
| TIME_WAIT | "TCP TIME_WAIT 原因 解决" | 10min | 2MSL 等待，端口复用 |
| backlog | "TCP listen backlog somaxconn" | 10min | 全连接队列/半连接队列 |

### 🟡 看博客 + 手写一遍的

| 知识点 | 手写什么 | 时长 | 要搞懂到什么程度 |
|--------|---------|------|----------------|
| **Socket API** | 手写一个最简 TCP echo（阻塞版） | 1h | socket/bind/listen/accept/read/write 流程 |
| **epoll echo server** | 手写一个 epoll ET 模式 echo | 2h | 事件驱动的基本模式 |

---

## 四、Buffer 和序列化

### 🟢 看博客就够的

| 知识点 | 搜什么关键词 | 时长 | 要搞懂到什么程度 |
|--------|------------|------|----------------|
| 内存对齐 | "C++ 内存对齐 alignas" | 15min | struct 大小怎么算，false sharing |
| 字节序（大端/小端） | "大端序 小端序 网络字节序" | 10min | 网络用大端，x86 用小端，htonl/ntohl |
| Protobuf 编码原理 | "protobuf varint encoding" | 20min | varint 编码，你的项目用了类似方案 |

### 🟡 看博客 + 手写一遍的

| 知识点 | 手写什么 | 时长 | 要搞懂到什么程度 |
|--------|---------|------|----------------|
| **varint 编解码** | 手写 encode_varint / decode_varint | 30min | 小数字省空间，大数字多字节 |
| **链式 Buffer** | 手写 BufferBlock 链表 + append + read | 1h | 动态增长不拷贝，零拷贝读取 |

---

## 五、RPC 模块（rpc/）

### 🟢 看博客就够的

| 知识点 | 搜什么关键词 | 时长 | 要搞懂到什么程度 |
|--------|------------|------|----------------|
| RPC 原理 | "RPC 原理 调用流程" | 20min | 代理 → 序列化 → 网络 → 反序列化 → 执行 |
| IDL 是什么 | "IDL 接口定义语言 protobuf" | 15min | 用 IDL 定义接口，自动生成代码 |
| 服务注册/发现 | "服务注册发现 etcd consul" | 15min | 你的项目用静态注册，知道动态注册怎么做的 |
| 负载均衡策略 | "负载均衡 轮询 一致性哈希 最少连接" | 20min | 3 种策略的原理和适用场景 |
| 连接池原理 | "连接池 为什么需要" | 10min | 复用连接，避免频繁建连 |

### 🟡 看博客 + 手写一遍的

| 知识点 | 手写什么 | 时长 | 要搞懂到什么程度 |
|--------|---------|------|----------------|
| **一致性哈希** | 手写一致性哈希 + 虚拟节点 | 1h | 为什么需要虚拟节点，增减节点影响范围 |

---

## 六、并发与多线程

### 🟢 看博客就够的

| 知识点 | 搜什么关键词 | 时长 | 要搞懂到什么程度 |
|--------|------------|------|----------------|
| mutex / lock_guard / unique_lock | "C++ mutex 用法" | 15min | RAII 加锁，lock_guard 和 unique_lock 区别 |
| condition_variable | "C++ condition_variable 生产者消费者" | 15min | wait/notify_one/notify_all |
| 死锁条件 | "死锁 四个必要条件" | 10min | 互斥/占有/不可抢占/循环等待 |
| false sharing | "false sharing cache line" | 15min | 不同线程写同一 cache line，加 padding 解决 |
| ABA 问题 | "ABA problem CAS" | 10min | 值从 A→B→A，CAS 误判，用版本号解决 |

### 🟡 看博客 + 手写一遍的

| 知识点 | 手写什么 | 时长 | 要搞懂到什么程度 |
|--------|---------|------|----------------|
| **线程池** | 手写一个固定大小线程池 | 1.5h | submit 任务 → 线程取任务 → 条件变量等待 |
| **自旋锁** | 手写 spin_lock（atomic + CAS） | 30min | 忙等 vs 让出 CPU，适用场景 |
| **生产者消费者队列** | 手写线程安全的有界队列 | 1h | mutex + condvar，满/空时阻塞 |

### 🔴 必须手写的

| 知识点 | 手写什么 | 时长 | 要搞懂到什么程度 |
|--------|---------|------|----------------|
| **memory_order** | 手写 seq_cst / acquire-release / relaxed 示例 | 1h | 三种内存序的区别，什么时候用哪种 |

---

## 七、C++ 语言特性

### 🟢 看博客就够的

| 知识点 | 搜什么关键词 | 时长 | 要搞懂到什么程度 |
|--------|------------|------|----------------|
| 右值引用 / 移动语义 | "C++ 右值引用 移动语义" | 20min | 避免深拷贝，std::move 的本质 |
| 完美转发 | "C++ 完美转发 forward" | 15min | 保持值类别，forward vs move |
| RAII | "C++ RAII 原理" | 10min | 构造获取，析构释放，栈展开保证 |
| SFINAE / concepts | "C++20 concepts SFINAE" | 15min | 编译期条件约束，你的项目用了 |
| intrusive 容器 | "intrusive list boost" | 10min | 节点嵌入对象内，无额外分配 |

### 🟡 看博客 + 手写一遍的

| 知识点 | 手写什么 | 时长 | 要搞懂到什么程度 |
|--------|---------|------|----------------|
| **shared_ptr / weak_ptr** | 手写简化版 shared_ptr | 1h | 引用计数 + 控制块 + 线程安全 |
| **unique_ptr** | 手写简化版 unique_ptr | 30min | 移动语义，不可拷贝 |

---

## 八、Linux 系统编程

### 🟢 看博客就够的

| 知识点 | 搜什么关键词 | 时长 | 要搞懂到什么程度 |
|--------|------------|------|----------------|
| 文件描述符 | "Linux 文件描述符" | 10min | 一切皆文件，fd 表，ulimit -n |
| 用户态 / 内核态 | "用户态 内核态 切换开销" | 15min | syscall 开销 ~几百ns，io_uring 省在哪 |
| mmap | "mmap 原理 共享内存" | 15min | 文件/匿名映射，和 io_uring 共享内存的关系 |
| 信号处理 | "Linux signal SIGINT SIGTERM" | 10min | 优雅关闭需要 |
| /proc 文件系统 | "Linux proc fd limits" | 5min | 查 fd 数、内存等 |

---

## 总时间汇总

| 类别 | 数量 | 总时间 |
|------|------|--------|
| 🟢 看博客 | 30 个 | **~7h** |
| 🟡 看+手写 | 14 个 | **~14h** |
| 🔴 必须手写 | 3 个 | **~7h** |
| **合计** | **47 个知识点** | **~28h** |

**28 小时，4-5 天集中搞定。**

---

## 每天怎么安排

### Day 1（6h）：协程 + io_uring 相关

```
上午（3h）：
  🟢 协程概念 4 个          1h
  🟡 promise_type + co_await  2h

下午（3h）：
  🟢 io_uring 概念 5 个     1h
  🟡 io_uring 初始化 + SQ/CQ 2h
```

### Day 2（6h）：网络 + Buffer + 序列化

```
上午（3h）：
  🟢 网络概念 8 个           2h
  🟡 Socket API 手写         1h

下午（3h）：
  🟢 Buffer/序列化概念 3 个  0.5h
  🟡 varint + 链式 Buffer    1.5h
  🟡 epoll echo server       1h
```

### Day 3（6h）：RPC + 并发 + C++ 特性

```
上午（3h）：
  🟢 RPC 概念 5 个           1.5h
  🟡 一致性哈希              1h
  🟢 并发概念 5 个           0.5h

下午（3h）：
  🟡 线程池 + 自旋锁         2h
  🟡 生产者消费者队列        1h
```

### Day 4（6h）：C++ 特性 + 系统编程 + 必须手写

```
上午（3h）：
  🟢 C++ 特性 5 个           1h
  🟡 shared_ptr + unique_ptr 1.5h
  🟢 Linux 系统编程 5 个     0.5h

下午（3h）：
  🔴 memory_order 手写       1h
  🔴 协程调度器手写          2h
```

### Day 5（4h）：终极手写

```
  🔴 io_uring echo server 手写  3h
  复习 + 查漏补缺              1h
```

---

## 资料推荐

### 博客（🟢 用这些就够了）

| 主题 | 搜什么 | 备选 |
|------|--------|------|
| 协程 | "C++20 Coroutines: The complete guide" | cppreference.com/coroutines |
| io_uring | "io_uring by example" | unixism.net/loti |
| 网络 | "TCP/IP 详解 卷1" 笔记 | 《图解 TCP/IP》 |
| 并发 | "C++ Concurrency in Action" 笔记 | cppreference.com/atomic |
| C++ 特性 | "C++ 移动语义完美转发" | cppreference.com |

### 手写参考（🟡🔴 卡住时看一眼）

| 主题 | 参考什么 |
|------|---------|
| 协程调度器 | 你项目的 `coroutine/task.hpp` + `scheduler.hpp` |
| io_uring echo | 你项目的 `io/io_uring.hpp` + `examples/echo_server.cpp` |
| 线程池 | 你项目的 `io/event_loop_pool.hpp` |
| Buffer | 你项目的 `net/buffer.hpp` |
| 序列化 | 你项目的 `rpc/rpc_server.hpp` 里的序列化部分 |

> **优先看你自己的代码**，有上下文，比看陌生代码快 10 倍。
