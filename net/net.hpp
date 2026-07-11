/**
 * @file net.hpp
 * @brief 网络模块主头文件
 * @author RPC Framework
 */

#pragma once

#include "buffer.hpp"
#include "connection.hpp"
#include "connection_pool.hpp"
#include "tcp_server.hpp"
#include "tcp_client.hpp"

namespace rpc {
namespace net {

constexpr const char* kNetModuleVersion = "1.0.0";

} // namespace net
} // namespace rpc
