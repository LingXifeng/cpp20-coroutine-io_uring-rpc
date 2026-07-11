/**
 * @file rpc.hpp
 * @brief RPC模块主头文件
 */

#pragma once

#include "protocol.hpp"
#include "serializer.hpp"
#include "service_registry.hpp"
#include "rpc_server.hpp"
#include "rpc_client.hpp"

namespace rpc {

/**
 * @brief RPC模块版本
 */
constexpr const char* kRpcModuleVersion = "1.0.0";

} // namespace rpc
