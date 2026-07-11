/**
 * @file rpc_server.hpp
 * @brief RPC服务器
 * @author RPC Framework
 * @version 1.0
 */

#pragma once

#include "protocol.hpp"
#include "service_registry.hpp"
#include "../net/net.hpp"
#include "../coroutine/coroutine.hpp"
#include <atomic>
#include <unordered_map>

namespace rpc {

/**
 * @brief RPC服务器配置
 */
struct RpcServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 9000;
    size_t max_connections = 10000;
    int timeout_ms = 30000;
    size_t max_request_size = 10 * 1024 * 1024; // 10MB
    bool enable_metrics = true;
};

/**
 * @brief RPC服务器
 * 
 * 基于协程和io_uring的高性能RPC服务器
 */
class RpcServer {
public:
    using Ptr = std::shared_ptr<RpcServer>;
    
    explicit RpcServer(const RpcServerConfig& config = RpcServerConfig{})
        : config_(config)
        , running_(false)
        , request_id_(0)
    {}
    
    ~RpcServer() {
        stop();
    }
    
    /**
     * @brief 启动服务器
     */
    bool start() {
        if (running_.load()) return true;
        
        net::TcpServerConfig tcp_config;
        tcp_config.host = config_.host;
        tcp_config.port = config_.port;
        tcp_config.max_connections = config_.max_connections;
        tcp_config.read_timeout_ms = config_.timeout_ms;
        
        tcp_server_ = std::make_shared<net::TcpServer>(tcp_config);
        
        // 设置连接处理器
        tcp_server_->set_connection_handler(
            [this](net::Connection::Ptr conn) -> coroutine::Task<void> {
                co_await handle_connection(conn);
            }
        );
        
        if (!tcp_server_->start()) {
            return false;
        }
        
        running_.store(true);
        return true;
    }
    
    /**
     * @brief 停止服务器
     */
    void stop() {
        if (!running_.load()) return;
        
        running_.store(false);
        
        if (tcp_server_) {
            tcp_server_->stop();
        }
    }
    
    /**
     * @brief 运行事件循环
     */
    void run() {
        if (tcp_server_) {
            tcp_server_->run();
        }
    }
    
    /**
     * @brief 注册服务
     */
    template<typename Service>
    void register_service(const std::string& name, Service* service) {
        ServiceRegistry::instance().register_service(name, service);
    }
    
    // ==================== 统计信息 ====================
    
    uint64_t total_requests() const { return total_requests_.load(); }
    uint64_t total_errors() const { return total_errors_.load(); }
    size_t active_connections() const {
        return tcp_server_ ? tcp_server_->active_connections() : 0;
    }
    
private:
    RpcServerConfig config_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> request_id_;
    
    net::TcpServer::Ptr tcp_server_;
    
    // 统计
    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> total_errors_{0};
    
    /**
     * @brief 处理连接
     */
    coroutine::Task<void> handle_connection(net::Connection::Ptr conn) {
        std::vector<uint8_t> buffer;
        buffer.resize(4096);
        
        while (conn->connected()) {
            // 读取协议头
            ssize_t n = co_await conn->async_read(buffer.data(), sizeof(ProtocolHeader));
            if (n <= 0) {
                break;
            }
            
            // 解析协议头
            ProtocolHeader header;
            std::memcpy(&header, buffer.data(), sizeof(header));
            
            if (!header.valid()) {
                total_errors_.fetch_add(1);
                break;
            }
            
            // 读取剩余数据
            size_t total_size = sizeof(ProtocolHeader) + header.meta_len + header.payload_len;
            if (total_size > config_.max_request_size) {
                total_errors_.fetch_add(1);
                break;
            }
            
            if (total_size > buffer.size()) {
                buffer.resize(total_size);
            }
            
            size_t remaining = total_size - sizeof(ProtocolHeader);
            if (remaining > 0) {
                n = co_await conn->async_read(
                    buffer.data() + sizeof(ProtocolHeader), remaining);
                if (n <= 0) {
                    break;
                }
            }
            
            // 处理消息
            auto response = co_await handle_message(buffer.data(), total_size);
            
            // 发送响应
            if (!response.empty()) {
                n = co_await conn->async_write(response.data(), response.size());
                if (n <= 0) {
                    break;
                }
            }
        }
    }
    
    /**
     * @brief 处理消息
     */
    coroutine::Task<std::vector<uint8_t>> handle_message(
        const uint8_t* data, size_t size
    ) {
        RpcMessage msg;
        if (!RpcMessage::deserialize(data, size, msg)) {
            total_errors_.fetch_add(1);
            co_return std::vector<uint8_t>();
        }
        
        total_requests_.fetch_add(1);
        
        if (msg.type() == MessageType::Request) {
            // 查找方法
            auto handler = ServiceRegistry::instance().find_method(
                msg.service(), msg.method());
            
            if (!handler) {
                auto error_msg = RpcMessage::create_error(
                    msg.request_id(),
                    ErrorCode::ServiceNotFound,
                    "Service or method not found"
                );
                co_return error_msg.serialize();
            }
            
            try {
                // 调用方法
                auto result = co_await handler(msg.payload());
                
                auto response = RpcMessage::create_response(
                    msg.request_id(), result);
                co_return response.serialize();
            } catch (...) {
                auto error_msg = RpcMessage::create_error(
                    msg.request_id(),
                    ErrorCode::Unknown,
                    "Internal server error"
                );
                total_errors_.fetch_add(1);
                co_return error_msg.serialize();
            }
        } else if (msg.type() == MessageType::Heartbeat) {
            // 心跳响应
            RpcMessage response;
            response = RpcMessage::create_response(msg.request_id(), {});
            co_return response.serialize();
        }
        
        co_return std::vector<uint8_t>();
    }
};

} // namespace rpc
