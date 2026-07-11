/**
 * @file framework.hpp
 * @brief RPC Framework 主头文件
 * 
 * 基于 C++20 协程与 io_uring 的高性能网络/RPC 框架
 */

#pragma once

#include "coroutine/coroutine.hpp"
#include "io/io.hpp"
#include "net/net.hpp"
#include "rpc/rpc.hpp"

namespace rpc {

constexpr const char* kFrameworkVersion = "1.0.0";
constexpr const char* kFrameworkName = "RPC Framework";

/**
 * @brief 框架初始化
 */
inline bool init() {
    // io_uring在global_io_uring()首次调用时自动初始化
    auto& ring = io::global_io_uring();
    return ring.is_valid();
}

/**
 * @brief 框架清理
 */
inline void shutdown() {
    net::ConnectionPoolManager::instance().shutdown_all();
}

/**
 * @brief 运行事件循环
 */
inline void run() {
    auto& loop = io::global_event_loop();
    loop.run();
}

} // namespace rpc
