/**
 * @file protocol.hpp
 * @brief RPC协议定义
 * @author RPC Framework
 * @version 1.0
 * 
 * 协议格式：
 * +----------------+----------------+------------------+
 * |  Header (16B)  |  Meta (变长)   |  Payload (变长)  |
 * +----------------+----------------+------------------+
 * 
 * Header:
 * - magic (2B): 魔数 0xRPC1
 * - version (1B): 协议版本
 * - type (1B): 消息类型
 * - meta_len (4B): 元数据长度
 * - payload_len (4B): 负载长度
 * - reserved (4B): 保留字段
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <cstring>

namespace rpc {

/**
 * @brief 消息类型
 */
enum class MessageType : uint8_t {
    Request = 0,        // 请求
    Response = 1,       // 响应
    Heartbeat = 2,      // 心跳
    Error = 3,          // 错误
    Stream = 4,         // 流式数据
    StreamEnd = 5,      // 流结束
};

/**
 * @brief 错误码
 */
enum class ErrorCode : int32_t {
    OK = 0,
    Unknown = -1,
    InvalidRequest = -2,
    ServiceNotFound = -3,
    MethodNotFound = -4,
    SerializationError = -5,
    Timeout = -6,
    ConnectionError = -7,
    ServerBusy = -8,
};

/**
 * @brief 协议头
 */
struct alignas(4) ProtocolHeader {
    uint16_t magic;         // 魔数
    uint8_t version;        // 版本
    uint8_t type;           // 消息类型
    uint32_t meta_len;      // 元数据长度
    uint32_t payload_len;   // 负载长度
    uint32_t reserved;      // 保留
    
    static constexpr uint16_t kMagic = 0x5250; // "RP"
    static constexpr uint8_t kVersion = 1;
    
    bool valid() const {
        return magic == kMagic;
    }
    
    void init(MessageType msg_type) {
        magic = kMagic;
        version = kVersion;
        type = static_cast<uint8_t>(msg_type);
        meta_len = 0;
        payload_len = 0;
        reserved = 0;
    }
};

static_assert(sizeof(ProtocolHeader) == 16, "ProtocolHeader size mismatch");

/**
 * @brief 请求元数据
 */
struct RequestMeta {
    uint64_t request_id;    // 请求ID
    uint64_t timestamp;     // 时间戳
    uint32_t timeout_ms;    // 超时时间
    uint32_t service_len;   // 服务名长度
    uint32_t method_len;    // 方法名长度
    // 后面紧跟: service_name + method_name
};

/**
 * @brief 响应元数据
 */
struct ResponseMeta {
    uint64_t request_id;    // 请求ID
    int32_t error_code;     // 错误码
    uint32_t error_len;     // 错误信息长度
    // 后面紧跟: error_message
};

/**
 * @brief RPC消息
 */
class RpcMessage {
public:
    RpcMessage() = default;
    
    // 创建请求
    static RpcMessage create_request(
        uint64_t request_id,
        const std::string& service,
        const std::string& method,
        const std::vector<uint8_t>& payload,
        uint32_t timeout_ms = 30000
    );
    
    // 创建响应
    static RpcMessage create_response(
        uint64_t request_id,
        const std::vector<uint8_t>& payload,
        ErrorCode error = ErrorCode::OK
    );
    
    // 创建错误响应
    static RpcMessage create_error(
        uint64_t request_id,
        ErrorCode error,
        const std::string& message = ""
    );
    
    // 序列化
    std::vector<uint8_t> serialize() const;
    
    // 反序列化
    static bool deserialize(const uint8_t* data, size_t size, RpcMessage& msg);
    
    // 属性访问
    MessageType type() const { return static_cast<MessageType>(header_.type); }
    uint64_t request_id() const { return request_id_; }
    const std::string& service() const { return service_; }
    const std::string& method() const { return method_; }
    const std::vector<uint8_t>& payload() const { return payload_; }
    ErrorCode error_code() const { return error_code_; }
    const std::string& error_message() const { return error_message_; }
    
    bool valid() const { return header_.valid(); }
    size_t total_size() const {
        return sizeof(ProtocolHeader) + header_.meta_len + header_.payload_len;
    }
    
private:
    ProtocolHeader header_{};
    uint64_t request_id_{0};
    std::string service_;
    std::string method_;
    std::vector<uint8_t> payload_;
    ErrorCode error_code_{ErrorCode::OK};
    std::string error_message_;
    uint32_t timeout_ms_{30000};
    uint64_t timestamp_{0};
};

// ==================== 实现 ====================

inline RpcMessage RpcMessage::create_request(
    uint64_t request_id,
    const std::string& service,
    const std::string& method,
    const std::vector<uint8_t>& payload,
    uint32_t timeout_ms
) {
    RpcMessage msg;
    msg.header_.init(MessageType::Request);
    msg.request_id_ = request_id;
    msg.service_ = service;
    msg.method_ = method;
    msg.payload_ = payload;
    msg.timeout_ms_ = timeout_ms;
    msg.timestamp_ = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    return msg;
}

