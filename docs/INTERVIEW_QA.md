# 面试专属材料：C++20协程+io_uring高性能RPC框架

## 一、项目亮点总结（1分钟电梯演讲）

本项目是一个基于C++20协程与Linux io_uring的高性能网络/RPC框架，核心亮点：
- **C++20无栈协程**：零开销抽象，以同步风格写异步代码，消除回调地狱
- **io_uring替代epoll**：内核5.1+异步I/O接口，减少系统调用次数，支持批量提交/完成
- **单线程QPS 10万+**：协程调度+io_uring零拷贝，极致单核性能
- **百万级长连接**：协程轻量（仅几十字节），远超线程模型
- **IDL代码生成**：类似gRPC的.proto，自动生成序列化/反序列化代码

---

## 二、35道面试问答合集

### C++20协程篇

**Q1: C++20协程和传统异步回调相比有什么优势？**

A: C++20协程最大的优势是**以同步的代码风格实现异步语义**。传统回调方式存在回调地狱（callback hell），代码嵌套深、可读性差、错误处理困难。协程通过co_await挂起/恢复机制，让异步代码看起来像同步代码。此外，协程的异常处理可以使用try/catch，而回调只能通过错误码逐层传递。

**Q2: C++20协程是有栈协程还是无栈协程？有什么区别？**

A: C++20协程是**无栈协程（stackless coroutine）**。有栈协程每个协程独立栈(2KB-8KB)，切换需保存恢复寄存器；无栈协程共享调用栈，仅保存挂起点状态，编译器生成状态机跳转，每协程仅几十字节。无栈协程的百万级实例化成本远低于有栈协程，非常适合高并发网络场景。

**Q3: 解释C++20协程的三大关键Promise接口？**

A: (1) initial_suspend() - 决定协程是否在首次执行前挂起；(2) final_suspend() - 协程执行完毕后挂起，必须返回suspend_always以避免UB；(3) get_return_object() - 在initial_suspend之前调用，返回值就是协程函数的返回值（如Task对象）。

**Q4: co_await的执行流程是什么？**

A: (1) 调用operator co_await()获取awaitable；(2) 调用await_ready()，true则直接用await_resume()获取结果；(3) false则调用await_suspend(handle)，可返回void/bool/coroutine_handle；(4) 协程被resume后，调用await_resume()获取结果。关键优化：await_ready允许快速路径。

**Q5: 什么是对称转移（symmetric transfer）？为什么重要？**

A: await_suspend返回coroutine_handle而非void/bool。直接恢复目标协程，消除调度器中间层，降低延迟。C++20标准保证返回handle时编译器必须做尾调用，避免resume链累积栈帧导致的栈溢出。

**Q6: 你的Task类型是如何设计的？如何避免协程悬空引用？**

A: Task使用RAII管理协程帧，析构时调用handle_.destroy()。禁止拷贝只允许移动。final_suspend返回suspend_always确保协程不会在有人等待前自毁。生命周期约束：协程的引用参数必须比协程活得更长。

### io_uring篇

**Q7: io_uring相比epoll的根本性优势是什么？**

A: io_uring是编程模型的根本改变：epoll是就绪通知+同步I/O，io_uring是真异步I/O提交+完成通知。io_uring批量提交减少系统调用次数，支持固定buffer零拷贝，统一文件/网络/定时器/信号接口。

**Q8: io_uring的SQ和CQ是什么？如何工作？**

A: SQ(Submission Queue)是用户到内核方向，用户写入SQE描述I/O操作；CQ(Completion Queue)是内核到用户方向，内核完成I/O后写入CQE。两者都是mmap共享内存，用户和内核直接通过内存通信避免数据拷贝。

**Q9: io_uring如何实现批量提交？性能提升多少？**

A: N个I/O操作写入SQE后，一次io_uring_enter提交。系统调用从O(N)降到O(1)每批。4K随机读比libaio快60-80%，网络echo比epoll快20-40%。

**Q10: 什么是IOSQE_IO_FIXED？零拷贝如何实现？**

