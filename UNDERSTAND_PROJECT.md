# 🧠 彻底理解一个项目的 5 层方法论

> 针对 C++20 协程 + io_uring RPC 框架的具体训练计划

---

## 理解的 5 个层次

```
Level 1: 能复述    — "这个模块做了什么"         ← 大多数人停在这
Level 2: 能画图    — "数据怎么流转的"           ← 面试及格线
Level 3: 能解释    — "为什么这么设计，不这么设计会怎样"  ← 面试加分
Level 4: 能改      — "加个功能 / 改个行为"       ← 面试强信号
Level 5: 能造      — "从零写出等价实现"          ← 真正的懂
```

**面试到 Level 3 就够用，Level 4 是杀手锏。**

---

## Level 1 → 2：从"看了"到"懂了"（10h）

### 方法：画 4 张图，每张图用纸笔画，不要用工具

#### 图 1：模块依赖图

```
你画完后应该长这样：

framework.hpp
  ├── coroutine/
  │   ├── task.hpp          ← Task, PromiseType
  │   ├── awaitable.hpp     ← IOAwaitable, TimerAwaitable
  │   ├── generator.hpp     ← Generator (lazy sequence)
  │   ├── scheduler.hpp     ← 协程调度 (ready + suspended 队列)
  │   └── sync.hpp          ← SyncWait, WhenAll
  ├── io/
  │   ├── io_uring.hpp      ← io_uring_setup/submit/peek 封装
  │   ├── event_loop.hpp    ← 事件循环 (单例, run_once)
  │   ├── event_loop_pool.hpp ← 多线程池
  │   ├── timer.hpp         ← TimingWheel
  │   ├── async_io.hpp      ← async_read/write/accept 封装
  │   └── io.hpp            ← 统一 include
  ├── net/
  │   ├── buffer.hpp        ← 链式 BufferBlock
  │   ├── connection.hpp    ← TCP 连接 (fd + read/write)
  │   ├── tcp_server.hpp    ← 监听 + accept
  │   ├── tcp_client.hpp    ← 连接 + 发请求
  │   ├── connection_pool.hpp ← 连接池管理
  │   └── load_balancer.hpp ← 3 种策略
  ├── rpc/
  │   ├── rpc_server.hpp    ← 注册 + 分发
  │   ├── rpc_client.hpp    ← 代理 + 调用
  │   └── service_registry.hpp ← 服务表 O(1) 查找
  └── idl/
      ├── idl_parser.hpp    ← 解析 .idl 文件
      └── code_generator.hpp ← 生成 C++ 代码
```

**训练**：不看代码，凭记忆画一遍。画不出来说明没真懂。

#### 图 2：一次 RPC 调用的完整数据流

```
客户端                              服务端
  │                                   │
  │ rpc_client.call("add", {1,2})     │
  │   │                               │
  │   ▼ 序列化                         │
  │   [method_id][arg1][arg2]         │
  │   │                               │
  │   ▼ Buffer 写入                    │
  │   BufferBlock链 → fd              │
  │   │                               │
  │   ▼ io_uring submit write         │
  │   ═══════ TCP ═══════════════════>│
  │                                   │ io_uring peek_cqe
  │                                   │   │
  │                                   │   ▼ Buffer 读取
  │                                   │   │
  │                                   │   ▼ 反序列化
  │                                   │   │ → method_id + args
  │                                   │   │
  │                                   │   ▼ service_registry 查找
  │                                   │   │ → handler 函数
  │                                   │   │
  │                                   │   ▼ 执行 handler(1,2) → 3
  │                                   │   │
  │                                   │   ▼ 序列化结果
  │                                   │   │ → [status][result=3]
  │                                   │   │
  │                                   │   ▼ io_uring submit write
  │ <═════ TCP ═══════════════════════│
  │ io_uring peek_cqe                 │
  │   │                               │
  │   ▼ Buffer 读取                    │
  │   │                               │
  │   ▼ 反序列化                       │
  │   │ → RpcResult{3}                │
  │   │                               │
  │ co_return 3                       │
```

**训练**：画完后问自己：数据在每一步是什么格式？经过了哪些转换？

#### 图 3：协程生命周期

```
Task<int> my_coro() {
    auto val = co_await some_io();   ← ① 挂起，返回给调度器
    co_return val + 1;               ← ③ 恢复后继续执行
}

调用方:
auto task = my_coro();              ← 创建协程帧（堆上分配）
task.resume();                       ← ② 调度器恢复协程
// task.done() == true              ← ④ 协程完成
// task.get_result() == val+1       ← ⑤ 获取结果
```

