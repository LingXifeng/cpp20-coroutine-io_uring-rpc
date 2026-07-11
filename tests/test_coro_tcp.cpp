/**
 * @file test_coro_tcp.cpp
 * @brief 协程+io_uring TCP echo 端到端测试
 */
#include "../framework.hpp"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace rpc;

int main() {
    std::cout << "=== Coroutine + io_uring TCP Echo Test ===" << std::endl;

    // 初始化框架
    if (!rpc::init()) {
        std::cerr << "FATAL: rpc::init() failed" << std::endl;
        return 1;
    }
    std::cout << "Framework initialized" << std::endl;

    const uint16_t port = 19999;

    // 创建阻塞 listening socket
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { std::cerr << "socket failed" << std::endl; return 1; }
    int opt = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind failed" << std::endl; ::close(listen_fd); return 1;
    }
    if (::listen(listen_fd, 16) < 0) {
        std::cerr << "listen failed" << std::endl; ::close(listen_fd); return 1;
    }
    std::cout << "Listening on port " << port << std::endl;

    // 在后台线程：accept 一个连接，然后用协程做 echo
    std::atomic<bool> test_done{false};
    std::atomic<bool> echo_ok{false};

    std::thread accept_thread([&]() {
        // 阻塞 accept
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        if (client_fd < 0) {
            std::cerr << "accept failed: " << strerror(errno) << std::endl;
            test_done.store(true);
            return;
        }
        std::cout << "Accepted connection fd=" << client_fd << std::endl;

        // 设置 recv 超时
        struct timeval tv{3, 0};
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // 用协程做 echo：co_await async_recv + co_await async_send
        auto echo_coro = [client_fd, &echo_ok]() -> coroutine::Task<void> {
            char buf[1024];
            int count = 0;
            for (int i = 0; i < 3; ++i) {
                ssize_t n = co_await io::async_recv(client_fd, buf, sizeof(buf), 0);
                if (n <= 0) {
                    std::cerr << "async_recv failed: n=" << n << std::endl;
                    break;
                }
                std::cout << "Server recv " << n << " bytes" << std::endl;

                ssize_t w = co_await io::async_send(client_fd, buf, n, 0);
                if (w <= 0) {
                    std::cerr << "async_send failed: w=" << w << std::endl;
                    break;
                }
                std::cout << "Server sent " << w << " bytes" << std::endl;
                ++count;
            }
            echo_ok.store(count == 3);
            co_return;
        };

        // 启动协程
        auto task = echo_coro();
        task.start();  // 直接启动，运行到第一个 co_await

        // 驱动 io_uring 事件循环
        auto& ring = io::global_io_uring();
        for (int i = 0; i < 100 && !echo_ok.load(); ++i) {
            ring.run_once(50);
        }

        ::close(client_fd);
        test_done.store(true);
        std::cout << "Server thread done, echo_ok=" << echo_ok.load() << std::endl;
    });

    // 等待 server 就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Client: 连接并发送3条消息
    int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (::connect(client_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "connect failed" << std::endl;
        test_done.store(true);
        accept_thread.join();
        ::close(listen_fd);
        return 1;
    }
    std::cout << "Client connected" << std::endl;

    const char* messages[] = {"Hello", "World", "RPC!"};
    bool all_ok = true;

    for (int i = 0; i < 3; ++i) {
        size_t len = strlen(messages[i]);
        ssize_t w = ::send(client_fd, messages[i], len, MSG_NOSIGNAL);
        if (w != (ssize_t)len) { all_ok = false; break; }
        std::cout << "Client sent: " << messages[i] << std::endl;

        // 给 io_uring 时间处理
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        char buf[1024];
        ssize_t r = ::recv(client_fd, buf, len, MSG_WAITALL);
        if (r != (ssize_t)len) { all_ok = false; break; }
        buf[r] = 0;
        std::cout << "Client recv: " << buf << std::endl;
        if (strcmp(buf, messages[i]) != 0) { all_ok = false; break; }
    }

    ::close(client_fd);

    // 等待 server 线程完成
    for (int i = 0; i < 30 && !test_done.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    accept_thread.join();
    ::close(listen_fd);

    bool pass = all_ok && echo_ok.load();
    std::cout << std::endl;
    std::cout << (pass ? "[PASS]" : "[FAIL]") << " Coroutine+io_uring TCP Echo" << std::endl;
    return pass ? 0 : 1;
}