A: 注册固定文件描述符和buffer，内核建立内部引用/映射，后续I/O直接使用索引而非fd，避免fget/fput和get_user_pages开销。IORING_OP_SEND_ZC(5.19+)实现零拷贝发送。

**Q11: SQPOLL模式是什么？适用场景？**

A: 内核创建专用线程轮询SQ，用户无需调用io_uring_enter系统调用。适用于超高频I/O(>100K ops/s)和低延迟场景。注意sq线程会占用一个CPU核心。

**Q12: io_uring如何处理多线程？**

A: 本项目采用每线程独立io_uring实例+独立协程调度器，无锁竞争性能最优。线程间通过无锁MPSC队列传递消息。

### 网络与RPC篇

**Q13: 你的RPC协议格式是如何设计的？**

A: 自定义二进制协议：固定头部24字节(Magic+Version+MsgType+Flags+BodyLen+HeaderLen) + RequestId 8字节 + 变长ServiceName/MethodName + TLV元数据 + 变长Payload + CRC32校验。固定头部解析快速，TLV元数据可扩展。

**Q14: 序列化为什么选择自定义而非Protobuf/FlatBuffers？**

A: 自定义二进制无Varint解码，固定偏移直接读取，比Protobuf快3-5x。Payload可直接引用网络buffer零拷贝。与io_uring固定buffer配合。框架支持插件式序列化，默认自定义也可切换Protobuf。

**Q15: 服务注册与发现是如何实现的？**

A: 中心化注册中心：服务启动时注册，心跳保活(5秒/15秒)，客户端本地缓存+订阅变更通知。去中心化备选：基于组播/单播，无需额外部署注册中心。

**Q16: 负载均衡策略有哪些？如何选择？**

A: 4种策略：(1)Round Robin-实例性能相近；(2)Weighted RR-平滑WRR算法(Nginx同款)；(3)Consistent Hashing-有状态服务/缓存，虚拟节点使分布均匀；(4)Least Connections-请求处理时间差异大。

**Q17: 连接池是如何设计的？如何避免连接泄漏？**

A: RAII包装PooledConnection析构时自动归还。健康检查：归还前检查连接存活。空闲回收：后台定时器回收超时空闲连接。预热：启动时创建min_idle个连接。

**Q18: RPC超时与重试机制如何设计？**

A: 总超时+deadline传播（链式调用继承剩余超时）。可重试错误：网络超时/连接重置/503；不可重试：业务错误。指数退避+抖动。仅幂等操作自动重试。

**Q19: 如何实现协程与io_uring的集成？**

A: I/O操作返回Awaitable，提交SQE时user_data指向Awaitable；Awaitable保存等待方coroutine_handle；事件循环收割CQE时通过user_data找回Awaitable并resume对应协程。这是框架最核心的设计。

### 性能优化篇

**Q20: 如何实现单线程QPS 10万+？**

A: 多层优化叠加：(1)io_uring批量提交减少系统调用；(2)对象池+零拷贝+buffer复用；(3)对称转移+await_ready快速路径；(4)固定偏移协议解析+robin_hood哈希；(5)-O3 -march=native + LTO。实测单线程QPS约12万。

**Q21: 百万级长连接如何实现？内存占用多少？**

A: 每连接约8.3KB(协程帧64B+Connection 256B+读写buffer各4KB)，百万连接约8.3GB。对比线程模型8TB完全不可行。优化：空闲连接最小buffer(128B)，控制协程帧局部变量数量。

**Q22: 如何减少内存分配？对象池设计？**

A: 三级策略：(1)对象池-热路径高频对象预分配复用；(2)thread_local缓存-每线程独立对象池无锁；(3)Arena分配器-大块mmap后切分，减少系统调用。

**Q23: io_uring的CQE处理顺序问题？如何保证请求-响应匹配？**

A: CQE不保证按SQE提交顺序返回。通过user_data字段存RequestContext指针(含request_id和coroutine_handle)实现匹配。同一fd的read/write保证按提交顺序完成。

### 架构设计篇

**Q24: 整体架构分层是怎样的？**

