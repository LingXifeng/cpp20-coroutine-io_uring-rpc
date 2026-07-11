/**
 * @file test_rpc_call.cpp
 * @brief 真实 RPC 完整调用示例: client 调 add(1,2) → server 返回 3
 */
#include "../framework.hpp"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace rpc;

// ==================== 服务方法实现 ====================

// add(a, b) -> a + b
static std::vector<uint8_t> math_add(const std::vector<uint8_t>& payload) {
    int32_t a, b;
    BinarySerializer::decode(payload.data(), payload.size(), a);
    BinarySerializer::decode(payload.data() + 4, payload.size() - 4, b);
    int32_t result = a + b;
    std::vector<uint8_t> out;
    BinarySerializer::encode(out, result);
    return out;
}

// multiply(a, b) -> a * b
static std::vector<uint8_t> math_multiply(const std::vector<uint8_t>& payload) {
    int32_t a, b;
    BinarySerializer::decode(payload.data(), payload.size(), a);
    BinarySerializer::decode(payload.data() + 4, payload.size() - 4, b);
    int32_t result = a * b;
    std::vector<uint8_t> out;
    BinarySerializer::encode(out, result);
    return out;
}

// ==================== 协程 RPC Server ====================

void run_rpc_server(uint16_t port, std::atomic<bool>& server_ready,
                    std::atomic<bool>& server_done, int max_requests) {
    // 注册服务方法
    auto& registry = rpc::ServiceRegistry::instance();
    registry.register_method("MathService", "add",
        [](const std::vector<uint8_t>& payload) -> coroutine::Task<std::vector<uint8_t>> {
            co_return math_add(payload);
        }, "Add two integers");
    registry.register_method("MathService", "multiply",
        [](const std::vector<uint8_t>& payload) -> coroutine::Task<std::vector<uint8_t>> {
            co_return math_multiply(payload);
        }, "Multiply two integers");

    // 创建 listening socket
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
        ::listen(listen_fd, 16) < 0) {
        std::cerr << "Server: bind/listen failed" << std::endl;
        return;
    }

    server_ready.store(true);
    std::cout << "RPC Server listening on port " << port << std::endl;

    int client_fd = ::accept(listen_fd, nullptr, nullptr);
    if (client_fd < 0) {
        std::cerr << "Server: accept failed" << std::endl;
        ::close(listen_fd);
        return;
    }
    std::cout << "Server: accepted connection" << std::endl;

    // 协程处理 RPC 请求
    auto handle_rpc = [client_fd, max_requests, &registry]() -> coroutine::Task<void> {
        char buf[65536];
        int count = 0;

        while (count < max_requests) {
            // 1. 读取协议头
            ssize_t n = co_await io::async_recv(client_fd, buf, sizeof(rpc::ProtocolHeader), MSG_WAITALL);
            if (n <= 0) break;

            rpc::ProtocolHeader header;
            std::memcpy(&header, buf, sizeof(header));
            if (!header.valid()) {
                std::cerr << "Server: invalid header" << std::endl;
                break;
            }

            // 2. 读取剩余数据
            size_t remaining = header.meta_len + header.payload_len;
            if (remaining > 0) {
                n = co_await io::async_recv(client_fd, buf + sizeof(header), remaining, MSG_WAITALL);
                if (n <= 0) break;
            }

            // 3. 反序列化
            size_t total = sizeof(header) + remaining;
            rpc::RpcMessage msg;
            if (!rpc::RpcMessage::deserialize(reinterpret_cast<uint8_t*>(buf), total, msg)) {
                std::cerr << "Server: deserialize failed" << std::endl;
                break;
            }

            std::cout << "Server: recv request id=" << msg.request_id()
                      << " service=" << msg.service()
                      << " method=" << msg.method() << std::endl;

            // 4. 查找并调用方法
            auto handler = registry.find_method(msg.service(), msg.method());
            if (!handler) {
                std::cerr << "Server: method not found" << std::endl;
                break;
            }

            auto result = co_await handler(msg.payload());

            // 5. 构造并发送响应
            auto response = rpc::RpcMessage::create_response(msg.request_id(), result);
            auto resp_data = response.serialize();

            ssize_t w = co_await io::async_send(client_fd, resp_data.data(), resp_data.size(), 0);
            if (w <= 0) break;

            std::cout << "Server: sent response (" << resp_data.size() << " bytes)" << std::endl;
            ++count;
        }
        co_return;
    };

    auto task = handle_rpc();
    task.start();

    auto& ring = io::global_io_uring();
    for (int i = 0; i < 200 && !server_done.load(); ++i) {
        ring.run_once(50);
    }

    ::close(client_fd);
    ::close(listen_fd);
    std::cout << "Server: done" << std::endl;
}

