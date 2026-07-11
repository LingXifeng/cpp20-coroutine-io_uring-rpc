/**
 * @file awaitable.hpp
 * @brief 可等待对象 - 异步操作的协程适配器
 */

#pragma once

#include <coroutine>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace rpc {
namespace coroutine {

/**
 * @brief 异步等待器基类
 */
template<typename T>
class AsyncAwaiter {
public:
    bool await_ready() const noexcept {
        return ready_.load(std::memory_order_acquire);
    }
    
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        continuation_ = handle;
    }
    
    T await_resume() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
        return std::move(result_);
    }
    
    void set_result(T value) {
        result_ = std::move(value);
        ready_.store(true, std::memory_order_release);
        if (continuation_) {
            continuation_.resume();
        }
    }
    
    void set_exception(std::exception_ptr e) {
        exception_ = e;
        ready_.store(true, std::memory_order_release);
        if (continuation_) {
            continuation_.resume();
        }
    }
    
protected:
    std::atomic<bool> ready_{false};
    std::coroutine_handle<> continuation_{nullptr};
    T result_;
    std::exception_ptr exception_;
};

/**
 * @brief void特化
 */
template<>
class AsyncAwaiter<void> {
public:
    bool await_ready() const noexcept {
        return ready_.load(std::memory_order_acquire);
    }
    
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        continuation_ = handle;
    }
    
    void await_resume() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }
    
    void set_completed() {
        ready_.store(true, std::memory_order_release);
        if (continuation_) {
            continuation_.resume();
        }
    }
    
    void set_exception(std::exception_ptr e) {
        exception_ = e;
        ready_.store(true, std::memory_order_release);
        if (continuation_) {
            continuation_.resume();
        }
    }
    
protected:
    std::atomic<bool> ready_{false};
    std::coroutine_handle<> continuation_{nullptr};
    std::exception_ptr exception_;
};

/**
 * @brief 延时等待器
 */
class SleepAwaiter {
public:
    explicit SleepAwaiter(std::chrono::milliseconds duration)
        : duration_(duration) {}
    
    bool await_ready() const noexcept { 
        return duration_.count() == 0; 
    }
    
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        std::thread([handle, duration = duration_]() mutable {
            std::this_thread::sleep_for(duration);
            handle.resume();
        }).detach();
    }
    
    void await_resume() const noexcept {}
    
private:
    std::chrono::milliseconds duration_;
};

/**
 * @brief 协程化sleep
 */
inline auto sleep_for(std::chrono::milliseconds duration) {
    return SleepAwaiter(duration);
}

inline auto sleep_for(int milliseconds) {
    return SleepAwaiter(std::chrono::milliseconds(milliseconds));
}

inline auto sleep(std::chrono::milliseconds duration) {
    return SleepAwaiter(duration);
}

inline auto sleep(int milliseconds) {
    return SleepAwaiter(std::chrono::milliseconds(milliseconds));
}

/**
 * @brief 条件等待器
 */
class WaitUntilAwaiter {
public:
    explicit WaitUntilAwaiter(std::function<bool()> predicate)
        : predicate_(std::move(predicate)) {}
    
    bool await_ready() const noexcept {
        return predicate_();
    }
    
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        std::thread([this, handle]() mutable {
            while (!predicate_()) {
                std::this_thread::yield();
            }
            handle.resume();
        }).detach();
    }
    
    void await_resume() const noexcept {}
    
private:
    std::function<bool()> predicate_;
};

inline auto wait_until(std::function<bool()> predicate) {
    return WaitUntilAwaiter(std::move(predicate));
}

} // namespace coroutine
} // namespace rpc
