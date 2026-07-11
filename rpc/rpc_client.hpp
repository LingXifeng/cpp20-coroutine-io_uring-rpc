/**
 * @file rpc_client.hpp
 * @brief RPC客户端
 * @author RPC Framework
 * @version 1.0
 */

#pragma once

#include "protocol.hpp"
#include "serializer.hpp"
#include "../net/net.hpp"
#include "../coroutine/coroutine.hpp"
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

namespace rpc {

/**
 * @brief RPC客户端配置
 */
struct RpcClientConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 9000;
    int timeout_ms = 30000;
    bool auto_reconnect = true;
    size_t max_retries = 3;
};

/**
 * @brief RPC调用结果
 */
template<typename T>
struct RpcResult {
    bool success;
    T value;
    ErrorCode error_code;
    std::string error_message;
    
    static RpcResult ok(T value) {
        return {true, std::move(value), ErrorCode::OK, {}};
    }
    
    static RpcResult make_error(ErrorCode code, const std::string& msg = "") {
        return {false, T{}, code, msg};
    }
};

/**
 * @brief RPC客户端
 * 
 * 基于协程的RPC客户端
 */
class RpcClient {
public:
    using Ptr = std::shared_ptr<RpcClient>;
    
    explicit RpcClient(const RpcClientConfig& config = RpcClientConfig{})
        : config_(config)
        , request_id_(0)
    {}
    
    ~RpcClient() {
        disconnect();
    }
    
    /**
     * @brief 连接服务器
     */
    coroutine::Task<bool> connect() {
        return connect(config_.host, config_.port);
    }
    
    /**
     * @brief 连接服务器
     */
    coroutine::Task<bool> connect(const std::string& host, uint16_t port) {
        config_.host = host;
        config_.port = port;
        
        tcp_client_ = std::make_shared<net::TcpClient>();
        bool ok = co_await tcp_client_->connect(host, port);
        co_return ok;
    }
    
    /**
     * @brief 断开连接
     */
    void disconnect() {
        if (tcp_client_) {
            tcp_client_->disconnect();
        }
    }
    
    /**
     * @brief 调用RPC方法
     */
    template<typename Request, typename Response>
    coroutine::Task<RpcResult<Response>> call(
        const std::string& service,
        const std::string& method,
        const Request& request
    ) {
        // 序列化请求
        auto request_data = serialize(request);
        
        // 创建消息
        uint64_t id = next_request_id();
        auto msg = RpcMessage::create_request(
            id, service, method, request_data, config_.timeout_ms);
        
        // 序列化消息
        auto buffer = msg.serialize();
        
        // 发送请求
        ssize_t n = co_await tcp_client_->send(buffer.data(), buffer.size());
        if (n <= 0) {
            co_return RpcResult<Response>::error(
                ErrorCode::ConnectionError, "Failed to send request");
        }
        
        // 接收响应
        std::vector<uint8_t> response_buffer;
        response_buffer.resize(4096);
        
        // 先读取header
        n = co_await tcp_client_->recv(response_buffer.data(), sizeof(ProtocolHeader));
        if (n <= 0) {
            co_return RpcResult<Response>::error(
                ErrorCode::ConnectionError, "Failed to receive response");
        }
        
        // 解析header
        ProtocolHeader header;
        std::memcpy(&header, response_buffer.data(), sizeof(header));
        
        if (!header.valid()) {
            co_return RpcResult<Response>::error(
                ErrorCode::InvalidRequest, "Invalid response header");
        }
        
        // 读取剩余数据
        size_t total_size = sizeof(ProtocolHeader) + header.meta_len + header.payload_len;
        if (total_size > response_buffer.size()) {
            response_buffer.resize(total_size);
        }
        
        size_t remaining = total_size - sizeof(ProtocolHeader);
        if (remaining > 0) {
            n = co_await tcp_client_->recv(
                response_buffer.data() + sizeof(ProtocolHeader), remaining);
            if (n <= 0) {
                co_return RpcResult<Response>::error(
                    ErrorCode::ConnectionError, "Failed to receive response");
            }
        }
        
        // 解析响应
        RpcMessage response;
        if (!RpcMessage::deserialize(response_buffer.data(), total_size, response)) {
            co_return RpcResult<Response>::error(
                ErrorCode::SerializationError, "Failed to deserialize response");
        }
        
        // 检查错误
        if (response.error_code() != ErrorCode::OK) {
            co_return RpcResult<Response>::error(
                response.error_code(), response.error_message());
        }
        
        // 反序列化结果
        Response result;
        if (!deserialize(response.payload(), result)) {
            co_return RpcResult<Response>::error(
                ErrorCode::SerializationError, "Failed to deserialize result");
        }
        
        co_return RpcResult<Response>::ok(std::move(result));
    }
    
    /**
     * @brief 异步调用（不等待响应）
     */
    template<typename Request>
    coroutine::Task<bool> async_call(
        const std::string& service,
        const std::string& method,
        const Request& request
    ) {
        auto request_data = serialize(request);
        uint64_t id = next_request_id();
        auto msg = RpcMessage::create_request(
            id, service, method, request_data, config_.timeout_ms);
        auto buffer = msg.serialize();
        
        ssize_t n = co_await tcp_client_->send(buffer.data(), buffer.size());
        co_return n > 0;
    }
    
    // ==================== 状态查询 ====================
    
    bool connected() const {
        return tcp_client_ && tcp_client_->connected();
    }
    
    uint64_t next_request_id() {
        return request_id_.fetch_add(1);
    }
    
private:
    RpcClientConfig config_;
    std::atomic<uint64_t> request_id_;
    net::TcpClient::Ptr tcp_client_;
};

/**
 * @brief RPC客户端代理
 * 
 * 提供更友好的调用接口
 */
template<typename Service>
class RpcClientProxy {
public:
    RpcClientProxy(RpcClient::Ptr client, const std::string& service_name)
        : client_(client)
        , service_name_(service_name)
    {}
    
    template<typename Request, typename Response>
    coroutine::Task<RpcResult<Response>> call(
        const std::string& method,
        const Request& request
    ) {
        return client_->call<Request, Response>(service_name_, method, request);
    }
    
private:
    RpcClient::Ptr client_;
    std::string service_name_;
};

} // namespace rpc
