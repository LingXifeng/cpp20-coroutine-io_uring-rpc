/**
 * @file event_loop.hpp
 * @brief 事件循环 - 协程与io_uring的集成
 */

#pragma once

#include "io_uring.hpp"
#include "../coroutine/coroutine.hpp"
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <memory>

namespace rpc {
namespace io {

struct TimerHandle {
    uint64_t id;
    std::chrono::steady_clock::time_point expire_time;
    std::function<void()> callback;
    bool cancelled = false;
    bool operator>(const TimerHandle& other) const {
        return expire_time > other.expire_time;
    }
};

struct EventLoopConfig {
    uint32_t io_uring_depth = 4096;
    bool sq_poll = true;
    uint32_t sq_thread_cpu = 0;
    uint32_t sq_thread_idle = 2000;
    size_t worker_threads = 4;
};

class EventLoop {
public:
    static EventLoop& instance() {
        static EventLoop loop;
        return loop;
    }
    
    EventLoop()
        : running_(false)
        , next_timer_id_(0)
    {}
    
    ~EventLoop() { stop(); }
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;
    
    // 使用全局 io_uring 单例，确保与 async_* awaiters 一致
    IoUring& io_uring() { return global_io_uring(); }
    const IoUring& io_uring() const { return global_io_uring(); }
    
    void stop() { running_.store(false); }
    
    void post(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            pending_tasks_.push(std::move(task));
        }
        wakeup();
    }
    
    template<typename T>
    void spawn(::rpc::coroutine::Task<T> task) {
        auto shared_task = std::make_shared<::rpc::coroutine::Task<T>>(std::move(task));
        post([shared_task]() mutable {
            shared_task->start();
        });
    }
    
    uint64_t set_timer(uint64_t delay_ms, std::function<void()> callback) {
        auto id = next_timer_id_.fetch_add(1);
        auto expire = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
        {
            std::lock_guard<std::mutex> lock(timers_mutex_);
            timers_.push({id, expire, std::move(callback)});
        }
        return id;
    }
    
    bool cancel_timer(uint64_t timer_id) {
        std::lock_guard<std::mutex> lock(timers_mutex_);
        auto it = timer_map_.find(timer_id);
        if (it != timer_map_.end()) {
            it->second->cancelled = true;
            timer_map_.erase(it);
            return true;
        }
        return false;
    }
    
    void run_once(int timeout_ms = 0) {
        process_timers();
        process_tasks();
        global_io_uring().run_once(timeout_ms);
    }
    
    void run() {
        running_.store(true);
        run_loop();
    }
    
    bool is_running() const { return running_.load(); }
    
private:
    std::atomic<bool> running_;
    std::thread loop_thread_;
    std::queue<std::function<void()>> pending_tasks_;
    std::mutex tasks_mutex_;
    std::priority_queue<TimerHandle, std::vector<TimerHandle>, std::greater<>> timers_;
    std::unordered_map<uint64_t, TimerHandle*> timer_map_;
    std::mutex timers_mutex_;
    std::atomic<uint64_t> next_timer_id_;
    
    void run_loop() {
        while (running_.load()) { run_once(10); }
    }
    
    void process_timers() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(timers_mutex_);
        while (!timers_.empty() && timers_.top().expire_time <= now) {
            auto timer = timers_.top();
            timers_.pop();
            if (!timer.cancelled) { timer.callback(); }
            timer_map_.erase(timer.id);
        }
    }
    
    void process_tasks() {
        std::queue<std::function<void()>> tasks;
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            tasks = std::move(pending_tasks_);
        }
        while (!tasks.empty()) {
            auto& task = tasks.front();
            task();
            tasks.pop();
        }
    }
    
    void wakeup() {
        global_io_uring().prepare_nop([](int, uint32_t) {});
        global_io_uring().submit();
    }
};

inline EventLoop& global_event_loop() {
    return EventLoop::instance();
}

} // namespace io
} // namespace rpc
