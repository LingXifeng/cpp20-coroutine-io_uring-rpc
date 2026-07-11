/**
 * @file coroutine.hpp
 * @brief 协程模块主头文件
 * @author RPC Framework
 */

#pragma once

#include "task.hpp"
#include "generator.hpp"
#include "scheduler.hpp"
#include "awaitable.hpp"
#include "sync.hpp"

namespace rpc {
namespace coroutine {

/**
 * @brief 协程模块版本
 */
constexpr const char* kCoroutineModuleVersion = "1.0.0";

} // namespace coroutine
} // namespace rpc
