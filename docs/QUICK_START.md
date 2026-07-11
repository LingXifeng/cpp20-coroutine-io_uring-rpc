# 快速开始指南

## 1. 环境准备

### 1.1 系统要求

- Ubuntu 22.04+ / Debian 12+ 或其他 Linux 发行版
- Linux 内核 5.4+（io_uring 支持）
- C++20 编译器

### 1.2 安装依赖

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y \
    cmake \
    g++ \
    liburing-dev \
    pkg-config

# 验证编译器版本
g++ --version  # 需要 GCC 13+

# 验证内核版本
uname -r  # 需要 5.4+
```

## 2. 编译项目

```bash
cd ~/rpc

# 创建构建目录
mkdir -p build && cd build

# 配置（Release 模式）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译
make -j$(nproc)

# 运行测试
make test

# 运行性能测试
./tests/benchmark
```

## 3. 编写第一个服务器

### 3.1 Echo 服务器

创建 `my_echo_server.cpp`：

```cpp
#include "../framework.hpp"
#include <iostream>

using namespace rpc;

// 连接处理协程
coroutine::Task<void> handle_connection(net::Connection::Ptr conn) {
    std::cout << "New connection from: " << conn->remote_addr() << std::endl;
    
    char buffer[4096];
    
    while (conn->connected()) {
        // 异步读取
        ssize_t n = co_await conn->async_read(buffer, sizeof(buffer));
        if (n <= 0) break;
        
        // Echo 回去
        ssize_t sent = co_await conn->async_write(buffer, n);
        if (sent <= 0) break;
    }
    
    std::cout << "Connection closed" << std::endl;
}

int main() {
    // 初始化框架
    if (!rpc::init()) {
        std::cerr << "Failed to initialize framework" << std::endl;
        return 1;
    }
    
    // 配置服务器
    net::TcpServerConfig config;
    config.port = 8080;
    config.max_connections = 10000;
    config.tcp_nodelay = true;
    
    // 创建服务器
    auto server = std::make_shared<net::TcpServer>(config);
    server->set_connection_handler(handle_connection);
    
    if (!server->start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    std::cout << "Echo server listening on port 8080" << std::endl;
    
    // 运行事件循环
    server->run();
    
    return 0;
}
```

### 3.2 编译运行

```bash
g++ -std=c++20 -O2 -I~/rpc my_echo_server.cpp -o echo_server -lpthread
./echo_server
```

### 3.3 测试

```bash
# 使用 nc 测试
echo "Hello" | nc localhost 8080

# 使用 telnet
telnet localhost 8080
```

## 4. 编写 RPC 服务

### 4.1 定义服务

```cpp
#include "../framework.hpp"

using namespace rpc;

// 请求/响应结构
struct AddRequest {
    int32_t a;
    int32_t b;
};

struct AddResponse {
    int32_t result;
};

// 序列化（实际项目中由 IDL 工具生成）
namespace rpc::rpc {
template<>
std::vector<uint8_t> serialize(const AddRequest& req) {
    std::vector<uint8_t> buf;
    BinarySerializer::encode(buf, req.a);
    BinarySerializer::encode(buf, req.b);
    return buf;
}

template<>
bool deserialize(const std::vector<uint8_t>& buf, AddRequest& req) {
    size_t offset = 0;
    auto r1 = BinarySerializer::decode(buf.data(), buf.size(), req.a);
    if (!r1.success) return false;
    offset += r1.bytes_read;
    auto r2 = BinarySerializer::decode(buf.data() + offset, 
                                       buf.size() - offset, req.b);
    return r2.success;
}

// AddResponse 序列化类似...
}

// 服务实现
class CalculatorService {
public:
    coroutine::Task<std::vector<uint8_t>> add(
        const std::vector<uint8_t>& data) {
        AddRequest req;
        deserialize(data, req);
        
        AddResponse resp;
        resp.result = req.a + req.b;
        
        co_return serialize(resp);
    }
};
```

### 4.2 注册服务

```cpp
int main() {
    rpc::init();
    
    auto calc = std::make_shared<CalculatorService>();
    
    ServiceRegistry::instance().register_method(
        "Calculator", "add",
        [calc](const std::vector<uint8_t>& data) 
            -> coroutine::Task<std::vector<uint8_t>> {
            co_return co_await calc->add(data);
        },
        "Add two numbers"
    );
    
    std::cout << "Service registered: Calculator.add" << std::endl;
    
    // 启动 RPC 服务器...
}
```

## 5. 使用 IDL 工具

### 5.1 编写 IDL 文件

创建 `calculator.idl`：

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

### 5.2 生成代码

```cpp
#include "idl/idl.hpp"

int main() {
    // 读取 IDL 文件
    std::ifstream file("calculator.idl");
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    // 解析
    rpc::idl::IDLParser parser;
    auto doc = parser.parse(content);
    
    // 生成代码
    rpc::idl::CodeGenerator gen;
    gen.generate(*doc, "calculator");
    
    return 0;
}
```

## 6. 下一步

- 阅读 [架构设计文档](ARCHITECTURE.md)
- 了解 [C++20 协程原理](COROUTINE_PRINCIPLE.md)
- 学习 [io_uring 原理](IO_URING_PRINCIPLE.md)
- 查看 [性能测试报告](PERFORMANCE_REPORT.md)
