# RPC Framework 架构设计文档

## 1. 概述

本项目是一个基于 **C++20 协程** 与 **io_uring** 的高性能网络/RPC 框架，旨在实现：
- 单线程 QPS 10万+
- 支持百万级长连接
- 零拷贝序列化与缓冲区管理
- 简洁的协程异步编程模型

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│                   (RPC Services / Handlers)                  │
├─────────────────────────────────────────────────────────────┤
│                        RPC Layer                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ RpcServer    │  │ RpcClient    │  │ ServiceRegistry│      │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ Protocol     │  │ Serializer   │  │ LoadBalancer │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
├─────────────────────────────────────────────────────────────┤
│                       Net Layer                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ TcpServer    │  │ TcpClient    │  │ Connection    │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
│  ┌──────────────┐  ┌──────────────┐                         │
│  │ Buffer       │  │ ConnPool     │                         │
│  └──────────────┘  └──────────────┘                         │
├─────────────────────────────────────────────────────────────┤
│                        IO Layer                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ IOUring      │  │ EventLoop    │  │ AsyncIO      │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
├─────────────────────────────────────────────────────────────┤
│                    Coroutine Layer                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ Task<T>      │  │ Generator<T> │  │ Scheduler    │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
│  ┌──────────────┐  ┌──────────────┐                         │
│  │ Awaitable    │  │ Sync         │                         │
│  └──────────────┘  └──────────────┘                         │
└─────────────────────────────────────────────────────────────┘
```

## 3. 核心模块设计

### 3.1 协程层 (Coroutine Layer)

#### Task<T> - 协程任务
```cpp
template<typename T>
class Task {
    // 协程返回值类型
    using value_type = T;
    
    // 协程句柄
    std::coroutine_handle<promise_type> handle_;
    
    // 支持 co_await
    auto operator co_await();
};
```

**关键特性：**
- 零开销抽象：编译器生成状态机
- 惰式执行：创建时不执行，co_await 时执行
- 异常传播：协程内异常可传播到调用者

#### Generator<T> - 协程生成器
```cpp
template<typename T>
class Generator {
    // 迭代器支持
    class iterator;
    iterator begin();
    iterator end();
};
```

**使用场景：**
- 懒加载序列
- 无限序列
- 大数据分批处理

### 3.2 IO层 (IO Layer)

#### io_uring - Linux 异步I/O

**核心原理：**
```
┌─────────────┐     ┌─────────────┐
│ Submission  │     │ Completion  │
│   Queue     │────▶│   Queue     │
│  (SQ)       │     │  (CQ)       │
└─────────────┘     └─────────────┘
      │                    │
      │                    │
      ▼                    ▼
   提交请求            完成通知
```

**优势对比：**

| 特性 | epoll | io_uring |
|------|-------|----------|
| 系统调用次数 | 每次操作都需调用 | 批量提交/收割 |
| 数据拷贝 | 用户态/内核态拷贝 | 零拷贝 |
| 扩展性 | 限于文件描述符 | 支持任意异步操作 |
| 性能 | 基准 | 2-5倍提升 |

#### EventLoop - 事件循环
```cpp
class EventLoop {
    void run();                    // 运行循环
    void stop();                   // 停止循环
    void post(Task<> task);        // 投递任务
    void schedule(Task<> task,     // 延迟调度
                  Duration delay);
};
```

### 3.3 网络层 (Net Layer)

#### Buffer - 缓冲区管理
```cpp
class Buffer {
    // 零拷贝设计
    char* data_;          // 数据指针
    size_t capacity_;     // 容量
    size_t read_pos_;     // 读位置
    size_t write_pos_;    // 写位置
};
```

#### Connection - 连接抽象
```cpp
class Connection {
    // 异步读写（协程）
    Task<ssize_t> async_read(void* buf, size_t len);
    Task<ssize_t> async_write(const void* buf, size_t len);
    
    // 状态管理
    bool connected() const;
    void close();
};
```

#### TcpServer - TCP服务器
```cpp
struct TcpServerConfig {
    uint16_t port = 8080;
    size_t max_connections = 10000;
    size_t buffer_size = 8192;
    bool reuse_addr = true;
    bool tcp_nodelay = true;
};

