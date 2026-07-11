/**
 * @file test_serializer.cpp
 * @brief 序列化测试
 */

#include "../framework.hpp"
#include <iostream>
#include <cassert>

using namespace rpc;

int main() {
    std::cout << "=== Serializer Tests ===" << std::endl;
    
    // Test int32 encode/decode
    {
        std::vector<uint8_t> buf;
        int32_t val = 12345;
        BinarySerializer::encode(buf, val);
        
        int32_t decoded = 0;
        auto result = BinarySerializer::decode(buf.data(), buf.size(), decoded);
        assert(result.success);
        assert(decoded == val);
        std::cout << "[PASS] int32 encode/decode" << std::endl;
    }
    
    // Test string encode/decode
    {
        std::vector<uint8_t> buf;
        std::string val = "Hello, RPC!";
        BinarySerializer::encode(buf, val);
        
        std::string decoded;
        auto result = BinarySerializer::decode(buf.data(), buf.size(), decoded);
        assert(result.success);
        assert(decoded == val);
        std::cout << "[PASS] string encode/decode" << std::endl;
    }
    
    // Test double encode/decode
    {
        std::vector<uint8_t> buf;
        double val = 3.14159;
        BinarySerializer::encode(buf, val);
        
        double decoded = 0;
        auto result = BinarySerializer::decode(buf.data(), buf.size(), decoded);
        assert(result.success);
        assert(decoded == val);
        std::cout << "[PASS] double encode/decode" << std::endl;
    }
    
    std::cout << "\nAll serializer tests passed!" << std::endl;
    return 0;
}
