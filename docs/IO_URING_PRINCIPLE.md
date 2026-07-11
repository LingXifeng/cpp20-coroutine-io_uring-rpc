# io_uring 核心原理

## 1. 传统 I/O 模型的问题

### 1.1 同步阻塞 I/O

```cpp
// 每个连接一个线程
void handle_connection(int fd) {
    char buf[1024];
    while (true) {
        int n = read(fd, buf, sizeof(buf));  // 阻塞等待
        if (n <= 0) break;
        write(fd, buf, n);                    // 阻塞等待
    }
}
```

**问题：**
- 线程数量 = 连接数量
- 线程栈开销大（每线程 1-8MB）
- 线程切换开销高

### 1.2 epoll

```cpp
int epfd = epoll_create1(0);
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);

while (true) {
    epoll_wait(epfd, events, MAX_EVENTS, -1);  // 系统调用
    for (auto& ev : events) {
        if (ev.events & EPOLLIN) {
            read(ev.data.fd, buf, len);        // 系统调用
        }
    }
}
```

**问题：**
- 每次操作都需要系统调用
- 用户态/内核态数据拷贝
- 只支持文件描述符事件

## 2. io_uring 设计

### 2.1 核心思想

io_uring 使用**共享内存环形缓冲区**实现用户态和内核态的通信：

```
┌─────────────────────────────────────────────────────────────┐
│                      Shared Memory                          │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   Submission Queue (SQ)              Completion Queue (CQ)  │
│   ┌───┬───┬───┬───┬───┐              ┌───┬───┬───┬───┬───┐  │
│   │ 0 │ 1 │ 2 │ 3 │...│              │ 0 │ 1 │ 2 │ 3 │...│  │
│   └───┴───┴───┴───┴───┘              └───┴───┴───┴───┴───┘  │
│        ▲                                    ▲               │
│        │                                    │               │
│   用户态写入 SQE                     内核态写入 CQE         │
│   内核态读取 SQE                     用户态读取 CQE         │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 SQE (Submission Queue Entry)

```c
struct io_uring_sqe {
    __u8    opcode;         // 操作码 (read, write, accept, ...)
    __u8    flags;          // 标志位
    __u16   ioprio;         // I/O 优先级
    __s32   fd;             // 文件描述符
    union {
        __u64   off;        // 文件偏移
        __u64   addr2;      // 第二个缓冲区地址
    };
    __u64   addr;           // 缓冲区地址
    __u32   len;            // 缓冲区长度
    ...
    __u64   user_data;      // 用户数据（用于关联 CQE）
};
```

### 2.3 CQE (Completion Queue Entry)

```c
struct io_uring_cqe {
    __u64   user_data;      // 对应 SQE 的 user_data
    __s32   res;            // 操作结果（返回值或错误码）
    __u32   flags;          // 标志位
};
```

## 3. 工作流程

### 3.1 提交请求

```cpp
// 1. 获取空闲 SQE
struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);

// 2. 准备 SQE
io_uring_prep_read(sqe, fd, buffer, len, offset);

// 3. 设置用户数据（用于识别完成事件）
sqe->user_data = my_request_id;

// 4. 提交（可以批量）
io_uring_submit(&ring);
```

### 3.2 等待完成

```cpp
// 1. 等待完成事件
struct io_uring_cqe *cqe;
io_uring_wait_cqe(&ring, &cqe);

// 2. 处理结果
int result = cqe->res;
uint64_t id = cqe->user_data;

// 3. 标记已处理
io_uring_cqe_seen(&ring, cqe);
```

### 3.3 批量处理

```cpp
// 批量提交
for (int i = 0; i < N; ++i) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, fds[i], bufs[i], lens[i], 0);
}
io_uring_submit(&ring);  // 一次系统调用提交 N 个请求