**训练**：能画出 co_await 时 CPU 的执行流是怎么跳转的。

#### 图 4：io_uring 工作流程

```
应用                          内核
  │                            │
  │ ① 准备 SQE (submit queue entry)
  │    sqe->opcode = READ
  │    sqe->fd = 3
  │    sqe->addr = buffer
  │                            │
  │ ② io_uring_submit()  ──────> ③ 内核处理 I/O
  │                            │    (异步，不阻塞)
  │ ④ 做其他事...               │
  │                            │ ⑤ 完成后写 CQE
  │                            │    (completion queue entry)
  │ ⑥ peek_cqe()  <────────── │
  │    cqe->res = 字节数
  │    cqe->user_data = 标识
  │
  │ ⑦ 回调/恢复协程
```

**训练**：能解释 SQ 和 CQ 的内存布局（共享内存环形缓冲区）。

---

## Level 2 → 3：从"懂了"到"能解释为什么"（10h）

### 方法：对每个设计决策，回答 3 个问题

| 设计决策 | 为什么这么做？ | 不这么做的后果？ | 有什么替代方案？ |
|---------|-------------|----------------|----------------|
| Buffer 用链式 Block | 动态增长，无需预分配大块 | 连续内存要 realloc 拷贝 | 可选：mmap 区域 / arena |
| 序列化用 varint | 小数字省空间，网络传输快 | 固定 int32 浪费 3 字节 | 可选：protobuf / flatbuffers |
| TimingWheel 用哈希环 | O(1) 添加/过期 | 红黑树 O(logN) | 可选：最小堆 / 跳表 |
| EventLoop 单例 | 单线程模型，避免锁 | 多实例要协调 | 已改：EventLoopPool |
| 协程用 intrusive list | 挂起/恢复 O(1)，无分配 | vector 删除 O(N) | 可选：侵入式 set |
| 连接池 + 负载均衡 | 复用连接，分散压力 | 每次新建连接慢 | 可选：无池 + DNS RR |

### 训练方法：假装面试官追问

对每个模块，自问自答这个循环：

```
你：我用了链式 Buffer
面试官：为什么不用连续内存？
你：因为不需要预分配，动态增长零拷贝
面试官：那为什么不直接用 std::vector？
你：vector 扩容要拷贝，链式 Block 不用
面试官：链式 Block 有什么缺点？
你：缓存不友好，不能随机访问
面试官：那什么时候该用连续内存？
你：需要随机访问 / 数据量已知时，比如 HTTP 头部解析
```

**如果你在任何一层卡住，说明那个点没真懂，回去看代码。**

---

## Level 3 → 4：从"能解释"到"能改"（10h）

### 方法：做 7 个修改练习，每个改完跑测试

#### 练习 1：加一种负载均衡策略（最简单）

```cpp
// 在 net/load_balancer.hpp 中加一个 Random 策略
// 思路：随机选一个后端
// 验证：跑 test_rpc_call 确认通过
```

**你学到的**：负载均衡的扩展点在哪，Strategy 模式怎么用。

#### 练习 2：给 RPC 加请求 ID

```cpp
// 在 RpcRequest/RpcResponse 中加 request_id 字段
// 序列化/反序列化都要改
// 验证：跑 test_e2e
```

**你学到的**：序列化协议怎么改，前后端怎么同步。

#### 练习 3：加一个连接超时配置

```cpp
// 连接池获取连接时，如果超过 N 秒没可用连接，返回错误
// 验证：写一个测试用例
```

**你学到的**：超时机制怎么和业务逻辑结合。

#### 练习 4：加日志输出

```cpp
// 在 EventLoop 的 run_once 中加每次循环处理的 CQE 数量统计
// 验证：运行 echo_server，观察输出
```

**你学到的**：可观测性怎么加，性能热点在哪。

#### 练习 5：改 Buffer 块大小

```cpp
// 把 BufferBlock 默认大小从 4096 改成 8192
// 跑 benchmark，对比性能差异
```

**你学到的**：块大小对性能的影响，内存对齐。

#### 练习 6：加 graceful shutdown

```cpp
// 收到 SIGINT 后：
// 1. 停止 accept 新连接
// 2. 等待现有请求完成（设超时）
// 3. 退出
```

**你学到的**：信号处理 + 生命周期管理。

#### 练习 7：写一个新示例

```cpp
// 用框架写一个简单的聊天室服务器
// 多个客户端连接，一条消息广播给所有人
```

**你学到的**：框架的 API 好不好用，哪里需要改进。

---

## Level 4 → 5：从"能改"到"能造"（15h）

