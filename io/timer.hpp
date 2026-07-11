/**
 * @file timer.hpp
 * @brief 超时机制 - io_uring timeout + 协程超时包装 + ASYNC_CANCEL
 *
 * 提供三层超时机制：
 * 1. IoUringTimeout: 直接提交 IORING_OP_TIMEOUT SQE
 * 2. LinkedTimeout: IOSQE_IO_LINK + IORING_OP_LINK_TIMEOUT，操作级超时
 * 3. with_timeout(): 协程级超时包装器，超时自动取消挂起的IO操作
 *
 * io_uring 超时 CQE result 含义：
 * - result == 0: 超时正常触发
 * - result == -ETIME: 超时到期
 * - result == -ECANCELED: 被取消（主操作先完成）
 */

#pragma once

#include "io_uring.hpp"
#include "../coroutine/coroutine.hpp"
#include <chrono>
#include <optional>
#include <variant>

namespace rpc {
namespace io {

// ==================== 工具函数 ====================

/**
 * @brief chrono时长转__kernel_timespec
 */
inline __kernel_timespec to_kernel_timespec(std::chrono::nanoseconds ns) {
    __kernel_timespec ts;
    ts.tv_sec = static_cast<__kernel_long_t>(ns.count() / 1000000000LL);
    ts.tv_nsec = static_cast<__kernel_long_t>(ns.count() % 1000000000LL);
    return ts;
}

inline __kernel_timespec to_kernel_timespec(std::chrono::milliseconds ms) {
    return to_kernel_timespec(std::chrono::duration_cast<std::chrono::nanoseconds>(ms));
}

inline __kernel_timespec to_kernel_timespec(std::chrono::seconds s) {
    return to_kernel_timespec(std::chrono::duration_cast<std::chrono::nanoseconds>(s));
}

// ==================== io_uring 级超时 ====================

/**
 * @brief io_uring 超时Awaiter
 * 直接提交 IORING_OP_TIMEOUT，等待指定时间后完成
 */
class TimeoutAwaiter {
public:
    explicit TimeoutAwaiter(std::chrono::nanoseconds duration)
        : ts_(to_kernel_timespec(duration)) {}

    explicit TimeoutAwaiter(std::chrono::milliseconds ms)
        : ts_(to_kernel_timespec(ms)) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        auto& ring = global_io_uring();
        ring.prepare_timeout(&ts_, 0,
            [this, handle](int result, uint32_t) mutable {
                result_ = result;
                handle.resume();
            });
        ring.submit();
    }

    /**
     * @brief 返回CQE result
     * -ETIME(-62): 超时正常到期
     * -ECANCELED(-125): 被取消
     */
    int await_resume() const { return result_; }

    bool timed_out() const { return result_ == -ETIME; }

private:
    __kernel_timespec ts_;
    int result_ = 0;
};

/**
 * @brief 协程化sleep（基于io_uring timeout，不占用线程）
 */
inline auto io_sleep_for(std::chrono::milliseconds ms) {
    return TimeoutAwaiter(ms);
}

inline auto io_sleep_for(std::chrono::seconds s) {
    return TimeoutAwaiter(std::chrono::duration_cast<std::chrono::nanoseconds>(s));
}

// ==================== Linked Timeout ====================

/**
 * @brief 带链接超时的异步recv
 * 使用 IOSQE_IO_LINK + IORING_OP_LINK_TIMEOUT
 * 如果recv在指定时间内未完成，自动触发超时CQE
 */
class TimedRecvAwaiter {
public:
    TimedRecvAwaiter(int fd, void* buffer, size_t size, int flags,
                     std::chrono::nanoseconds timeout)
        : fd_(fd), buffer_(buffer), size_(size), flags_(flags)
        , timeout_ts_(to_kernel_timespec(timeout)) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        auto& ring = global_io_uring();

        // 提交recv，设置IOSQE_IO_LINK
        auto* recv_sqe = ring.prepare_recv(fd_, buffer_, size_, flags_,
            [this, handle](int result, uint32_t) mutable {
                recv_result_ = result;
                recv_completed_.store(true, std::memory_order_release);
                if (!done_.exchange(true, std::memory_order_acq_rel)) {
                    handle.resume();
                }
            });
        recv_sqe->flags |= IOSQE_IO_LINK;

        // 紧跟提交link_timeout
        ring.prepare_link_timeout(&timeout_ts_,
            [this, handle](int result, uint32_t) mutable {
                timeout_result_ = result;
                timeout_fired_.store(true, std::memory_order_release);
                if (!done_.exchange(true, std::memory_order_acq_rel)) {
                    handle.resume();
                }
            });

        ring.submit();
    }

    /**
     * @brief 返回recv结果
     * 如果超时，抛出TimeoutException
     */
    ssize_t await_resume() {
        if (timeout_fired_.load(std::memory_order_acquire)) {
            throw TimeoutException("recv timed out");
        }
        return recv_result_;
    }