// 批量收割
struct io_uring_cqe *cqes[N];
int n = io_uring_peek_batch_cqe(&ring, cqes, N);
for (int i = 0; i < n; ++i) {
    // 处理 cqes[i]
    io_uring_cqe_seen(&ring, cqes[i]);
}
```

## 4. 支持的操作

### 4.1 文件操作

| 操作 | 说明 |
|------|------|
| IORING_OP_READ | 异步读 |
| IORING_OP_WRITE | 异步写 |
| IORING_OP_READV | 向量读 |
| IORING_OP_WRITEV | 向量写 |
| IORING_OP_FSYNC | 同步文件 |
| IORING_OP_OPENAT | 打开文件 |
| IORING_OP_CLOSE | 关闭文件 |

### 4.2 网络操作

| 操作 | 说明 |
|------|------|
| IORING_OP_ACCEPT | 接受连接 |
| IORING_OP_CONNECT | 建立连接 |
| IORING_OP_SEND | 发送数据 |
| IORING_OP_RECV | 接收数据 |
| IORING_OP_SENDMSG | 发送消息 |
| IORING_OP_RECVMSG | 接收消息 |

### 4.3 其他操作

| 操作 | 说明 |
|------|------|
| IORING_OP_TIMEOUT | 超时 |
| IORING_OP_POLL | 轮询事件 |
| IORING_OP_STATX | 获取文件状态 |
| IORING_OP_SPLICE | 零拷贝数据移动 |

## 5. 高级特性

### 5.1 链式请求

```cpp
// 请求链：read -> write -> close
sqe1 = io_uring_get_sqe(&ring);
io_uring_prep_read(sqe1, fd_in, buf, len, 0);
sqe1->flags |= IOSQE_IO_LINK;  // 链接到下一个

sqe2 = io_uring_get_sqe(&ring);
io_uring_prep_write(sqe2, fd_out, buf, len, 0);
sqe2->flags |= IOSQE_IO_LINK;  // 链接到下一个

sqe3 = io_uring_get_sqe(&ring);
io_uring_prep_close(sqe3, fd_in);
// 无 LINK 标志，链结束
```

### 5.2 固定缓冲区

```cpp
// 注册固定缓冲区，避免每次传递地址
struct iovec iovs[N];
io_uring_register_buffers(&ring, iovs, N);

// 使用固定缓冲区索引
sqe->flags |= IOSQE_FIXED_BUFFER;
sqe->buf_index = buffer_index;
```

### 5.3 固定文件描述符

```cpp
// 注册固定文件描述符
int fds[N];
io_uring_register_files(&ring, fds, N);

// 使用固定文件索引
sqe->flags |= IOSQE_FIXED_FILE;
sqe->fd = file_index;
```

## 6. 性能对比

### 6.1 系统调用次数

| 场景 | epoll | io_uring |
|------|-------|----------|
| 1000 次 read | 2000 次 | 1-2 次 |
| 1000 次 write | 2000 次 | 1-2 次 |
| accept + read + write | 3+ 次 | 1 次 |

### 6.2 基准测试

```
                    IOPS
epoll:              ~1.5M
io_uring (basic):   ~2.5M
io_uring (fixed):   ~3.5M
io_uring (poll):    ~4.5M
```

## 7. 与协程结合

```cpp
// 异步读取协程
Task<size_t> async_read(int fd, void* buf, size_t len) {
    // 提交异步读请求
    auto sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, fd, buf, len, 0);
    sqe->user_data = request_id;
    io_uring_submit(&ring);
    
    // 等待完成（暂停协程）
    size_t result = co_await wait_completion(request_id);
    
    co_return result;
}

// 使用
Task<void> handle_connection(int fd) {
    char buf[1024];
    while (true) {
        size_t n = co_await async_read(fd, buf, sizeof(buf));
        if (n == 0) break;
        co_await async_write(fd, buf, n);
    }
}
```

## 8. 内核版本要求

| 特性 | 内核版本 |
|------|----------|
| 基本功能 | 5.1+ |
| 文件操作 | 5.1+ |
| 网络操作 | 5.5+ |
| 链式请求 | 5.5+ |
| 固定缓冲区 | 5.7+ |
| 快速轮询 | 5.15+ |