class TcpServer {
    void set_connection_handler(Handler h);
    bool start();
    void run();
};
```

### 3.4 RPC层 (RPC Layer)

#### 协议设计
```
┌────────────────────────────────────────────────────────────┐
│ Magic(4B) │ Version(1B) │ Type(1B) │ Length(4B) │ ...     │
├────────────────────────────────────────────────────────────┤
│ RequestID(8B) │ ServiceLen(2B) │ MethodLen(2B) │ ...      │
├────────────────────────────────────────────────────────────┤
│ ServiceName │ MethodName │ PayloadLength(4B) │ Payload    │
└────────────────────────────────────────────────────────────┘
```

#### 序列化
```cpp
// 二进制序列化（高性能）
class BinarySerializer {
    static void encode(vector<uint8_t>& buf, int32_t val);
    static void encode(vector<uint8_t>& buf, const string& val);
    static void encode(vector<uint8_t>& buf, const vector<T>& val);
    
    static DecodeResult decode(const uint8_t* data, size_t len, T& val);
};
```

#### 服务注册
```cpp
class ServiceRegistry {
    void register_method(string service, string method, 
                         MethodHandler handler, string desc);
    MethodHandler get_method(string service, string method);
    vector<string> list_services();
    vector<string> list_methods(string service);
};
```

### 3.5 IDL工具 (IDL Tool)

#### IDL语法示例
```idl
namespace calculator

struct AddRequest {
    int32 a;
    int32 b;
}

struct AddResponse {
    int32 result;
}

service Calculator {
    AddResponse add(AddRequest req);
}
```

#### 代码生成
- 自动生成 C++ 结构体定义
- 自动生成序列化/反序列化代码
- 自动生成客户端代理类
- 自动生成服务端骨架

## 4. 性能优化策略

### 4.1 零拷贝
- Buffer 使用指针操作，避免数据拷贝
- io_uring 直接 I/O，绕过页缓存
- 序列化直接写入输出缓冲区

### 4.2 批量处理
- io_uring 批量提交 SQE
- 批量收割 CQE
- 连接池批量预分配

### 4.3 内存管理
- Buffer 池化复用
- 连接预分配缓冲区
- 避免频繁 new/delete

### 4.4 锁优化
- 单线程事件循环，无锁设计
- 连接池使用原子操作
- 服务注册使用读写锁

## 5. 扩展点

### 5.1 自定义序列化
```cpp
template<>
std::vector<uint8_t> serialize(const MyType& obj);

template<>
bool deserialize(const std::vector<uint8_t>& buf, MyType& obj);
```

### 5.2 自定义负载均衡
```cpp
class MyLoadBalancer : public LoadBalancer {
    void update(const vector<Endpoint>& eps) override;
    optional<Endpoint> select() override;
};
```

### 5.3 中间件
```cpp
server.add_middleware([](Context& ctx, Next next) {
    // 前置处理
    auto result = next();
    // 后置处理
    return result;
});

## 架构修复与演进

### v1.1 修复：EventLoop io_uring 实例统一

**问题根因**：`EventLoop` 类在构造函数中创建独立的 `IoUring io_uring_` 成员，而 `async_accept/async_recv/async_send` 等 awaiter 直接使用 `global_io_uring()` 静态单例。两个不同的 `io_uring` 实例导致：
- 协程提交的 SQE 进入 ring A（global_io_uring）
- EventLoop.run_once() 从 ring B（io_uring_）收割 CQE
- 完成回调永远不触发，协程永久挂起

**修复方案**：移除 `EventLoop` 的 `io_uring_` 成员和 `config_` 成员，所有 io_uring 操作改为调用 `global_io_uring()`，确保进程内唯一 io_uring 实例。

**设计原则**：异步框架中，I/O 提交端和完成收割端必须使用同一个 io_uring 实例。单例模式在此场景下是强制要求，不是可选优化。

### v1.1 修复：run_once 批量 CQE 处理

**问题根因**：`IoUring::run_once()` 在 `wait_cqe()` 返回后只处理一个 CQE，忽略同轮其他已完成事件。io_uring 的完成队列（CQ）是批量机制，一次唤醒可能有多条 CQE 就绪。

**修复方案**：处理首个 CQE 后追加 `process_all_cqe()` 调用，确保每次事件循环迭代处理所有就绪 CQE。

### 测试覆盖

| 层级 | 数量 | 覆盖范围 |
|------|------|----------|
| 单元测试 | 3 | coroutine/Task, Buffer, Serializer |
| 端到端测试 | 13 | 协议往返, 服务注册, TCP通信, io_uring, 大负载, Buffer网络 |
| 性能基准 | 4 | 协程QPS, 序列化QPS, Buffer QPS, RPC查找QPS |