    bool was_timeout() const {
        return timeout_fired_.load(std::memory_order_acquire) && !recv_completed_.load();
    }

private:
    int fd_;
    void* buffer_;
    size_t size_;
    int flags_;
    __kernel_timespec timeout_ts_;
    ssize_t recv_result_ = 0;
    int timeout_result_ = 0;
    std::atomic<bool> recv_completed_{false};
    std::atomic<bool> timeout_fired_{false};
    std::atomic<bool> done_{false};
};

/**
 * @brief 带链接超时的异步send
 */
class TimedSendAwaiter {
public:
    TimedSendAwaiter(int fd, const void* buffer, size_t size, int flags,
                     std::chrono::nanoseconds timeout)
        : fd_(fd), buffer_(buffer), size_(size), flags_(flags)
        , timeout_ts_(to_kernel_timespec(timeout)) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        auto& ring = global_io_uring();

        auto* send_sqe = ring.prepare_send(fd_, buffer_, size_, flags_,
            [this](int result, uint32_t) mutable {
                send_result_ = result;
                send_completed_.store(true, std::memory_order_release);
            });
        send_sqe->flags |= IOSQE_IO_LINK;

        ring.prepare_link_timeout(&timeout_ts_,
            [this](int result, uint32_t) mutable {
                timeout_result_ = result;
                timeout_fired_.store(true, std::memory_order_release);
            });

        ring.prepare_nop([this, handle](int, uint32_t) mutable {
            handle.resume();
        });

        ring.submit();
    }

    ssize_t await_resume() {
        if (timeout_fired_.load(std::memory_order_acquire)) {
            throw TimeoutException("send timed out");
        }
        return send_result_;
    }

    bool was_timeout() const {
        return timeout_fired_.load(std::memory_order_acquire) && !send_completed_.load();
    }

private:
    int fd_;
    const void* buffer_;
    size_t size_;
    int flags_;
    __kernel_timespec timeout_ts_;
    ssize_t send_result_ = 0;
    int timeout_result_ = 0;
    std::atomic<bool> send_completed_{false};
    std::atomic<bool> timeout_fired_{false};
};

    std::atomic<bool> done_{false};
// ==================== 协程级超时包装 ====================

/**
 * @brief 协程超时包装器
 * 在io_uring上同时提交操作和timeout，先完成者胜出
 * 超时时使用IORING_OP_ASYNC_CANCEL取消挂起的操作
 *
 * 用法：
 *   auto result = co_await with_timeout(async_recv(fd, buf, sz, 0), 5s);
 *
 * 实现原理：
 *   1. 提交主操作SQE
 *   2. 提交IORING_OP_TIMEOUT SQE
 *   3. 两者竞争：
 *      - 主操作先完成 → cancel timeout SQE
 *      - timeout先完成 → cancel 主操作SQE, throw TimeoutException
 */
template<typename T>
class WithTimeoutAwaiter {
public:
    WithTimeoutAwaiter(std::function<void(std::function<void(T)>, std::function<void(std::exception_ptr)>)> submit_fn,
                       std::chrono::nanoseconds timeout)
        : submit_fn_(std::move(submit_fn))
        , timeout_ts_(to_kernel_timespec(timeout)) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        // 提交主操作
        submit_fn_(
            [this, handle](T result) mutable {
                if (!done_.exchange(true, std::memory_order_acq_rel)) {
                    result_ = std::move(result);
                    // 取消timeout
                    cancel_timeout();
                    handle.resume();
                }
            },
            [this, handle](std::exception_ptr e) mutable {
                if (!done_.exchange(true, std::memory_order_acq_rel)) {
                    exception_ = e;
                    cancel_timeout();
                    handle.resume();
                }
            }
        );

        // 提交timeout
        auto& ring = global_io_uring();
        timeout_user_data_ = ring.peek_next_callback_id();
        ring.prepare_timeout(&timeout_ts_, 0,
            [this, handle](int result, uint32_t) mutable {
                if (!done_.exchange(true, std::memory_order_acq_rel)) {
                    if (result == -ETIME || result == 0) {
                        timeout_fired_ = true;
                        exception_ = std::make_exception_ptr(TimeoutException());
                    }
                    // 取消主操作
                    cancel_operation();
                    handle.resume();
                }
            });
        ring.submit();
    }

    T await_resume() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
        return std::move(result_.value());
    }

    bool was_timeout() const { return timeout_fired_; }

