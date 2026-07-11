/**
 * @file test_coroutine.cpp
 * @brief 协程模块测试
 */

#include "../framework.hpp"
#include <iostream>
#include <cassert>

using namespace rpc;

// 测试1: 基本Task创建和启动
coroutine::Task<int> simple_task() {
    co_return 42;
}

// 测试2: Task<void>
coroutine::Task<void> void_task() {
    co_return;
}

// 测试3: Generator
coroutine::Generator<int> count_up(int n) {
    for (int i = 0; i < n; ++i) {
        co_yield i;
    }
}

int main() {
    std::cout << "=== Coroutine Tests ===" << std::endl;
    
    // Test Task<int>
    {
        auto task = simple_task();
        int result = task.start().get();
        std::cout << "Task<int> result: " << result << std::endl;
        assert(result == 42);
        std::cout << "[PASS] Task<int> returns correct value" << std::endl;
    }
    
    // Test Task<void>
    {
        auto task = void_task();
        task.start().get();
        std::cout << "[PASS] Task<void> completes" << std::endl;
    }
    
    // Test Generator
    {
        int sum = 0;
        for (int val : count_up(5)) {
            sum += val;
        }
        std::cout << "Generator sum: " << sum << std::endl;
        assert(sum == 10); // 0+1+2+3+4
        std::cout << "[PASS] Generator works correctly" << std::endl;
    }
    
    std::cout << "\nAll coroutine tests passed!" << std::endl;
    return 0;
}
