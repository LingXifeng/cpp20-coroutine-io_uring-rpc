/**
 * @file connection.hpp
 * @brief TCP连接封装 - 支持协程化异步操作
 * @author RPC Framework
 * @version 1.0
 */

#pragma once

#include "buffer.hpp"
#include "../io/io.hpp"
#include "../coroutine/coroutine.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <atomic>
#include <chrono>
#include <functional>

namespace rpc {
namespace net {

/**
 * @brief 连接状态
 */
enum class ConnectionState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Closing,
    Closed
};

/**
 * @brief 连接配置
 */
struct ConnectionConfig {
    int read_timeout_ms = 30000;     // 读超时
    int write_timeout_ms = 30000;    // 写超时
    int connect_timeout_ms = 5000;   // 连接超时
    size_t read_buffer_size = 65536; // 读缓冲区大小
    size_t write_buffer_size = 65536;// 写缓冲区大小
    bool keep_alive = true;          // TCP Keep-Alive
    bool no_delay = true;            // TCP_NODELAY
    int linger_timeout = 0;          // SO_LINGER超时
};

/**
 * @brief TCP连接
 * 
 * 封装socket，提供协程化异步读写接口
 */
class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Ptr = std::shared_ptr<Connection>;
    using WeakPtr = std::weak_ptr<Connection>;
    using CloseCallback = std::function<void(Ptr)>;
    
    /**
     * @brief 创建连接对象
     */
    static Ptr create(int fd, const ConnectionConfig& config = ConnectionConfig{}) {
        return Ptr(new Connection(fd, config));
    }
    
    ~Connection() {
        close();
    }
    
    // ==================== 属性访问 ====================
    
    int fd() const { return fd_; }
    ConnectionState state() const { return state_.load(); }
    bool connected() const { return state_.load() == ConnectionState::Connected; }
    
    void set_close_callback(CloseCallback cb) { close_callback_ = std::move(cb); }
    
    // ==================== 协程化异步操作 ====================
    
    /**
     * @brief 异步连接
     */
    rpc::coroutine::Task<bool> async_connect(const std::string& host, uint16_t port) {
        if (fd_ < 0) {
            fd_ = create_socket();
            if (fd_ < 0) {
                co_return false;
            }
        }
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
        
        state_.store(ConnectionState::Connecting);
        
        int ret = co_await rpc::io::async_connect(fd_, 
            reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        
        if (ret == 0) {
            state_.store(ConnectionState::Connected);
            apply_socket_options();
            co_return true;
        }
        
        state_.store(ConnectionState::Disconnected);
        co_return false;
    }
    
    /**
     * @brief 异步读取
     */
    rpc::coroutine::Task<ssize_t> async_read(void* buffer, size_t size) {
        if (!connected()) {
            co_return -1;
        }
        
        ssize_t n = co_await rpc::io::async_recv(fd_, buffer, size, 0);
        
        if (n <= 0) {
            handle_error();
        }
        
        co_return n;
    }
    
    /**
     * @brief 异步写入
     */
    rpc::coroutine::Task<ssize_t> async_write(const void* buffer, size_t size) {
        if (!connected()) {
            co_return -1;
        }
        
        ssize_t n = co_await rpc::io::async_send(fd_, buffer, size, 0);
        
        if (n <= 0) {
            handle_error();
        }
        
        co_return n;
    }
    
    /**
     * @brief 异步读取到缓冲区
     */
    rpc::coroutine::Task<ssize_t> async_read(Buffer& buffer) {
        size_t writable = buffer.writable();
        if (writable == 0) {
            co_return 0;
        }
        
        ssize_t n = co_await async_read(buffer.write_ptr(), writable);
        if (n > 0) {
            // 需要调整buffer的write_pos，这里简化处理
        }
        co_return n;
    }
    
    /**
     * @brief 异步写入全部数据
     */
    rpc::coroutine::Task<size_t> async_write_all(const void* buffer, size_t size) {
        size_t written = 0;
        const char* ptr = static_cast<const char*>(buffer);
        
        while (written < size && connected()) {
            ssize_t n = co_await async_write(ptr + written, size - written);
            if (n <= 0) break;
            written += n;
        }
        
        co_return written;
    }
    
    /**
     * @brief 异步读取一行
     */
    rpc::coroutine::Task<std::string> async_read_line() {
        std::string line;
        char ch;
        
        while (connected()) {
            ssize_t n = co_await async_read(&ch, 1);
            if (n <= 0) break;
            
            if (ch == '\n') {
                break;
            }
            line.push_back(ch);
        }
        
        co_return line;
    }
    
    /**
     * @brief 关闭连接
     */
    void close() {
        ConnectionState expected = ConnectionState::Connected;
        if (state_.compare_exchange_strong(expected, ConnectionState::Closing)) {
            ::close(fd_);
            fd_ = -1;
            state_.store(ConnectionState::Closed);
            
            if (close_callback_) {
                close_callback_(shared_from_this());
            }
        }
    }
    
    /**
     * @brief 关闭写端
     */
    void shutdown_write() {
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_WR);
        }
    }
    
    // ==================== 统计信息 ====================
    
    uint64_t bytes_read() const { return bytes_read_.load(); }
    uint64_t bytes_written() const { return bytes_written_.load(); }
    std::chrono::steady_clock::time_point last_read_time() const { return last_read_time_; }
    std::chrono::steady_clock::time_point last_write_time() const { return last_write_time_; }
    
private:
    int fd_;
    ConnectionConfig config_;
    std::atomic<ConnectionState> state_;
    CloseCallback close_callback_;
    
    // 统计
    std::atomic<uint64_t> bytes_read_{0};
    std::atomic<uint64_t> bytes_written_{0};
    std::chrono::steady_clock::time_point last_read_time_;
    std::chrono::steady_clock::time_point last_write_time_;
    
    // 缓冲区
    Buffer read_buffer_;
    Buffer write_buffer_;
    
    explicit Connection(int fd, const ConnectionConfig& config)
        : fd_(fd)
        , config_(config)
        , state_(fd >= 0 ? ConnectionState::Connected : ConnectionState::Disconnected)
        , read_buffer_(config.read_buffer_size)
        , write_buffer_(config.write_buffer_size)
    {
        if (fd >= 0) {
            apply_socket_options();
        }
    }
    
    int create_socket() {
        int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        return fd;
    }
    
    void apply_socket_options() {
        if (fd_ < 0) return;
        
        // TCP_NODELAY
        if (config_.no_delay) {
            int flag = 1;
            ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        }
        
        // SO_KEEPALIVE
        if (config_.keep_alive) {
            int flag = 1;
            ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
        }
        
        // SO_LINGER
        if (config_.linger_timeout >= 0) {
            struct linger lg {
                .l_onoff = config_.linger_timeout > 0 ? 1 : 0,
                .l_linger = config_.linger_timeout
            };
            ::setsockopt(fd_, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
        }
    }
    
    void handle_error() {
        close();
    }
};

} // namespace net
} // namespace rpc