private:
    std::function<void(std::function<void(T)>, std::function<void(std::exception_ptr)>)> submit_fn_;
    __kernel_timespec timeout_ts_;
    uint64_t timeout_user_data_ = 0;
    uint64_t operation_user_data_ = 0;
    std::optional<T> result_;
    std::exception_ptr exception_;
    std::atomic<bool> done_{false};
    bool timeout_fired_ = false;

    void cancel_timeout() {
        if (timeout_user_data_ != 0) {
            auto& ring = global_io_uring();
            try {
                ring.prepare_async_cancel(timeout_user_data_, [](int, uint32_t) {});
                ring.submit();
            } catch (...) {}
        }
    }

    void cancel_operation() {
        if (operation_user_data_ != 0) {
            auto& ring = global_io_uring();
            try {
                ring.prepare_async_cancel(operation_user_data_, [](int, uint32_t) {});
                ring.submit();
            } catch (...) {}
        }
    }
};

/**
 * @brief 带超时的recv便捷函数
 */
inline auto timed_recv(int fd, void* buffer, size_t size, int flags,
                       std::chrono::nanoseconds timeout) {
    return TimedRecvAwaiter(fd, buffer, size, flags, timeout);
}

inline auto timed_recv(int fd, void* buffer, size_t size, int flags,
                       std::chrono::milliseconds timeout) {
    return TimedRecvAwaiter(fd, buffer, size, flags,
                            std::chrono::duration_cast<std::chrono::nanoseconds>(timeout));
}

/**
 * @brief 带超时的send便捷函数
 */
inline auto timed_send(int fd, const void* buffer, size_t size, int flags,
                       std::chrono::nanoseconds timeout) {
    return TimedSendAwaiter(fd, buffer, size, flags, timeout);
}

inline auto timed_send(int fd, const void* buffer, size_t size, int flags,
                       std::chrono::milliseconds timeout) {
    return TimedSendAwaiter(fd, buffer, size, flags,
                            std::chrono::duration_cast<std::chrono::nanoseconds>(timeout));
}

// ==================== Hashed Timing Wheel ====================

/**
 * @brief Hashed Timing Wheel - O(1)插入/过期的高性能定时器
 * 用于大量短超时场景（如RPC调用超时）
 *
 * 设计：
 * - 1ms精度，64个slot，每slot一个链表
 * - 溢出链表处理远期超时
 * - 每次tick推进当前slot，执行到期回调
 */
class TimingWheel {
public:
    using TimerId = uint64_t;
    using TimerCallback = std::function<void()>;

    explicit TimingWheel(size_t slots = 64, size_t tick_ms = 1)
        : slots_(slots)
        , tick_ms_(tick_ms)
        , current_tick_(0)
        , next_id_(1)
        , wheels_(slots)
    {}

    /**
     * @brief 添加定时器
     * @param delay_ms 延迟毫秒数
     * @param callback 到期回调
     * @return 定时器ID
     */
    TimerId add_timer(size_t delay_ms, TimerCallback callback) {
        auto id = next_id_.fetch_add(1);
        size_t ticks = (delay_ms + tick_ms_ - 1) / tick_ms_;  // 向上取整
        size_t expire_tick = current_tick_ + ticks - 1;
        size_t slot = expire_tick % slots_;

        std::lock_guard<std::mutex> lock(mutex_);
        wheels_[slot].push_back({id, expire_tick, std::move(callback)});
        timer_slot_[id] = slot;
        return id;
    }

    /**
     * @brief 取消定时器
     */
    bool cancel_timer(TimerId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = timer_slot_.find(id);
        if (it == timer_slot_.end()) return false;

        auto& slot_list = wheels_[it->second];
        slot_list.erase(
            std::remove_if(slot_list.begin(), slot_list.end(),
                [id](const TimerEntry& e) { return e.id == id; }),
            slot_list.end());
        timer_slot_.erase(it);
        return true;
    }

    /**
     * @brief 推进一个tick，执行到期回调
     * @return 执行的回调数量
     */
    size_t tick() {
        std::vector<TimerCallback> expired;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& slot = wheels_[current_tick_ % slots_];
            for (auto& entry : slot) {
                if (entry.expire_tick <= current_tick_) {
                    expired.push_back(std::move(entry.callback));
                    timer_slot_.erase(entry.id);
                }
            }
            slot.erase(
                std::remove_if(slot.begin(), slot.end(),
                    [this](const TimerEntry& e) { return e.expire_tick <= current_tick_; }),
                slot.end());
            ++current_tick_;
        }

        for (auto& cb : expired) {
            cb();
        }
        return expired.size();
    }

    /**
     * @brief 推进多个tick
     */
    size_t advance(size_t ticks) {
        size_t total = 0;
        for (size_t i = 0; i < ticks; ++i) {
            total += tick();
        }
        return total;
    }

private:
    struct TimerEntry {
        TimerId id;
        size_t expire_tick;
        TimerCallback callback;
    };

    size_t slots_;
    size_t tick_ms_;
    size_t current_tick_;
    std::atomic<uint64_t> next_id_;
    std::vector<std::vector<TimerEntry>> wheels_;
    std::unordered_map<TimerId, size_t> timer_slot_;
    std::mutex mutex_;
};

} // namespace io
} // namespace rpc