### 方法：选 2 个模块，从零重写

不需要重写整个项目，选**最核心的 2 个**：

### 模块 A：从零写一个协程调度器

```cpp
// 目标：不看你现有代码，从空文件开始写
// 要求：
//   - Task<T> 支持 co_await / co_return
//   - Scheduler 管理 ready + suspended 队列
//   - 支持 sync_wait() 阻塞等待结果
// 
// 提示（只给方向，不给代码）：
//   1. 先写 promise_type（return_object / initial_suspend / final_suspend）
//   2. 再写 Task（handle 包装 + resume + get_result）
//   3. 最后写 Scheduler（schedule + run）
//
// 检验：能跑通一个 co_await 链式调用
// 预计：4-6h
```

### 模块 B：从零写一个 io_uring echo server

```cpp
// 目标：不看你现有代码，从空文件开始写
// 要求：
//   - io_uring_setup + 提交 accept/read/write
//   - 循环 peek_cqe 处理完成事件
//   - 能跑通 echo: 客户端发什么，服务端回什么
//
// 提示：
//   1. 先写 io_uring 初始化（setup + mmap SQ/CQ）
//   2. 再写 submit_sqe（accept → read → write 循环）
//   3. 最后写事件循环（peek_cqe + 回调）
//
// 检验：nc 连上去能 echo
// 预计：3-5h
```

**这两个模块从零写完后，你对项目的理解会质变。**

---

## 每日训练计划（3 周搞定）

### 第 1 周：Level 1→2（画图 + 追踪数据流）

| 天 | 任务 | 时间 |
|----|------|------|
| D1 | 画模块依赖图，不看书 | 2h |
| D2 | 画 RPC 完整数据流图 | 2h |
| D3 | 画协程生命周期图 | 2h |
| D4 | 画 io_uring 工作流程图 | 2h |
| D5 | 用 gdb 跑一遍 echo_server，单步跟踪 | 2h |
| D6 | 用 strace 跟踪系统调用序列 | 1h |
| D7 | 复习 4 张图，能脱稿画 | 1h |

### 第 2 周：Level 2→3（解释为什么 + 改代码）

| 天 | 任务 | 时间 |
|----|------|------|
| D8 | 对 6 个设计决策写"为什么/不这样/替代"表 | 2h |
| D9 | 做修改练习 1-2（负载均衡 + 请求 ID） | 2h |
| D10 | 做修改练习 3-4（连接超时 + 日志） | 2h |
| D11 | 做修改练习 5-6（Buffer 大小 + graceful shutdown） | 2h |
| D12 | 做修改练习 7（写聊天室示例） | 2h |
| D13 | 面试题脱稿讲（INTERVIEW_QA.md 前 20 题） | 2h |
| D14 | 面试题脱稿讲（后 20 题） | 2h |

### 第 3 周：Level 4→5（从零写 + 模拟面试）

| 天 | 任务 | 时间 |
|----|------|------|
| D15-16 | 从零写协程调度器 | 6h |
| D17-18 | 从零写 io_uring echo server | 5h |
| D19 | 录屏模拟讲项目（10 分钟） | 2h |
| D20 | 找人模拟面试，被追问 | 2h |
| D21 | 查漏补缺，整理最终版 | 2h |

---

## 检验标准：你真的懂了吗？

### 自测清单

- [ ] 不看代码，能画出 4 张图
- [ ] 随便指一个函数，能说出它被谁调用、调用了谁
- [ ] 随便指一个设计决策，能说出 3 个替代方案和优劣
- [ ] 能在 30 分钟内给框架加一个新功能
- [ ] 能从零写出一个协程 Task + io_uring echo server
- [ ] INTERVIEW_QA.md 40 题能脱稿讲
- [ ] 录屏讲项目 10 分钟不卡壳

### 终极检验：费曼测试

> **如果你能给一个不懂编程的人讲清楚这个项目做了什么、为什么这么做，你就真的懂了。**

试一下：打开手机录音，假装给同学讲"协程调度器是怎么工作的"，讲 5 分钟。回放听一遍，卡壳的地方就是没真懂的地方。

---

## 一个反直觉的建议

> **不要从第一行代码开始顺序读。**

正确的阅读顺序：

```
1. README.md          → 知道项目做什么
2. framework.hpp      → 知道对外 API 长什么样
3. examples/          → 知道用户怎么用
4. 画数据流图         → 知道数据怎么流转
5. 再读核心实现       → 知道内部怎么实现
6. 最后读测试         → 知道边界情况怎么处理
```

**先理解"为什么"和"怎么用"，再理解"怎么实现"。**