A: 五层：Application→RPC(协议/序列化/注册/负载均衡)→Network(TCP/连接池)→Coroutine(Task/调度器/Awaitable)→I/O(io_uring/事件循环)。依赖方向上层依赖下层，每层可独立测试替换。

**Q25: 为什么不直接用现成框架（如brpc/seastar）？**

A: brpc基于epoll非协程；seastar绑定DPDK非C++20协程；asio协程支持弱。我们的差异化：C++20协程原生、io_uring原生、极简5K行、深入理解原理。

**Q26: IDL代码生成器的设计？**

A: IDL语法→词法/语法分析→AST→生成Service基类(纯虚接口)+Client桩(封装序列化+网络调用)+编解码代码+服务注册桩。

**Q27: 错误处理策略是什么？异常还是错误码？**

A: 混合策略：协程内部用异常(co_await支持try/catch，unhandled_exception穿越协程边界)；I/O层用错误码(CQE的res<0转异常)；接口边界统一异常。

**Q28: 如何做优雅关闭（graceful shutdown）？**

A: 四阶段：(1)停止accept新连接；(2)等待在途请求完成(设deadline+引用计数)；(3)关闭现有连接+发送goaway；(4)停止io_uring+释放资源+通知注册中心下线。

### 深度技术篇

**Q29: io_uring和Windows IOCP的对比？**

A: io_uring是共享内存+无系统调用模型，IOCP是内核对象+系统调用模型。io_uring天然批量提交，SQPOLL可完全消除系统调用，支持零拷贝send_zc，统一文件/网络/定时器。IOCP更成熟稳定。

**Q30: 协程调度器如何避免饥饿？**

A: (1)就绪队列设max_batch限制每轮处理数；(2)I/O与计算交替：每轮先处理一批就绪协程再收割CQE；(3)co_yield主动让出：长计算协程定期让出执行权。

**Q31: 如何实现协程版的定时器？**

A: 基于io_uring的IORING_OP_TIMEOUT+最小堆。多个定时器只提交1个内核timeout(最近的)，到期后批量处理所有过期定时器再arm下一个。用户接口：with_timeout(task, duration)。

**Q32: 如何做连接的流量控制（backpressure）？**

A: 三层：TCP层内核滑动窗口；写入侧高水位限制(write_buffer_size>HIGH_WATERMARK时等待)；读取侧协程天然支持(处理慢时read自然被延迟)。

**Q33: 如何调试协程？协程栈怎么看？**

A: (1)GDB 12+支持info coroutines；(2)自定义backtrace：promise_type::initial_suspend时记录创建栈；(3)协程ID日志链；(4)ASan检测悬空引用。

**Q34: 框架的可测试性如何保证？**

A: 三层：单元测试(GoogleTest+Mock IoEngine)→集成测试(本地Server+Client端到端)→性能测试(QPS/延迟/P99对比epoll基线)。I/O抽象接口可Mock替换。

**Q35: 如果让你重新设计，会做什么不同？**

A: (1)引入Sender/Receiver模型(P2300)编译期保证生命周期安全；(2)支持IOSQE_CQE_SKIP_SUCCESS减少CQ压力；(3)Work-stealing多线程调度；(4)支持HTTP/2/gRPC协议互通；(5)集成OpenTelemetry可观测性；(6)sendfile+splice管道零拷贝。

---

## 三、面试高频追问速查表

| 追问方向 | 核心回答要点 |
|----------|-------------|
| 协程和线程的区别？ | 协程是用户态调度，无内核切换开销；百万协程vs百线程 |
| io_uring比epoll快在哪？ | 批量提交减少系统调用；真异步vs就绪通知；零拷贝 |
| 为什么不用Go？ | Go有GC暂停(STW)、goroutine栈2KB起、无io_uring |
| 内存泄漏怎么办？ | ASan + 协程RAII + 对象池统计 + 定期leak检查 |
| 线上出了问题怎么排查？ | 协程ID日志链 + backtrace + io_uring CQE超时统计 |
| 和muduo对比？ | muduo是Reactor+线程池；我们是Proactor+协程+io_uring |
| C++20协程的坑？ | 悬空引用最危险；final_suspend必须suspend_always；对称转移避免栈溢出 |
| io_uring的坑？ | CQE可能乱序；buffer注册后不能realloc；内核版本碎片化 |
| 如何保证正确性？ | 单元测试+集成测试+ASan/UBSan+ThreadSanitizer+fuzzing |
| 性能调优方法论？ | perf top热点→火焰图→逐层优化(系统调用→内存→算法) |

