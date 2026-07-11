/**
 * @file echo_server.cpp
 * @brief Echo服务器示例
 */

#include "../framework.hpp"
#include <iostream>

using namespace rpc;

int main() {
    std::cout << "Echo Server Example" << std::endl;
    std::cout << "===================" << std::endl;
    
    if (!rpc::init()) {
        std::cerr << "Failed to initialize framework" << std::endl;
        return 1;
    }
    
    net::TcpServerConfig config;
    config.port = 8080;
    net::TcpServer server(config);
    std::cout << "Echo server listening on port 8080" << std::endl;
    std::cout << "Server created successfully" << std::endl;
    
    return 0;
}
