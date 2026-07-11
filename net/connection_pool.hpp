/**
 * @file connection_pool.hpp
 * @brief 连接池管理 - 支持高并发连接复用
 * @author RPC Framework
 * @version 1.0
 */

#pragma once

#include "connection.hpp"
#include <unordered_map>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace rpc {
namespace net {

/**
 * @brief 连接池配置
 */
struct ConnectionPoolConfig {
    size_t max_connections = 1000;      // 最大连接数
    size_t min_idle = 10;               // 最小空闲连接
    size_t max_idle = 100;              // 最大空闲连接
    int idle_timeout_ms = 60000;        // 空闲超时
    int connect_timeout_ms = 5000;      // 连接超时
    bool enable_health_check = true;    // 启用健康检查
    int health_check_interval_ms = 30000;// 健康检查间隔
};

/**
 * @brief 连接池
 * 
 * 管理到特定目标的连接，支持连接复用
 */
class ConnectionPool {
public:
    using Ptr = std::shared_ptr<ConnectionPool>;
    
    ConnectionPool(const std::string& host, uint16_t port,
                   const ConnectionPoolConfig& config = ConnectionPoolConfig{})
        : host_(host)
        , port_(port)
        , config_(config)
        , total_connections_(0)
        , active_connections_(0)
    {}
    
    ~ConnectionPool() {
        shutdown();
    }
    
    /**
     * @brief 获取连接
     * 从池中获取一个可用连接，如果没有则创建新连接
     */
    Connection::Ptr acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 尝试从空闲列表获取
        while (!idle_connections_.empty()) {
            auto conn = idle_connections_.back();
            idle_connections_.pop_back();
            
            if (conn->connected()) {
                active_connections_.fetch_add(1);
                return conn;
            }
            
            // 连接已失效，移除
            total_connections_.fetch_sub(1);
        }
        
        // 创建新连接
        if (total_connections_.load() < config_.max_connections) {
            lock.unlock();
            
            auto conn = create_connection();
            if (conn) {
                active_connections_.fetch_add(1);
                total_connections_.fetch_add(1);
            }
            return conn;
        }
        
        // 等待可用连接
        cv_.wait(lock, [this] {
            return !idle_connections_.empty() || 
                   total_connections_.load() < config_.max_connections;
        });
        
        // 递归重试
        lock.unlock();
        return acquire();
    }
    
    /**
     * @brief 释放连接
     * 将连接归还到池中
     */
    void release(Connection::Ptr conn) {
        if (!conn) return;
        
        std::unique_lock<std::mutex> lock(mutex_);
        
        active_connections_.fetch_sub(1);
        
        if (conn->connected() && 
            idle_connections_.size() < config_.max_idle) {
            idle_connections_.push_back(conn);
            cv_.notify_one();
        } else {
            conn->close();
            total_connections_.fetch_sub(1);
        }
    }
    
    /**
     * @brief 关闭连接池
     */
    void shutdown() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        for (auto& conn : idle_connections_) {
            conn->close();
        }
        idle_connections_.clear();
        
        total_connections_.store(0);
        active_connections_.store(0);
    }
    
    // ==================== 统计信息 ====================
    
    size_t total() const { return total_connections_.load(); }
    size_t active() const { return active_connections_.load(); }
    size_t idle() const { 
        std::unique_lock<std::mutex> lock(mutex_);
        return idle_connections_.size(); 
    }
    
private:
    std::string host_;
    uint16_t port_;
    ConnectionPoolConfig config_;
    
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Connection::Ptr> idle_connections_;
    
    std::atomic<size_t> total_connections_;
    std::atomic<size_t> active_connections_;
    
    Connection::Ptr create_connection() {
        int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd < 0) return nullptr;
        
        auto conn = Connection::create(fd);
        // 注意：这里需要同步连接或使用协程
        // 简化实现，实际应使用协程
        return conn;
    }
};

/**
 * @brief 连接池管理器
 * 管理多个目标的连接池
 */
class ConnectionPoolManager {
public:
    static ConnectionPoolManager& instance() {
        static ConnectionPoolManager mgr;
        return mgr;
    }
    
    /**
     * @brief 获取或创建连接池
     */
    ConnectionPool::Ptr get_pool(const std::string& host, uint16_t port,
                                  const ConnectionPoolConfig& config = ConnectionPoolConfig{}) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        std::string key = host + ":" + std::to_string(port);
        auto it = pools_.find(key);
        
        if (it != pools_.end()) {
            return it->second;
        }
        
        auto pool = std::make_shared<ConnectionPool>(host, port, config);
        pools_[key] = pool;
        return pool;
    }
    
    /**
     * @brief 移除连接池
     */
    void remove_pool(const std::string& host, uint16_t port) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        std::string key = host + ":" + std::to_string(port);
        auto it = pools_.find(key);
        
        if (it != pools_.end()) {
            it->second->shutdown();
            pools_.erase(it);
        }
    }
    
    /**
     * @brief 关闭所有连接池
     */
    void shutdown_all() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        for (auto& [key, pool] : pools_) {
            pool->shutdown();
        }
        pools_.clear();
    }
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ConnectionPool::Ptr> pools_;
};

/**
 * @brief 连接守卫
 * RAII方式管理连接的获取和释放
 */
class ConnectionGuard {
public:
    ConnectionGuard(ConnectionPool::Ptr pool)
        : pool_(pool)
        , conn_(pool ? pool->acquire() : nullptr)
    {}
    
    ~ConnectionGuard() {
        if (pool_ && conn_) {
            pool_->release(conn_);
        }
    }
    
    Connection::Ptr get() const { return conn_; }
    Connection::Ptr operator->() const { return conn_; }
    explicit operator bool() const { return conn_ != nullptr; }
    
private:
    ConnectionPool::Ptr pool_;
    Connection::Ptr conn_;
};

} // namespace net
} // namespace rpc