// ==================== RPC Client ====================

bool rpc_call(int fd, uint32_t request_id,
              const std::string& service, const std::string& method,
              const std::vector<uint8_t>& payload,
              int32_t& out_result) {
    auto msg = rpc::RpcMessage::create_request(request_id, service, method, payload);
    auto data = msg.serialize();

    ssize_t w = ::send(fd, data.data(), data.size(), MSG_NOSIGNAL);
    if (w != (ssize_t)data.size()) return false;

    std::vector<uint8_t> resp_buf(65536);
    ssize_t r = ::recv(fd, resp_buf.data(), sizeof(rpc::ProtocolHeader), MSG_WAITALL);
    if (r <= 0) return false;

    rpc::ProtocolHeader header;
    std::memcpy(&header, resp_buf.data(), sizeof(header));
    if (!header.valid()) return false;

    size_t remaining = header.meta_len + header.payload_len;
    if (remaining > 0) {
        r = ::recv(fd, resp_buf.data() + sizeof(header), remaining, MSG_WAITALL);
        if (r <= 0) return false;
    }

    size_t total = sizeof(header) + remaining;
    rpc::RpcMessage resp;
    if (!rpc::RpcMessage::deserialize(resp_buf.data(), total, resp)) return false;

    BinarySerializer::decode(resp.payload().data(), resp.payload().size(), out_result);
    return true;
}

int main() {
    std::cout << "=== Real RPC Call Example ===" << std::endl;
    std::cout << "Client calls MathService.add(1,2) -> expects 3" << std::endl;
    std::cout << "Client calls MathService.multiply(3,4) -> expects 12" << std::endl;
    std::cout << std::endl;

    if (!rpc::init()) {
        std::cerr << "FATAL: rpc::init() failed" << std::endl;
        return 1;
    }

    const uint16_t port = 19998;
    std::atomic<bool> server_ready{false}, server_done{false};

    std::thread server_thread(run_rpc_server, port,
                              std::ref(server_ready), std::ref(server_done), 2);

    for (int i = 0; i < 50 && !server_ready.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Client: connect failed" << std::endl;
        server_done.store(true);
        server_thread.join();
        return 1;
    }
    std::cout << "Client: connected" << std::endl;

    struct timeval tv{5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    bool pass = true;

    // === add(1, 2) ===
    {
        std::vector<uint8_t> payload;
        BinarySerializer::encode(payload, int32_t(1));
        BinarySerializer::encode(payload, int32_t(2));

        int32_t result = 0;
        std::cout << ">>> RPC Call: MathService.add(1, 2)" << std::endl;
        bool ok = rpc_call(fd, 1, "MathService", "add", payload, result);
        std::cout << "<<< RPC Response: result = " << result << std::endl;
        if (!ok || result != 3) {
            std::cerr << "[FAIL] add(1,2) expected 3, got " << result << std::endl;
            pass = false;
        } else {
            std::cout << "[PASS] add(1,2) = 3" << std::endl;
        }
    }

    // === multiply(3, 4) ===
    {
        std::vector<uint8_t> payload;
        BinarySerializer::encode(payload, int32_t(3));
        BinarySerializer::encode(payload, int32_t(4));

        int32_t result = 0;
        std::cout << ">>> RPC Call: MathService.multiply(3, 4)" << std::endl;
        bool ok = rpc_call(fd, 2, "MathService", "multiply", payload, result);
        std::cout << "<<< RPC Response: result = " << result << std::endl;
        if (!ok || result != 12) {
            std::cerr << "[FAIL] multiply(3,4) expected 12, got " << result << std::endl;
            pass = false;
        } else {
            std::cout << "[PASS] multiply(3,4) = 12" << std::endl;
        }
    }

    ::close(fd);
    server_done.store(true);

    for (int i = 0; i < 30 && !server_done.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server_thread.join();

    std::cout << std::endl;
    std::cout << (pass ? "[PASS]" : "[FAIL]") << " Real RPC Complete Call Chain" << std::endl;
    return pass ? 0 : 1;
}
