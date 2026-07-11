/**
 * @file sync.hpp
 * @brief 协程同步原语 - 协程友好的锁与信号量
 * @author RPC Framework
 * @version 1.0
 */

#pragma once

#include <coroutine>
#include <atomic>
#include <queue>
#include <mutex>

namespace rpc {
namespace coroutine {

/**
 * @brief 协程互斥锁
 * 非阻塞式互斥锁，支持协程等待
 */
class AsyncMutex {
public:
    AsyncMutex() : locked_(false) {}
    
    class LockAwaiter {
    public:
        explicit LockAwaiter(AsyncMutex& mutex) : mutex_(mutex) {}
        
        bool await_ready() const noexcept {
            return !mutex_.locked_.load(std::memory_order_acquire);
        }
        
        void await_suspend(std::coroutine_handle<> handle) noexcept {
            std::lock_guard<std::mutex> lock(mutex_.waiters_mutex_);
            if (!mutex_.locked_.exchange(true, std::memory_order_acq_rel)) {
                handle.resume();
            } else {
                mutex_.waiters_.push(handle);
            }
        }
        
        void await_resume() const noexcept {}
        
    private:
        AsyncMutex& mutex_;
    };
    
    auto lock() { return LockAwaiter(*this); }
    
    void unlock() {
        std::lock_guard<std::mutex> lock(waiters_mutex_);
        if (waiters_.empty()) {
            locked_.store(false, std::memory_order_release);
        } else {
            auto next = waiters_.front();
            waiters_.pop();
            next.resume();
        }
    }
    
private:
    std::atomic<bool> locked_;
    std::queue<std::coroutine_handle<>> waiters_;
    std::mutex waiters_mutex_;
};

/**
 * @brief 协程信号量
 */
class AsyncSemaphore {
public:
    explicit AsyncSemaphore(size_t count = 0) : count_(count) {}
    
    class AcquireAwaiter {
    public:
        explicit AcquireAwaiter(AsyncSemaphore& sem) : sem_(sem) {}
        
        bool await_ready() const noexcept {
            return sem_.count_.load(std::memory_order_acquire) > 0;
        }
        
        void await_suspend(std::coroutine_handle<> handle) noexcept {
            std::lock_guard<std::mutex> lock(sem_.waiters_mutex_);
            if (sem_.count_.fetch_sub(1, std::memory_order_acq_rel) > 0) {
                handle.resume();
            } else {
                sem_.waiters_.push(handle);
            }
        }
        
        void await_resume() const noexcept {}
        
    private:
        AsyncSemaphore& sem_;
    };
    
    auto acquire() { return AcquireAwaiter(*this); }
    
    void release() {
        std::lock_guard<std::mutex> lock(waiters_mutex_);
        if (waiters_.empty()) {
            count_.fetch_add(1, std::memory_order_release);
        } else {
            auto next = waiters_.front();
            waiters_.pop();
            next.resume();
        }
    }
    
private:
    std::atomic<size_t> count_;
    std::queue<std::coroutine_handle<>> waiters_;
    std::mutex waiters_mutex_;
};

/**
 * @brief 协程条件变量
 */
class AsyncConditionVariable {
public:
    class WaitAwaiter {
    public:
        WaitAwaiter(AsyncConditionVariable& cv, AsyncMutex& mutex)
            : cv_(cv), mutex_(mutex) {}
        
        bool await_ready() const noexcept { return false; }
        
        void await_suspend(std::coroutine_handle<> handle) noexcept {
            {
                std::lock_guard<std::mutex> lock(cv_.waiters_mutex_);
                cv_.waiters_.push(handle);
            }
            mutex_.unlock();
        }
        
        void await_resume() noexcept {}
        
    private:
        AsyncConditionVariable& cv_;
        AsyncMutex& mutex_;
    };
    
    auto wait(AsyncMutex& mutex) { return WaitAwaiter(*this, mutex); }
    
    void notify_one() {
        std::lock_guard<std::mutex> lock(waiters_mutex_);
        if (!waiters_.empty()) {
            auto next = waiters_.front();
            waiters_.pop();
            next.resume();
        }
    }
    
    void notify_all() {
        std::lock_guard<std::mutex> lock(waiters_mutex_);
        while (!waiters_.empty()) {
            auto next = waiters_.front();
            waiters_.pop();
            next.resume();
        }
    }
    
private:
    std::queue<std::coroutine_handle<>> waiters_;
    std::mutex waiters_mutex_;
};

/**
 * @brief 协程RAII锁守卫
 */
class AsyncLockGuard {
public:
    explicit AsyncLockGuard(AsyncMutex& mutex) : mutex_(mutex) {}
    ~AsyncLockGuard() { mutex_.unlock(); }
    
    AsyncLockGuard(const AsyncLockGuard&) = delete;
    AsyncLockGuard& operator=(const AsyncLockGuard&) = delete;
    
private:
    AsyncMutex& mutex_;
};

} // namespace coroutine
} // namespace rpc
