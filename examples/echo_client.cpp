/**
 * @file echo_client.cpp
 * @brief Echo客户端示例
 */

#include "../framework.hpp"
#include <iostream>

using namespace rpc;

int main() {
    std::cout << "Echo Client Example" << std::endl;
    std::cout << "===================" << std::endl;
    
    if (!rpc::init()) {
        std::cerr << "Failed to initialize framework" << std::endl;
        return 1;
    }
    
    net::TcpClient client;
    std::cout << "Echo client created successfully" << std::endl;
    
    return 0;
}
