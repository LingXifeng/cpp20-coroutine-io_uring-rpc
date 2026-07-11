# 🎯 RPC 框架学习进度检查清单

> 基于 `rpc_learning_roadmap.md` 路线文档，零基础复刻 C++20 协程 + io_uring 高性能 RPC 框架的逐项进度追踪表。
> 
> **使用方法**：每完成一项，将 `[ ]` 改为 `[x]`，并在 `完成日期` 列填写日期。
> **状态标记**：✅ 已完成 | 🔄 进行中 | ⏳ 未开始 | ⏭️ 跳过

---

## 📊 总览仪表盘

| 维度 | 进度 | 说明 |
|------|------|------|
| Phase 0 前置基础 | 0 / 42 项 | C++/Linux/网络/工具链 |
| Phase 1 协程引擎 | 0 / 18 项 | coroutine 模块 |
| Phase 2 IO 引擎 | 0 / 16 项 | io_uring + event_loop |
| Phase 3 网络层 | 0 / 14 项 | TCP/RPC/连接池 |
| Phase 4 高级特性 | 0 / 12 项 | 中间件/超时/多线程 |
| Phase 5 测试与优化 | 0 / 10 项 | benchmark/ASan/文档 |
| Phase 6 面试与展示 | 0 / 8 项 | 面试题/简历/开源 |
| **总计** | **0 / 120 项** | **0%** |

---

## Phase 0：前置基础（预计 104h）

### 0.1 C++ 核心（20h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 1 | 理解 RAII 原则，能手写 unique_ptr / shared_ptr 简化版 | ⏳ | | |
| 2 | 掌握移动语义：std::move、右值引用、完美转发 | ⏳ | | |
| 3 | 理解 SFINAE 与 type_traits 常用工具 | ⏳ | | |
| 4 | 掌握可变参数模板 + fold expression | ⏳ | | |
| 5 | 理解 std::tuple 与 std::apply 的原理 | ⏳ | | |
| 6 | 掌握 std::function / std::any 的底层机制 | ⏳ | | |
| 7 | 理解内存对齐、placement new、allocator | ⏳ | | |
| 8 | 能手写 thread-safe 的 SPSC 环形缓冲区 | ⏳ | | |

### 0.2 C++20 协程前置（12h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 9 | 理解协程 vs 线程 vs 回调的本质区别 | ⏳ | | |
| 10 | 掌握 co_await / co_yield / co_return 语义 | ⏳ | | |
| 11 | 理解 promise_type 的 6 个核心接口 | ⏳ | | |
| 12 | 能手写一个 generator 协程 | ⏳ | | |
| 13 | 理解 awaiter 的 3 个方法：await_ready / await_suspend / await_resume | ⏳ | | |
| 14 | 理解协程帧的内存布局与生命周期 | ⏳ | | |
| 15 | 理解对称转移（symmetric transfer）与 noop_coroutine | ⏳ | | |

### 0.3 Linux 系统编程（18h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 16 | 理解文件描述符、epoll LT/ET 模式 | ⏳ | | |
| 17 | 掌握 socket API：bind/listen/accept/connect/send/recv | ⏳ | | |
| 18 | 理解 TCP 三次握手/四次挥手在内核中的状态机 | ⏳ | | |
| 19 | 理解 REUSEADDR / REUSEPORT / TCP_NODELAY / CORK | ⏳ | | |
| 20 | 掌握非阻塞 IO + IO 多路复用的事件循环模式 | ⏳ | | |
| 21 | 理解 io_uring 基本原理：SQ/CQ/Kernel 共享环 | ⏳ | | |
| 22 | 能用 liburing 写一个 echo server | ⏳ | | |
| 23 | 理解信号处理、timerfd、eventfd | ⏳ | | |
| 24 | 掌握 mmap / mprotect / huge pages 概念 | ⏳ | | |

### 0.4 网络编程（16h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 25 | 理解 Reactor / Proactor 模式 | ⏳ | | |
| 26 | 理解 Length-Field 协议帧编解码 | ⏳ | | |
| 27 | 掌握 Protobuf / FlatBuffers 序列化原理 | ⏳ | | |
| 28 | 理解 RPC 调用链：stub → serialize → transport → deserialize → skeleton | ⏳ | | |
| 29 | 理解连接池、负载均衡（RR/一致性哈希） | ⏳ | | |
| 30 | 理解服务发现与注册的基本模型 | ⏳ | | |

