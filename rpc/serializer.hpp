/**
 * @file serializer.hpp
 * @brief 序列化器 - 支持多种序列化格式
 * @author RPC Framework
 * @version 1.0
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <optional>
#include <map>
#include <any>

namespace rpc {

/**
 * @brief 序列化格式
 */
enum class SerializeFormat : uint8_t {
    Binary = 0,     // 二进制（默认，最高效）
    Json = 1,       // JSON（调试友好）
    MessagePack = 2,// MessagePack（紧凑）
};

/**
 * @brief 二进制序列化器
 * 
 * 简单高效的二进制序列化，支持基本类型和容器
 */
class BinarySerializer {
public:
    // ==================== 编码 ====================
    
    static void encode(std::vector<uint8_t>& buffer, bool value) {
        buffer.push_back(value ? 1 : 0);
    }
    
    static void encode(std::vector<uint8_t>& buffer, int8_t value) {
        buffer.push_back(static_cast<uint8_t>(value));
    }
    
    static void encode(std::vector<uint8_t>& buffer, uint8_t value) {
        buffer.push_back(value);
    }
    
    static void encode(std::vector<uint8_t>& buffer, int16_t value) {
        buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }
    
    static void encode(std::vector<uint8_t>& buffer, uint16_t value) {
        encode(buffer, static_cast<int16_t>(value));
    }
    
    static void encode(std::vector<uint8_t>& buffer, int32_t value) {
        buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }
    
    static void encode(std::vector<uint8_t>& buffer, uint32_t value) {
        encode(buffer, static_cast<int32_t>(value));
    }
    
    static void encode(std::vector<uint8_t>& buffer, int64_t value) {
        for (int i = 0; i < 8; ++i) {
            buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }
    
    static void encode(std::vector<uint8_t>& buffer, uint64_t value) {
        encode(buffer, static_cast<int64_t>(value));
    }
    
    static void encode(std::vector<uint8_t>& buffer, float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        encode(buffer, bits);
    }
    
    static void encode(std::vector<uint8_t>& buffer, double value) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        encode(buffer, bits);
    }
    
    static void encode(std::vector<uint8_t>& buffer, const std::string& value) {
        encode(buffer, static_cast<uint32_t>(value.size()));
        buffer.insert(buffer.end(), value.begin(), value.end());
    }
    
    template<typename T>
    static void encode(std::vector<uint8_t>& buffer, const std::vector<T>& value) {
        encode(buffer, static_cast<uint32_t>(value.size()));
        for (const auto& item : value) {
            encode(buffer, item);
        }
    }
    
    template<typename K, typename V>
    static void encode(std::vector<uint8_t>& buffer, const std::map<K, V>& value) {
        encode(buffer, static_cast<uint32_t>(value.size()));
        for (const auto& [k, v] : value) {
            encode(buffer, k);
            encode(buffer, v);
        }
    }
    
    // ==================== 解码 ====================
    
    struct DecodeResult {
        size_t bytes_read;
        bool success;
    };
    
    static DecodeResult decode(const uint8_t* data, size_t size, bool& value) {
        if (size < 1) return {0, false};
        value = data[0] != 0;
        return {1, true};
    }
    
    static DecodeResult decode(const uint8_t* data, size_t size, int8_t& value) {
        if (size < 1) return {0, false};
        value = static_cast<int8_t>(data[0]);
        return {1, true};
    }
    
    static DecodeResult decode(const uint8_t* data, size_t size, uint8_t& value) {
        if (size < 1) return {0, false};
        value = data[0];
        return {1, true};
    }
    
    static DecodeResult decode(const uint8_t* data, size_t size, int16_t& value) {
        if (size < 2) return {0, false};
        value = static_cast<int16_t>(data[0] | (data[1] << 8));
        return {2, true};
    }
    
    static DecodeResult decode(const uint8_t* data, size_t size, uint16_t& value) {
        int16_t v;
        auto ret = decode(data, size, v);
        value = static_cast<uint16_t>(v);
        return ret;
    }
    
    static DecodeResult decode(const uint8_t* data, size_t size, int32_t& value) {
        if (size < 4) return {0, false};
        value = static_cast<int32_t>(
            data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24)
        );
        return {4, true};
    }
    
    static DecodeResult decode(const uint8_t* data, size_t size, uint32_t& value) {
        int32_t v;
        auto ret = decode(data, size, v);
        value = static_cast<uint32_t>(v);
        return ret;
    }
    
    static DecodeResult decode(const uint8_t* data, size_t size, int64_t& value) {
        if (size < 8) return {0, false};
        value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= static_cast<int64_t>(data[i]) << (i * 8);
        }
        return {8, true};
    }
    
    static DecodeResult decode(const uint8_t* data, size_t size, uint64_t& value) {
        int64_t v;
        auto ret = decode(data, size, v);
        value = static_cast<uint64_t>(v);
        return ret;
    }
    
    static DecodeResult decode(const uint8_t* data, size_t size, float& value) {
        uint32_t bits;
        auto ret = decode(data, size, bits);
        std::memcpy(&value, &bits, sizeof(value));
        return ret;
    }
    
    static DecodeResult decode(const uint8_t* data, size_t size, double& value) {
        uint64_t bits;
        auto ret = decode(data, size, bits);
        std::memcpy(&value, &bits, sizeof(value));
        return ret;
    }
    
    static DecodeResult decode(const uint8_t* data, size_t size, std::string& value) {
        uint32_t len;
        auto ret = decode(data, size, len);
        if (!ret.success || size < ret.bytes_read + len) {
            return {0, false};
        }
        value.assign(reinterpret_cast<const char*>(data + ret.bytes_read), len);
        return {ret.bytes_read + len, true};
    }
    
    template<typename T>
    static DecodeResult decode(const uint8_t* data, size_t size, std::vector<T>& value) {
        uint32_t len;
        auto ret = decode(data, size, len);
        if (!ret.success) return {0, false};
        
        size_t offset = ret.bytes_read;
        value.clear();
        value.reserve(len);
        
        for (uint32_t i = 0; i < len; ++i) {
            T item;
            auto r = decode(data + offset, size - offset, item);
            if (!r.success) return {0, false};
            offset += r.bytes_read;
            value.push_back(std::move(item));
        }
        
        return {offset, true};
    }
};

/**
 * @brief 序列化工具函数
 */
template<typename T>
std::vector<uint8_t> serialize(const T& value) {
    std::vector<uint8_t> buffer;
    BinarySerializer::encode(buffer, value);
    return buffer;
}

template<typename T>
bool deserialize(const uint8_t* data, size_t size, T& value) {
    auto ret = BinarySerializer::decode(data, size, value);
    return ret.success;
}

template<typename T>
bool deserialize(const std::vector<uint8_t>& buffer, T& value) {
    return deserialize(buffer.data(), buffer.size(), value);
}

} // namespace rpc
