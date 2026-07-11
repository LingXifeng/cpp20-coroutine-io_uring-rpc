/**
 * @file tcp_server.hpp
 * @brief TCP服务器 - 基于协程和io_uring
 * @author RPC Framework
 * @version 1.0
 */

#pragma once

#include "connection.hpp"
#include "../io/io.hpp"
#include "../coroutine/coroutine.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <atomic>
#include <functional>

namespace rpc {
namespace net {

/**
 * @brief TCP服务器配置
 */
struct TcpServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    int backlog = 128;                  // listen backlog
    bool reuse_addr = true;             // SO_REUSEADDR
    bool reuse_port = true;             // SO_REUSEPORT (Linux 3.9+)
    size_t max_connections = 100000;    // 最大连接数
    int read_timeout_ms = 30000;        // 读超时
    int write_timeout_ms = 30000;       // 写超时
    bool enable_metrics = true;         // 启用统计
};

/**
 * @brief TCP服务器
 * 
 * 高性能TCP服务器，使用io_uring和协程
 */
class TcpServer {
public:
    using Ptr = std::shared_ptr<TcpServer>;
    using ConnectionHandler = std::function<rpc::coroutine::Task<void>(Connection::Ptr)>;
    
    explicit TcpServer(const TcpServerConfig& config = TcpServerConfig{})
        : config_(config)
        , listen_fd_(-1)
        , running_(false)
        , connections_(0)
    {}
    
    ~TcpServer() {
        stop();
    }
    
    /**
     * @brief 设置连接处理器
     */
    void set_connection_handler(ConnectionHandler handler) {
        connection_handler_ = std::move(handler);
    }
    
    /**
     * @brief 启动服务器
     */
    bool start() {
        if (running_.load()) return true;
        
        // 创建监听socket
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (listen_fd_ < 0) {
            return false;
        }
        
        // 设置socket选项
        apply_socket_options();
        
        // 绑定地址
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config_.port);
        inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);
        
        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }
        
        // 开始监听
        if (::listen(listen_fd_, config_.backlog) < 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }
        
        running_.store(true);
        
        // 启动接受连接协程
        accept_loop();
        
        return true;
    }
    
    /**
     * @brief 停止服务器
     */
    void stop() {
        if (!running_.load()) return;
        
        running_.store(false);
        
        if (listen_fd_ >= 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
    }
    
    /**
     * @brief 运行事件循环
     */
    void run() {
        auto& loop = rpc::io::global_event_loop();
        loop.run();
    }
    
    // ==================== 统计信息 ====================
    
    size_t active_connections() const { return connections_.load(); }
    uint64_t total_accepted() const { return total_accepted_.load(); }
    uint64_t total_bytes_read() const { return total_bytes_read_.load(); }
    uint64_t total_bytes_written() const { return total_bytes_written_.load(); }
    
private:
    TcpServerConfig config_;
    int listen_fd_;
    std::atomic<bool> running_;
    
    ConnectionHandler connection_handler_;
    
    // 统计
    std::atomic<size_t> connections_;
    std::atomic<uint64_t> total_accepted_{0};
    std::atomic<uint64_t> total_bytes_read_{0};
    std::atomic<uint64_t> total_bytes_written_{0};
    
    void apply_socket_options() {
        if (config_.reuse_addr) {
            int opt = 1;
            ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        }
        
        if (config_.reuse_port) {
            int opt = 1;
            ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        }
    }
    
    /**
     * @brief 接受连接循环
     */
    void accept_loop() {
        // 使用协程接受连接
        auto accept_task = [this]() -> rpc::coroutine::Task<void> {
            while (running_.load()) {
                sockaddr_in client_addr{};
                socklen_t addr_len = sizeof(client_addr);
                
                int client_fd = co_await rpc::io::async_accept(
                    listen_fd_,
                    reinterpret_cast<sockaddr*>(&client_addr),
                    &addr_len,
                    0
                );
                
                if (client_fd < 0) {
                    continue;
                }
                
                // 检查连接数限制
                if (connections_.load() >= config_.max_connections) {
                    ::close(client_fd);
                    continue;
                }
                
                connections_.fetch_add(1);
                total_accepted_.fetch_add(1);
                
                // 创建连接对象
                auto conn = Connection::create(client_fd);
                
                // 处理连接
                if (connection_handler_) {
                    // 启动连接处理协程
                    handle_connection(conn);
                }
            }
        };
        
        // 启动协程
        auto task = accept_task();
        auto& loop = rpc::io::global_event_loop();
        loop.spawn(std::move(task));
    }
    
    /**
     * @brief 处理连接
     */
    void handle_connection(Connection::Ptr conn) {
        auto handler_task = [this, conn]() -> rpc::coroutine::Task<void> {
            // 设置关闭回调
            conn->set_close_callback([this](Connection::Ptr) {
                connections_.fetch_sub(1);
            });
            
            try {
                co_await connection_handler_(conn);
            } catch (...) {
                // 异常处理
            }
            
            conn->close();
        };
        
        auto task = handler_task();
        auto& loop = rpc::io::global_event_loop();
        loop.spawn(std::move(task));
    }
};

} // namespace net
} // namespace rpc