### 0.5 构建与工具链（10h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 31 | 掌握 CMake 基础：add_library / target_link / FetchContent | ⏳ | | |
| 32 | 能写 CMakePresets.json 配置多编译模式 | ⏳ | | |
| 33 | 掌握 GTest 框架：TEST / EXPECT / ASSERT / TYPED_TEST | ⏳ | | |
| 34 | 理解 Google Benchmark 用法 | ⏳ | | |
| 35 | 掌握 clang-format + clang-tidy 配置 | ⏳ | | |
| 36 | 能配置 GitHub Actions CI 流水线 | ⏳ | | |
| 37 | 理解 AddressSanitizer / UBSan / TSan 原理与使用 | ⏳ | | |

### 0.6 设计模式与架构（8h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 38 | 理解生产者-消费者、对象池、零拷贝 | ⏳ | | |
| 39 | 理解 Type Erasure / CRTP / EBO | ⏳ | | |
| 40 | 理解分层架构：IO → 网络 → 协议 → RPC | ⏳ | | |
| 41 | 理解中间件/拦截器链模式 | ⏳ | | |
| 42 | 能画本项目模块依赖图 | ⏳ | | |

### 🔍 Phase 0 检查点

- [ ] **CP0-1**：能手写一个基于 epoll 的 echo server（不借助框架）
- [ ] **CP0-2**：能用 liburing 写一个 io_uring echo server
- [ ] **CP0-3**：能手写一个简单的 C++20 generator 协程
- [ ] **CP0-4**：能用 CMake + GTest 搭建一个可编译测试的项目骨架

---
## Phase 1：协程引擎（预计 30h）

### 1.1 Task 协程类型（10h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 43 | 实现 Task<T> 的 promise_type（get_return_object / initial_suspend / final_suspend） | ⏳ | | |
| 44 | 实现 Task<T> 的 await_transform（支持 co_await 嵌套 Task） | ⏳ | | |
| 45 | 实现 Task 的对称转移（final_suspend 返回 coroutine_handle） | ⏳ | | |
| 46 | 处理 void / non-void 返回值类型 | ⏳ | | |
| 47 | 处理异常传播：promise_type::unhandled_exception | ⏳ | | |

### 1.2 调度器（8h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 48 | 实现 Scheduler 类：run / enqueue / schedule | ⏳ | | |
| 49 | 理解协程挂起恢复与调度器的交互 | ⏳ | | |
| 50 | 实现协程的定时调度（delay / at） | ⏳ | | |
| 51 | 实现 yield / sleep 协程操作 | ⏳ | | |

### 1.3 生成器与通道（6h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 52 | 实现 Generator<T>（co_yield 支持） | ⏳ | | |
| 53 | 实现 Channel<T>（协程间通信） | ⏳ | | |
| 54 | 实现 Select（多通道选择） | ⏳ | | |

### 1.4 协程工具（6h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 55 | 实现 WhenAll（并发等待多个 Task） | ⏳ | | |
| 56 | 实现 WhenAny（竞速等待） | ⏳ | | |
| 57 | 实现 WithTimeout（超时包装器） | ⏳ | | |
| 58 | 实现协程的 cancel 机制 | ⏳ | | |

### 🔍 Phase 1 检查点

- [ ] **CP1-1**：`Task<int> add(int a, int b)` 能正确 co_return a+b
- [ ] **CP1-2**：嵌套 Task 调用 co_await 正常传播返回值与异常
- [ ] **CP1-3**：Scheduler 跑 1000 个协程交替调度无死锁
- [ ] **CP1-4**：WhenAll 并发 100 个 Task 全部完成
- [ ] **CP1-5**：WithTimeout 超时后协程被正确取消

---

## Phase 2：IO 引擎（预计 40h）

### 2.1 io_uring 封装（15h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 59 | 实现 IoUring 类：init / submit / wait_cqe / peek_cqe | ⏳ | | |
| 60 | 封装 prep_accept / prep_recv / prep_send / prep_connect | ⏳ | | |
| 61 | 实现 SQ 批量提交 + CQ 批量收割 | ⏳ | | |
| 62 | 实现 CQE callback 注册与分发 | ⏳ | | |
| 63 | 理解 io_uring 固定文件注册（register_files） | ⏳ | | |
| 64 | 理解 io_uring 固定缓冲区注册（register_buffers） | ⏳ | | |

### 2.2 EventLoop（12h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 65 | 实现 EventLoop 单例：run / run_once / stop | ⏳ | | |
| 66 | 集成 io_uring 到 EventLoop 事件循环 | ⏳ | | |
| 67 | 实现定时器管理：add_timer / cancel_timer | ⏳ | | |
| 68 | 实现 TimingWheel 高效定时器轮 | ⏳ | | |
| 69 | 实现协程 IO 操作：async_accept / async_recv / async_send / async_connect | ⏳ | | |