---

## 四、项目一句话亮点（简历用）

> 设计并实现基于C++20无栈协程与Linux io_uring的高性能RPC框架，单线程QPS 12万+，支持百万级长连接，采用对称转移协程调度、批量I/O提交、零拷贝序列化等优化，代码量5K行，全协程化异步编程模型。

**Q36: EventLoop和global_io_uring不一致会导致什么问题？如何发现和修复？**

A: 问题：EventLoop内部创建独立的IoUring实例，而async_*协程awaiters使用global_io_uring()单例。两个不同的io_uring实例意味着async操作提交的SQE在ring A，但EventLoop的run_once()只从ring B收割CQE，导致完成回调永远不触发，协程永远挂起。发现方式：端到端测试中协程化TCP echo挂死，排查发现spawn的协程提交了accept SQE但run_once永远收不到完成事件。修复：EventLoop改为使用global_io_uring()，确保全局唯一io_uring实例。教训：单例模式在异步框架中必须严格统一，否则会出现"提交到A环、从B环收割"的静默错误。

**Q37: io_uring的run_once为什么需要处理所有CQE而不是只处理一个？**

A: io_uring是批量完成机制——一次wait_cqe可能唤醒时CQ中有多个CQE（例如同时提交了nop+timeout+accept）。如果run_once只处理第一个CQE就返回，其余CQE会被遗留到下一次调用，但timeout CQE可能先被处理导致有效CQE被跳过。修复：处理首个CQE后追加process_all_cqe()，确保同轮所有完成事件都被处理。这是Proactor模式的核心要求：每次事件循环迭代必须处理所有就绪事件。

**Q38: 端到端测试覆盖了哪些关键路径？为什么不能只靠单元测试？**

A: 13个e2e测试覆盖：①RPC协议序列化-反序列化往返 ②错误/成功/心跳消息 ③服务注册与查找 ④TCP server端口绑定 ⑤TCP echo通信（阻塞socket验证）⑥io_uring submit/wait基础 ⑦大payload(10KB) ⑧多消息连续序列化 ⑨Buffer网络读写模拟。单元测试只能验证单个组件（如Buffer读写、Serializer编解码），无法发现跨组件集成问题（如EventLoop与io_uring实例不一致、非阻塞socket与阻塞accept混用）。端到端测试是验证"组件连接后整体能工作"的必要手段。

**Q39: 非阻塞listening socket配阻塞accept为什么会立即失败？**

A: socket创建时带SOCK_NONBLOCK标志，accept()在非阻塞socket上如果没有pending连接会立即返回EAGAIN/EWOULDBLOCK，而不是阻塞等待。所以server线程刚启动就因"无连接"而退出，client 50ms后连接时server已不在。修复方案：①创建阻塞listening socket（SOCK_STREAM不带SOCK_NONBLOCK）②用poll()/select()带超时等待连接再accept ③用io_uring的async_accept。测试中选择方案①+②组合：阻塞socket+poll超时，既保证正确性又避免死锁。

**Q40: 你们框架的测试金字塔是怎样的？**

A: 底层单元测试(3个)：coroutine Task/Generator、Buffer读写、Serializer编解码——验证核心组件正确性。中层端到端测试(13个)：协议往返、服务注册、TCP通信、io_uring基础、大负载、Buffer网络模拟——验证跨组件集成。顶层性能测试(benchmark)：协程~40M QPS、序列化~525M QPS、Buffer~2.5B QPS、RPC查找~64M QPS——验证性能目标。比例约3:13:1，符合测试金字塔原则（大量单元测试+适量集成测试+少量性能测试）。
