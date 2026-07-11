/**
 * @file rpc_example.cpp
 * @brief RPC使用示例
 */

#include "../framework.hpp"
#include <iostream>

using namespace rpc;

int main() {
    std::cout << "RPC Framework Example" << std::endl;
    std::cout << "====================" << std::endl;
    
    if (!rpc::init()) {
        std::cerr << "Failed to initialize framework" << std::endl;
        return 1;
    }
    
    // 注册服务
    ServiceRegistry::instance().register_method(
        "Calculator", "add",
        [](const std::vector<uint8_t>& data) 
            -> coroutine::Task<std::vector<uint8_t>> {
            co_return data;
        },
        "Add two numbers"
    );
    
    std::cout << "Registered service: Calculator" << std::endl;
    std::cout << "  Methods: add" << std::endl;
    
    auto services = ServiceRegistry::instance().list_services();
    std::cout << "\nAll registered services:" << std::endl;
    for (const auto& svc : services) {
        std::cout << "  - " << svc << std::endl;
        auto methods = ServiceRegistry::instance().list_methods(svc);
        for (const auto& method : methods) {
            std::cout << "    * " << method << std::endl;
        }
    }
    
    return 0;
}