### 2.3 Buffer 管理（8h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 70 | 实现 BufferBlock（固定大小内存块） | ⏳ | | |
| 71 | 实现 BufferChain（链式缓冲区，支持零拷贝读取） | ⏳ | | |
| 72 | 实现 BufferPool（对象池复用） | ⏳ | | |
| 73 | 实现 Buffer 的 read/write/compact 操作 | ⏳ | | |

### 2.4 多线程 EventLoop（5h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 74 | 实现 EventLoopPool：多线程多 io_uring 实例 | ⏳ | | |
| 75 | 实现 Round-Robin / Least-Loaded 分配策略 | ⏳ | | |
| 76 | 理解线程间协程迁移（steal work） | ⏳ | | |

### 🔍 Phase 2 检查点

- [ ] **CP2-1**：io_uring echo server 跑通，客户端能收发数据
- [ ] **CP2-2**：EventLoop 跑 10s 无崩溃无泄漏
- [ ] **CP2-3**：async_recv + async_send 协程化 echo 跑通
- [ ] **CP2-4**：TimingWheel 定时器精度 < 1ms
- [ ] **CP2-5**：BufferChain 零拷贝读取验证
- [ ] **CP2-6**：EventLoopPool 多线程负载均衡验证

---
## Phase 3：网络与 RPC 层（预计 45h）

### 3.1 TCP 连接（10h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 77 | 实现 TcpStream：封装 fd + async_read / async_write | ⏳ | | |
| 78 | 实现 TcpListener：async_accept 循环 | ⏳ | | |
| 79 | 实现连接生命周期管理（close / reset / half-close） | ⏳ | | |
| 80 | 实现 TcpConnector：async_connect + 超时 | ⏳ | | |

### 3.2 协议编解码（10h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 81 | 实现 Serializer：二进制序列化/反序列化基础类型 | ⏳ | | |
| 82 | 实现变长整数编码（VarInt） | ⏳ | | |
| 83 | 实现 RpcHeader：magic + version + type + seq_id + body_len | ⏳ | | |
| 84 | 实现 LengthFieldFrameDecoder：解决 TCP 粘包/拆包 | ⏳ | | |
| 85 | 实现 RpcMessage 完整编解码 | ⏳ | | |

### 3.3 RPC 核心（15h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 86 | 实现 RpcStub：客户端代理生成 + 远程调用 | ⏳ | | |
| 87 | 实现 RpcSkeleton：服务端分发 + 方法注册 | ⏳ | | |
| 88 | 实现 RpcResult<T>：统一返回值 + 错误码 | ⏳ | | |
| 89 | 实现 ServiceRegistry：服务名 → 方法表映射 | ⏳ | | |
| 90 | 实现 IDL 代码生成器（.proto → C++ stub/skeleton） | ⏳ | | |
| 91 | 实现异步 RPC 调用：call_async / call_with_timeout | ⏳ | | |

### 3.4 连接池与负载均衡（10h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 92 | 实现 ConnectionPool：get / release / health_check | ⏳ | | |
| 93 | 实现 RoundRobin 负载均衡 | ⏳ | | |
| 94 | 实现 ConsistentHash 负载均衡 | ⏳ | | |
| 95 | 实现 WeightedRoundRobin 负载均衡 | ⏳ | | |
| 96 | 实现 LeastConnections 负载均衡 | ⏳ | | |

### 🔍 Phase 3 检查点

- [ ] **CP3-1**：TcpStream 收发 1MB 数据无丢失
- [ ] **CP3-2**：RpcHeader + body 编解码往返一致
- [ ] **CP3-3**：客户端 stub 调用服务端 skeleton 方法，返回值正确
- [ ] **CP3-4**：ConnectionPool 并发 100 连接无泄漏
- [ ] **CP3-5**：负载均衡 4 节点 10000 次调用分布均匀

---

## Phase 4：高级特性（预计 30h）

### 4.1 超时与重试（8h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 97 | 实现 TimedRecv / TimedSend：IO 操作超时控制 | ⏳ | | |
| 98 | 实现 RPC 调用超时：call_with_timeout | ⏳ | | |
| 99 | 实现指数退避重试策略 | ⏳ | | |
| 100 | 实现超时后的资源清理（取消 io_uring 请求） | ⏳ | | |

### 4.2 中间件/拦截器（8h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 101 | 实现 Middleware 基类 + 拦截器链 | ⏳ | | |
| 102 | 实现 LoggingMiddleware：请求/响应日志 | ⏳ | | |
| 103 | 实现 MetricsMiddleware：QPS / 延迟统计 | ⏳ | | |
| 104 | 实现 RateLimitMiddleware：令牌桶限流 | ⏳ | | |

