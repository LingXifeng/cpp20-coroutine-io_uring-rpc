/**
 * @file task.hpp
 * @brief C++20协程任务封装 - 无栈协程核心实现
 */

#pragma once

#include <coroutine>
#include <exception>
#include <utility>
#include <memory>
#include <functional>
#include <type_traits>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace rpc {
namespace coroutine {

// 前向声明
template<typename T = void>
class Task;

/**
 * @brief 协程状态枚举
 */
enum class CoroutineState : uint8_t {
    Created,
    Running,
    Suspended,
    Completed,
    Cancelled
};

/**
 * @brief Task的Promise类型（非void）
 * get_return_object() 延迟到Task定义之后实现
 */
template<typename T>
struct TaskPromise {
    using value_type = T;
    using coroutine_handle = std::coroutine_handle<TaskPromise>;
    
    std::shared_ptr<T> result;
    std::exception_ptr exception;
    std::atomic<CoroutineState> state{CoroutineState::Created};
    std::coroutine_handle<> continuation{nullptr};
    std::atomic<int> ref_count{1};
    
    // 仅声明，定义在Task之后
    Task<T> get_return_object() noexcept;
    
    std::suspend_always initial_suspend() noexcept { return {}; }
    
    auto final_suspend() noexcept {
        struct FinalAwaiter {
            bool await_ready() noexcept { return false; }
            void await_suspend(coroutine_handle h) noexcept {
                auto& promise = h.promise();
                promise.state.store(CoroutineState::Completed, std::memory_order_release);
                if (promise.continuation) {
                    promise.continuation.resume();
                }
            }
            void await_resume() noexcept {}
        };
        return FinalAwaiter{};
    }
    
    void return_value(T value) noexcept {
        result = std::make_shared<T>(std::move(value));
    }
    
    void unhandled_exception() noexcept {
        exception = std::current_exception();
    }
};

/**
 * @brief void特化的Promise类型
 */
template<>
struct TaskPromise<void> {
    using coroutine_handle = std::coroutine_handle<TaskPromise>;
    
    std::exception_ptr exception;
    std::atomic<CoroutineState> state{CoroutineState::Created};
    std::coroutine_handle<> continuation{nullptr};
    std::atomic<int> ref_count{1};
    
    // 仅声明，定义在Task<void>之后
    Task<void> get_return_object() noexcept;
    
    std::suspend_always initial_suspend() noexcept { return {}; }
    
    auto final_suspend() noexcept {
        struct FinalAwaiter {
            bool await_ready() noexcept { return false; }
            void await_suspend(coroutine_handle h) noexcept {
                auto& promise = h.promise();
                promise.state.store(CoroutineState::Completed, std::memory_order_release);
                if (promise.continuation) {
                    promise.continuation.resume();
                }
            }
            void await_resume() noexcept {}
        };
        return FinalAwaiter{};
    }
    
    void return_void() noexcept {}
    void unhandled_exception() noexcept { exception = std::current_exception(); }
};

/**
 * @brief Task类型 - 协程的返回对象（非void）
 */
template<typename T>
class Task {
public:
    using promise_type = TaskPromise<T>;
    using coroutine_handle = std::coroutine_handle<promise_type>;
    
    Task() noexcept : handle_(nullptr) {}
    explicit Task(coroutine_handle h) noexcept : handle_(h) {}
    
    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    ~Task() { destroy(); }
    
    bool valid() const noexcept { return handle_ != nullptr; }
    
    CoroutineState state() const noexcept {
        if (!handle_) return CoroutineState::Cancelled;
        return handle_.promise().state.load(std::memory_order_acquire);
    }
    
    Task& start() & {
        if (handle_ && state() == CoroutineState::Created) {
            handle_.promise().state.store(CoroutineState::Running, std::memory_order_release);
            handle_.resume();
        }
        return *this;
    }
    
    Task&& start() && {
        start();
        return std::move(*this);
    }
    
