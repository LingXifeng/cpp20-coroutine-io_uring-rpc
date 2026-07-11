/**
 * @file test_buffer.cpp
 * @brief Buffer测试
 */

#include "../framework.hpp"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace rpc;

int main() {
    std::cout << "=== Buffer Tests ===" << std::endl;
    
    net::Buffer buf(1024);
    std::cout << "Buffer capacity: " << buf.capacity() << std::endl;
    
    const char* data = "Hello, RPC!";
    buf.write(data, strlen(data));
    std::cout << "Written " << strlen(data) << " bytes" << std::endl;
    std::cout << "Readable: " << buf.readable() << std::endl;
    
    char read_buf[64] = {};
    buf.read(read_buf, strlen(data));
    std::cout << "Read: " << read_buf << std::endl;
    assert(strcmp(read_buf, "Hello, RPC!") == 0);
    
    std::cout << "[PASS] Buffer read/write works correctly" << std::endl;
    std::cout << "\nAll buffer tests passed!" << std::endl;
    return 0;
}