### 4.3 io_uring 高级特性（8h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 105 | 实现 register_files / register_buffers | ⏳ | | |
| 106 | 实现 SQPOLL 模式（内核线程轮询） | ⏳ | | |
| 107 | 实现 provided_buffers 零拷贝接收 | ⏳ | | |
| 108 | 理解 io_uring 多环实例 + IOPOLL | ⏳ | | |

### 4.4 服务发现（6h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 109 | 实现 ServiceDiscovery 接口 | ⏳ | | |
| 110 | 实现静态配置发现（JSON/文件） | ⏳ | | |
| 111 | 理解与 etcd / Consul 集成方案 | ⏳ | | |
| 112 | 实现服务健康检查 + 自动摘除 | ⏳ | | |

### 🔍 Phase 4 检查点

- [ ] **CP4-1**：RPC 调用超时 100ms 后正确返回超时错误
- [ ] **CP4-2**：中间件链 Logging → Metrics → RateLimit 正确执行
- [ ] **CP4-3**：register_files 后 IO 性能提升可测量
- [ ] **CP4-4**：服务摘除后连接池不再分配到该节点

---

## Phase 5：测试与优化（预计 20h）

### 5.1 单元测试（6h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 113 | Buffer 单元测试全部通过 | ⏳ | | |
| 114 | Serializer 单元测试全部通过 | ⏳ | | |
| 115 | Coroutine 单元测试全部通过 | ⏳ | | |
| 116 | ConnectionPool + LoadBalancer 单元测试通过 | ⏳ | | |
| 117 | IDL parser 单元测试通过 | ⏳ | | |

### 5.2 端到端测试（4h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 118 | 完整 RPC 调用链测试通过 | ⏳ | | |
| 119 | 协程 + io_uring TCP echo 端到端通过 | ⏳ | | |
| 120 | 多线程 EventLoop 端到端通过 | ⏳ | | |

### 5.3 性能 Benchmark（4h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 121 | 协程调度 QPS benchmark 跑通 | ⏳ | | |
| 122 | 序列化 QPS benchmark 跑通 | ⏳ | | |
| 123 | Buffer 操作 QPS benchmark 跑通 | ⏳ | | |
| 124 | RPC 查找 QPS benchmark 跑通 | ⏳ | | |

### 5.4 安全与质量（6h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 125 | ASan + UBSan 全量测试通过，0 内存泄漏 | ⏳ | | |
| 126 | clang-format 全量格式化通过 | ⏳ | | |
| 127 | clang-tidy 零 warning（或已知豁免） | ⏳ | | |
| 128 | GitHub Actions CI 流水线跑通 | ⏳ | | |

### 🔍 Phase 5 检查点

- [ ] **CP5-1**：所有 9 个测试套件通过（38 个用例）
- [ ] **CP5-2**：ASan + UBSan 零泄漏
- [ ] **CP5-3**：Benchmark 数据记录到 PERFORMANCE_REPORT.md
- [ ] **CP5-4**：CI 绿灯

---
## Phase 6：面试准备与项目展示（预计 15h）

### 6.1 面试知识整理（6h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 129 | 完成 INTERVIEW_QA.md 40 题全部理解，能脱稿回答 | ⏳ | | |
| 130 | 能画协程调度流程图并讲解 | ⏳ | | |
| 131 | 能画 io_uring 工作原理图并讲解 | ⏳ | | |
| 132 | 能对比 epoll vs io_uring 性能与编程模型差异 | ⏳ | | |

### 6.2 简历与展示（5h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 133 | 简历项目描述写好（含量化数据） | ⏳ | | |
| 134 | 架构图 + 模块依赖图绘制完成 | ⏳ | | |
| 135 | GitHub 仓库 README 完善（badge + benchmark + 快速上手） | ⏳ | | |

### 6.3 开源准备（4h）

| # | 检查项 | 状态 | 完成日期 | 备注 |
|---|--------|------|----------|------|
| 136 | LICENSE（MIT）已添加 | ⏳ | | |
| 137 | CONTRIBUTING.md 编写 | ⏳ | | |
| 138 | 代码全量格式化 + CI 绿灯 | ⏳ | | |

### 🔍 Phase 6 检查点

- [ ] **CP6-1**：模拟面试：能在 5 分钟内讲清项目架构
- [ ] **CP6-2**：模拟面试：能回答"为什么用 io_uring 而非 epoll"
- [ ] **CP6-3**：模拟面试：能回答"C++20 协程的内存开销与调度开销"
- [ ] **CP6-4**：GitHub 仓库可 clone 后 cmake --build 一次通过

