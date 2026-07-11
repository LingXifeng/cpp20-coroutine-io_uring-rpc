/**
 * @file async_io.hpp
 * @brief 协程化异步I/O操作
 * @author RPC Framework
 * @version 1.0
 */

#pragma once

#include "io_uring.hpp"
#include "event_loop.hpp"
#include "../coroutine/coroutine.hpp"
#include <sys/uio.h>

namespace rpc {
namespace io {

/**
 * @brief 异步读取Awaiter
 */
class AsyncReadAwaiter {
public:
    AsyncReadAwaiter(int fd, void* buffer, size_t size, off_t offset = 0)
        : fd_(fd), buffer_(buffer), size_(size), offset_(offset) {}
    
    bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        auto& ring = global_io_uring();
        ring.prepare_read(fd_, buffer_, size_, offset_,
            [this, handle](int result, uint32_t) mutable {
                result_ = result;
                handle.resume();
            });
        ring.submit();
    }
    
    ssize_t await_resume() const { return result_; }
    
private:
    int fd_;
    void* buffer_;
    size_t size_;
    off_t offset_;
    ssize_t result_ = 0;
};

/**
 * @brief 异步写入Awaiter
 */
class AsyncWriteAwaiter {
public:
    AsyncWriteAwaiter(int fd, const void* buffer, size_t size, off_t offset = 0)
        : fd_(fd), buffer_(buffer), size_(size), offset_(offset) {}
    
    bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        auto& ring = global_io_uring();
        ring.prepare_write(fd_, buffer_, size_, offset_,
            [this, handle](int result, uint32_t) mutable {
                result_ = result;
                handle.resume();
            });
        ring.submit();
    }
    
    ssize_t await_resume() const { return result_; }
    
private:
    int fd_;
    const void* buffer_;
    size_t size_;
    off_t offset_;
    ssize_t result_ = 0;
};

/**
 * @brief 异步接收Awaiter
 */
class AsyncRecvAwaiter {
public:
    AsyncRecvAwaiter(int fd, void* buffer, size_t size, int flags = 0)
        : fd_(fd), buffer_(buffer), size_(size), flags_(flags) {}
    
    bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        auto& ring = global_io_uring();
        ring.prepare_recv(fd_, buffer_, size_, flags_,
            [this, handle](int result, uint32_t) mutable {
                result_ = result;
                handle.resume();
            });
        ring.submit();
    }
    
    ssize_t await_resume() const { return result_; }
    
private:
    int fd_;
    void* buffer_;
    size_t size_;
    int flags_;
    ssize_t result_ = 0;
};

/**
 * @brief 异步发送Awaiter
 */
class AsyncSendAwaiter {
public:
    AsyncSendAwaiter(int fd, const void* buffer, size_t size, int flags = 0)
        : fd_(fd), buffer_(buffer), size_(size), flags_(flags) {}
    
    bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        auto& ring = global_io_uring();
        ring.prepare_send(fd_, buffer_, size_, flags_,
            [this, handle](int result, uint32_t) mutable {
                result_ = result;
                handle.resume();
            });
        ring.submit();
    }
    
    ssize_t await_resume() const { return result_; }
    
private:
    int fd_;
    const void* buffer_;
    size_t size_;
    int flags_;
    ssize_t result_ = 0;
};

/**
 * @brief 异步接受连接Awaiter
 */
class AsyncAcceptAwaiter {
public:
    AsyncAcceptAwaiter(int fd, sockaddr* addr, socklen_t* addrlen, int flags = 0)
        : fd_(fd), addr_(addr), addrlen_(addrlen), flags_(flags) {}
    
    bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        auto& ring = global_io_uring();
        ring.prepare_accept(fd_, addr_, addrlen_, flags_,
            [this, handle](int result, uint32_t) mutable {
                result_ = result;
                handle.resume();
            });
        ring.submit();
    }
    
    int await_resume() const { return result_; }
    
private:
    int fd_;
    sockaddr* addr_;
    socklen_t* addrlen_;
    int flags_;
    int result_ = -1;
};

/**
 * @brief 异步连接Awaiter
 */
class AsyncConnectAwaiter {
public:
    AsyncConnectAwaiter(int fd, const sockaddr* addr, socklen_t addrlen)
        : fd_(fd), addr_(addr), addrlen_(addrlen) {}
    
    bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        auto& ring = global_io_uring();
        ring.prepare_connect(fd_, addr_, addrlen_,
            [this, handle](int result, uint32_t) mutable {
                result_ = result;
                handle.resume();
            });
        ring.submit();
    }
    
    int await_resume() const { return result_; }
    
private:
    int fd_;
    const sockaddr* addr_;
    socklen_t addrlen_;
    int result_ = -1;
};

// ==================== 便捷函数 ====================

/**
 * @brief 协程化读取
 */
inline auto async_read(int fd, void* buffer, size_t size, off_t offset = 0) {
    return AsyncReadAwaiter(fd, buffer, size, offset);
}

/**
 * @brief 协程化写入
 */
inline auto async_write(int fd, const void* buffer, size_t size, off_t offset = 0) {
    return AsyncWriteAwaiter(fd, buffer, size, offset);
}

/**
 * @brief 协程化接收
 */
inline auto async_recv(int fd, void* buffer, size_t size, int flags = 0) {
    return AsyncRecvAwaiter(fd, buffer, size, flags);
}

/**
 * @brief 协程化发送
 */
inline auto async_send(int fd, const void* buffer, size_t size, int flags = 0) {
    return AsyncSendAwaiter(fd, buffer, size, flags);
}

/**
 * @brief 协程化接受连接
 */
inline auto async_accept(int fd, sockaddr* addr, socklen_t* addrlen, int flags = 0) {
    return AsyncAcceptAwaiter(fd, addr, addrlen, flags);
}

/**
 * @brief 协程化连接
 */
inline auto async_connect(int fd, const sockaddr* addr, socklen_t addrlen) {
    return AsyncConnectAwaiter(fd, addr, addrlen);
}

/**
 * @brief 协程化读取全部数据
 */
rpc::coroutine::Task<std::string> async_read_all(int fd, size_t max_size = 1024 * 1024) {
    std::string result;
    result.resize(4096);
    size_t total = 0;
    
    while (total < max_size) {
        ssize_t n = co_await async_read(fd, result.data() + total, 
                                        result.size() - total, total);
        if (n <= 0) break;
        total += n;
        if (total == result.size()) {
            result.resize(std::min(result.size() * 2, max_size));
        }
    }
    
    result.resize(total);
    co_return result;
}

/**
 * @brief 协程化写入全部数据
 */
rpc::coroutine::Task<size_t> async_write_all(int fd, const void* buffer, size_t size) {
    size_t written = 0;
    const char* ptr = static_cast<const char*>(buffer);
    
    while (written < size) {
        ssize_t n = co_await async_write(fd, ptr + written, size - written, written);
        if (n <= 0) break;
        written += n;
    }
    
    co_return written;
}

} // namespace io
} // namespace rpc