    auto operator co_await() noexcept {
        struct Awaiter {
            coroutine_handle handle;
            
            bool await_ready() noexcept {
                auto s = handle.promise().state.load(std::memory_order_acquire);
                return s == CoroutineState::Completed || s == CoroutineState::Cancelled;
            }
            
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont) noexcept {
                handle.promise().continuation = cont;
                if (handle.promise().state.load() == CoroutineState::Created) {
                    handle.promise().state.store(CoroutineState::Running, std::memory_order_release);
                    return handle;
                }
                return cont;
            }
            
            T await_resume() {
                auto& promise = handle.promise();
                if (promise.exception) {
                    std::rethrow_exception(promise.exception);
                }
                return *promise.result;
            }
        };
        return Awaiter{handle_};
    }
    
    T get() {
        if (!handle_) throw std::runtime_error("Task is null");
        if (state() == CoroutineState::Created) {
            handle_.promise().state.store(CoroutineState::Running, std::memory_order_release);
            handle_.resume();
        }
        while (state() != CoroutineState::Completed && 
               state() != CoroutineState::Cancelled) {
            std::this_thread::yield();
        }
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return *handle_.promise().result;
    }
    
    void cancel() noexcept {
        if (handle_ && state() != CoroutineState::Completed) {
            handle_.promise().state.store(CoroutineState::Cancelled, std::memory_order_release);
        }
    }
    
private:
    coroutine_handle handle_;
    void destroy() noexcept {
        if (handle_) { handle_.destroy(); handle_ = nullptr; }
    }
};

/**
 * @brief void特化的Task
 */
template<>
class Task<void> {
public:
    using promise_type = TaskPromise<void>;
    using coroutine_handle = std::coroutine_handle<promise_type>;
    
    Task() noexcept : handle_(nullptr) {}
    explicit Task(coroutine_handle h) noexcept : handle_(h) {}
    
    Task(Task&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    Task& operator=(Task&& other) noexcept {
        if (this != &other) { destroy(); handle_ = other.handle_; other.handle_ = nullptr; }
        return *this;
    }
    
    ~Task() { destroy(); }
    
    bool valid() const noexcept { return handle_ != nullptr; }
    
    CoroutineState state() const noexcept {
        if (!handle_) return CoroutineState::Cancelled;
        return handle_.promise().state.load(std::memory_order_acquire);
    }
    
    Task& start() & {
        if (handle_ && state() == CoroutineState::Created) {
            handle_.promise().state.store(CoroutineState::Running, std::memory_order_release);
            handle_.resume();
        }
        return *this;
    }
    
    Task&& start() && { start(); return std::move(*this); }
    
    auto operator co_await() noexcept {
        struct Awaiter {
            coroutine_handle handle;
            
            bool await_ready() noexcept {
                auto s = handle.promise().state.load(std::memory_order_acquire);
                return s == CoroutineState::Completed || s == CoroutineState::Cancelled;
            }
            
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont) noexcept {
                handle.promise().continuation = cont;
                if (handle.promise().state.load() == CoroutineState::Created) {
                    handle.promise().state.store(CoroutineState::Running, std::memory_order_release);
                    return handle;
                }
                return cont;
            }
            
            void await_resume() {
                if (handle.promise().exception) {
                    std::rethrow_exception(handle.promise().exception);
                }
            }
        };
        return Awaiter{handle_};
    }
    
    void get() {
        if (!handle_) throw std::runtime_error("Task is null");
        if (state() == CoroutineState::Created) {
            handle_.promise().state.store(CoroutineState::Running, std::memory_order_release);
            handle_.resume();
        }
        while (state() != CoroutineState::Completed && 
               state() != CoroutineState::Cancelled) {
            std::this_thread::yield();
        }
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
    }
    
    void cancel() noexcept {
        if (handle_ && state() != CoroutineState::Completed) {
            handle_.promise().state.store(CoroutineState::Cancelled, std::memory_order_release);
        }
    }
    
private:
    coroutine_handle handle_;
    void destroy() noexcept {
        if (handle_) { handle_.destroy(); handle_ = nullptr; }
    }
};

// 延迟实现 get_return_object
template<typename T>
Task<T> TaskPromise<T>::get_return_object() noexcept {
    return Task<T>{coroutine_handle::from_promise(*this)};
}

inline
Task<void> TaskPromise<void>::get_return_object() noexcept {
    return Task<void>{coroutine_handle::from_promise(*this)};
}

// 辅助函数
template<typename T>
Task<T> make_ready_task(T value) {
    co_return value;
}

inline
Task<void> make_ready_task() {
    co_return;
}

} // namespace coroutine
} // namespace rpc