---

## 📋 检查点汇总（共 27 个）

| 检查点 | 所属 Phase | 描述 |
|--------|-----------|------|
| CP0-1 | Phase 0 | 手写 epoll echo server |
| CP0-2 | Phase 0 | 手写 io_uring echo server |
| CP0-3 | Phase 0 | 手写 C++20 generator 协程 |
| CP0-4 | Phase 0 | CMake + GTest 项目骨架 |
| CP1-1 | Phase 1 | Task<int> 正确 co_return |
| CP1-2 | Phase 1 | 嵌套 Task 调用传播 |
| CP1-3 | Phase 1 | 1000 协程调度无死锁 |
| CP1-4 | Phase 1 | WhenAll 并发完成 |
| CP1-5 | Phase 1 | WithTimeout 超时取消 |
| CP2-1 | Phase 2 | io_uring echo server 跑通 |
| CP2-2 | Phase 2 | EventLoop 稳定运行 |
| CP2-3 | Phase 2 | 协程化 echo 跑通 |
| CP2-4 | Phase 2 | TimingWheel 精度验证 |
| CP2-5 | Phase 2 | BufferChain 零拷贝验证 |
| CP2-6 | Phase 2 | 多线程负载均衡验证 |
| CP3-1 | Phase 3 | TcpStream 大数据收发 |
| CP3-2 | Phase 3 | 编解码往返一致 |
| CP3-3 | Phase 3 | RPC 调用返回值正确 |
| CP3-4 | Phase 3 | 连接池并发无泄漏 |
| CP3-5 | Phase 3 | 负载均衡分布均匀 |
| CP4-1 | Phase 4 | RPC 超时正确返回 |
| CP4-2 | Phase 4 | 中间件链正确执行 |
| CP4-3 | Phase 4 | register_files 性能提升 |
| CP4-4 | Phase 4 | 服务摘除验证 |
| CP5-1 | Phase 5 | 全量测试通过 |
| CP5-2 | Phase 5 | ASan 零泄漏 |
| CP5-3 | Phase 5 | Benchmark 数据记录 |
| CP5-4 | Phase 5 | CI 绿灯 |
| CP6-1 | Phase 6 | 5 分钟讲清架构 |
| CP6-2 | Phase 6 | io_uring vs epoll 对比 |
| CP6-3 | Phase 6 | 协程开销分析 |
| CP6-4 | Phase 6 | 一键编译通过 |

---

## 🕐 学习日志模板

> 每次学习后记录，便于回顾与调整节奏。

| 日期 | Phase | 完成项 # | 耗时 | 遇到的问题 | 解决方式 |
|------|-------|----------|------|------------|----------|
| YYYY-MM-DD | Px | #n | Xh | | |
| | | | | | |

---

## 🏆 里程碑

| 里程碑 | 触发条件 | 目标日期 |
|--------|----------|----------|
| 🎯 M1：前置知识就绪 | Phase 0 全部 ✅ + 4 个检查点通过 | |
| 🎯 M2：协程引擎可用 | Phase 1 全部 ✅ + 5 个检查点通过 | |
| 🎯 M3：IO 引擎可用 | Phase 2 全部 ✅ + 6 个检查点通过 | |
| 🎯 M4：RPC 可调用 | Phase 3 全部 ✅ + 5 个检查点通过 | |
| 🎯 M5：高级特性完成 | Phase 4 全部 ✅ + 4 个检查点通过 | |
| 🎯 M6：测试全绿 | Phase 5 全部 ✅ + 4 个检查点通过 | |
| 🎯 M7：面试就绪 | Phase 6 全部 ✅ + 4 个检查点通过 | |
| 🏅 **最终**：秋招可用 | M1~M7 全部达成 | |

---

## ⚡ 快速自测命令

> 在项目根目录 `~/rpc` 下执行，验证当前项目状态。

```bash
# 编译检查
cmake --build build --config Release 2>&1 | tail -5

# 单元测试
cd build && ctest --output-on-failure 2>&1 | tail -20

# ASan 检查
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON ..
cmake --build build-asan && cd build-asan && ctest --output-on-failure

# Benchmark
./build/bench_coroutine 2>&1 | head -10
./build/bench_serializer 2>&1 | head -10
./build/bench_buffer 2>&1 | head -10

# 格式化检查
find include src -name '*.hpp' -o -name '*.cpp' | xargs clang-format --dry-run --Werror
```

---

> 📌 **提示**：此清单与 `rpc_learning_roadmap.md` 配套使用。路线文档提供"怎么学"，本清单提供"学到哪"。
> 
> 最后更新：2025-01
