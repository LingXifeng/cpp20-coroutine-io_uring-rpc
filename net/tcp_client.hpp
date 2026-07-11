/**
 * @file tcp_client.hpp
 * @brief TCP客户端 - 基于协程和io_uring
 * @author RPC Framework
 * @version 1.0
 */

#pragma once

#include "connection.hpp"
#include "connection_pool.hpp"
#include "../io/io.hpp"
#include "../coroutine/coroutine.hpp"
#include <atomic>

namespace rpc {
namespace net {

/**
 * @brief TCP客户端配置
 */
struct TcpClientConfig {
    int connect_timeout_ms = 5000;      // 连接超时
    int read_timeout_ms = 30000;        // 读超时
    int write_timeout_ms = 30000;       // 写超时
    bool auto_reconnect = true;         // 自动重连
    int reconnect_delay_ms = 1000;      // 重连延迟
    int max_reconnect_attempts = 3;     // 最大重连次数
    bool enable_connection_pool = true; // 启用连接池
    size_t pool_size = 10;              // 连接池大小
};

/**
 * @brief TCP客户端
 * 
 * 高性能TCP客户端，支持连接池和自动重连
 */
class TcpClient {
public:
    using Ptr = std::shared_ptr<TcpClient>;
    
    explicit TcpClient(const TcpClientConfig& config = TcpClientConfig{})
        : config_(config)
        , connected_(false)
    {}
    
    ~TcpClient() {
        disconnect();
    }
    
    /**
     * @brief 异步连接
     */
    rpc::coroutine::Task<bool> connect(const std::string& host, uint16_t port) {
        host_ = host;
        port_ = port;
        
        if (config_.enable_connection_pool) {
            ConnectionPoolConfig pool_config;
            pool_config.max_connections = config_.pool_size;
            pool_config.connect_timeout_ms = config_.connect_timeout_ms;
            
            pool_ = ConnectionPoolManager::instance().get_pool(host, port, pool_config);
            co_return true;
        }
        
        // 直接连接
        int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd < 0) {
            co_return false;
        }
        
        connection_ = Connection::create(fd);
        bool ok = co_await connection_->async_connect(host, port);
        
        connected_.store(ok);
        co_return ok;
    }
    
    /**
     * @brief 断开连接
     */
    void disconnect() {
        connected_.store(false);
        
        if (connection_) {
            connection_->close();
            connection_.reset();
        }
        
        if (pool_) {
            // 连接池由管理器管理，不需要手动关闭
            pool_.reset();
        }
    }
    
    /**
     * @brief 异步发送数据
     */
    rpc::coroutine::Task<ssize_t> send(const void* data, size_t size) {
        if (pool_) {
            auto conn = pool_->acquire();
            ssize_t n = co_await conn->async_write(data, size);
            pool_->release(conn);
            co_return n;
        }
        
        if (!connection_ || !connected_.load()) {
            co_return -1;
        }
        
        ssize_t n = co_await connection_->async_write(data, size);
        
        if (n <= 0 && config_.auto_reconnect) {
            co_await try_reconnect();
        }
        
        co_return n;
    }
    
    /**
     * @brief 异步接收数据
     */
    rpc::coroutine::Task<ssize_t> recv(void* buffer, size_t size) {
        if (pool_) {
            auto conn = pool_->acquire();
            ssize_t n = co_await conn->async_read(buffer, size);
            pool_->release(conn);
            co_return n;
        }
        
        if (!connection_ || !connected_.load()) {
            co_return -1;
        }
        
        ssize_t n = co_await connection_->async_read(buffer, size);
        
        if (n <= 0 && config_.auto_reconnect) {
            co_await try_reconnect();
        }
        
        co_return n;
    }
    
    /**
     * @brief 异步发送全部数据
     */
    rpc::coroutine::Task<size_t> send_all(const void* data, size_t size) {
        if (pool_) {
            auto conn = pool_->acquire();
            size_t n = co_await conn->async_write_all(data, size);
            pool_->release(conn);
            co_return n;
        }
        
        if (!connection_ || !connected_.load()) {
            co_return 0;
        }
        
        size_t n = co_await connection_->async_write_all(data, size);
        
        if (n < size && config_.auto_reconnect) {
            co_await try_reconnect();
        }
        
        co_return n;
    }
    
    /**
     * @brief 发送请求并等待响应
     */
    rpc::coroutine::Task<std::string> request(const std::string& data) {
        ssize_t sent = co_await send(data.data(), data.size());
        if (sent <= 0) {
            co_return "";
        }
        
        char buffer[4096];
        ssize_t n = co_await recv(buffer, sizeof(buffer));
        if (n <= 0) {
            co_return "";
        }
        
        co_return std::string(buffer, n);
    }
    
    // ==================== 状态查询 ====================
    
    bool connected() const { return connected_.load(); }
    const std::string& host() const { return host_; }
    uint16_t port() const { return port_; }
    
private:
    TcpClientConfig config_;
    std::string host_;
    uint16_t port_ = 0;
    
    Connection::Ptr connection_;
    ConnectionPool::Ptr pool_;
    
    std::atomic<bool> connected_;
    
    /**
     * @brief 尝试重连
     */
    rpc::coroutine::Task<bool> try_reconnect() {
        for (int i = 0; i < config_.max_reconnect_attempts; ++i) {
            // 等待重连延迟
            co_await rpc::coroutine::sleep(
                std::chrono::milliseconds(config_.reconnect_delay_ms)
            );
            
            bool ok = co_await connect(host_, port_);
            if (ok) {
                co_return true;
            }
        }
        
        co_return false;
    }
};

} // namespace net
} // namespace rpc
