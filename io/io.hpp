/**
 * @file io.hpp
 * @brief I/O模块主头文件
 * @author RPC Framework
 */

#pragma once

#include "io_uring.hpp"
#include "event_loop.hpp"
#include "async_io.hpp"

namespace rpc {
namespace io {

constexpr const char* kIoModuleVersion = "1.0.0";

} // namespace io
} // namespace rpc