inline RpcMessage RpcMessage::create_response(
    uint64_t request_id,
    const std::vector<uint8_t>& payload,
    ErrorCode error
) {
    RpcMessage msg;
    msg.header_.init(MessageType::Response);
    msg.request_id_ = request_id;
    msg.payload_ = payload;
    msg.error_code_ = error;
    return msg;
}

inline RpcMessage RpcMessage::create_error(
    uint64_t request_id,
    ErrorCode error,
    const std::string& message
) {
    RpcMessage msg;
    msg.header_.init(MessageType::Response);
    msg.request_id_ = request_id;
    msg.error_code_ = error;
    msg.error_message_ = message;
    return msg;
}

inline std::vector<uint8_t> RpcMessage::serialize() const {
    std::vector<uint8_t> buffer;
    
    // 计算大小
    size_t meta_size = 0;
    if (type() == MessageType::Request) {
        meta_size = sizeof(RequestMeta) + service_.size() + method_.size();
    } else {
        meta_size = sizeof(ResponseMeta) + error_message_.size();
    }
    
    size_t total = sizeof(ProtocolHeader) + meta_size + payload_.size();
    buffer.resize(total);
    
    // 写入header
    ProtocolHeader header = header_;
    header.meta_len = static_cast<uint32_t>(meta_size);
    header.payload_len = static_cast<uint32_t>(payload_.size());
    std::memcpy(buffer.data(), &header, sizeof(header));
    
    uint8_t* ptr = buffer.data() + sizeof(header);
    
    // 写入meta
    if (type() == MessageType::Request) {
        RequestMeta meta{};
        meta.request_id = request_id_;
        meta.timestamp = timestamp_;
        meta.timeout_ms = timeout_ms_;
        meta.service_len = static_cast<uint32_t>(service_.size());
        meta.method_len = static_cast<uint32_t>(method_.size());
        
        std::memcpy(ptr, &meta, sizeof(meta));
        ptr += sizeof(meta);
        
        std::memcpy(ptr, service_.data(), service_.size());
        ptr += service_.size();
        
        std::memcpy(ptr, method_.data(), method_.size());
        ptr += method_.size();
    } else {
        ResponseMeta meta{};
        meta.request_id = request_id_;
        meta.error_code = static_cast<int32_t>(error_code_);
        meta.error_len = static_cast<uint32_t>(error_message_.size());
        
        std::memcpy(ptr, &meta, sizeof(meta));
        ptr += sizeof(meta);
        
        if (!error_message_.empty()) {
            std::memcpy(ptr, error_message_.data(), error_message_.size());
            ptr += error_message_.size();
        }
    }
    
    // 写入payload
    if (!payload_.empty()) {
        std::memcpy(ptr, payload_.data(), payload_.size());
    }
    
    return buffer;
}

inline bool RpcMessage::deserialize(const uint8_t* data, size_t size, RpcMessage& msg) {
    if (size < sizeof(ProtocolHeader)) {
        return false;
    }
    
    // 读取header
    std::memcpy(&msg.header_, data, sizeof(ProtocolHeader));
    
    if (!msg.header_.valid()) {
        return false;
    }
    
    size_t expected = sizeof(ProtocolHeader) + msg.header_.meta_len + msg.header_.payload_len;
    if (size < expected) {
        return false;
    }
    
    const uint8_t* ptr = data + sizeof(ProtocolHeader);
    
    // 读取meta
    if (msg.type() == MessageType::Request) {
        RequestMeta meta;
        std::memcpy(&meta, ptr, sizeof(meta));
        ptr += sizeof(meta);
        
        msg.request_id_ = meta.request_id;
        msg.timestamp_ = meta.timestamp;
        msg.timeout_ms_ = meta.timeout_ms;
        
        msg.service_.assign(reinterpret_cast<const char*>(ptr), meta.service_len);
        ptr += meta.service_len;
        
        msg.method_.assign(reinterpret_cast<const char*>(ptr), meta.method_len);
        ptr += meta.method_len;
    } else if (msg.type() == MessageType::Response) {
        ResponseMeta meta;
        std::memcpy(&meta, ptr, sizeof(meta));
        ptr += sizeof(meta);
        
        msg.request_id_ = meta.request_id;
        msg.error_code_ = static_cast<ErrorCode>(meta.error_code);
        
        if (meta.error_len > 0) {
            msg.error_message_.assign(reinterpret_cast<const char*>(ptr), meta.error_len);
            ptr += meta.error_len;
        }
    }
    
    // 读取payload
    if (msg.header_.payload_len > 0) {
        msg.payload_.assign(ptr, ptr + msg.header_.payload_len);
    }
    
    return true;
}

} // namespace rpc
