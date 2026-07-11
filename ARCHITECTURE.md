
## P0 新增：超时机制 + 多线程 EventLoop

### 超时机制（三层设计）

```
┌─────────────────────────────────────────────┐
│  Layer 3: with_timeout<T>()                 │  协程级超时包装器
│  - 提交主操作 + 独立 timeout SQE             │  超时自动 cancel 主操作
│  - done_ 原子标志防止双重 resume              │
├─────────────────────────────────────────────┤
│  Layer 2: TimedRecvAwaiter / TimedSend      │  操作级 linked timeout
│  - IOSQE_IO_LINK + IORING_OP_LINK_TIMEOUT   │  io_uring 原生链接超时
│  - done_ 原子标志，先完成者 resume 协程       │
├─────────────────────────────────────────────┤
│  Layer 1: TimeoutAwaiter                    │  io_uring 级超时
│  - IORING_OP_TIMEOUT SQE                   │  纯等待，不关联操作
│  - IORING_OP_ASYNC_CANCEL 取消              │
└─────────────────────────────────────────────┘
```

**TimingWheel**：Hashed Timing Wheel，O(1) 插入/过期，用于大量短超时 RPC 调用。

### 多线程 EventLoop（Reactor-Per-Thread）

```
┌─────────────────────────────────────────────┐
│  EventLoopPool                              │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐    │
│  │ MainLoop │ │ Worker 0 │ │ Worker 1 │    │
│  │ io_uring │ │ io_uring │ │ io_uring │    │
│  │ thread   │ │ thread   │ │ thread   │    │
│  └──────────┘ └──────────┘ └──────────┘    │
│       │              │          │            │
│       └── MSG_RING ──┘─────────┘            │
│         (跨线程唤醒，零管道开销)              │
│                                             │
│  负载均衡：round-robin / least-connections   │
│  连接计数：原子操作，O(1) 查询               │
└─────────────────────────────────────────────┘
```

**EventLoopThread**：每个线程独立 io_uring 实例 + 事件循环。
**MSG_RING**：`IORING_OP_MSG_RING` 跨线程投递任务，无需 pipe/eventfd。
**post()**：线程安全任务投递，MSG_RING 优先，失败回退 write(eventfd)。
