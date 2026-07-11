/**
 * @file scheduler.hpp
 * @brief 协程调度器 - 多线程协程调度与负载均衡
 * @author RPC Framework
 * @version 1.0
 * 
 * 调度器负责：
 * - 管理协程的执行队列
 * - 多线程调度，工作窃取算法
 * - 线程本地存储优化
 * - 公平调度与优先级调度
 */

#pragma once

#include <coroutine>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <memory>
#include <deque>
#include <array>

namespace rpc {
namespace coroutine {

/**
 * @brief 工作窃取队列
 * 线程安全的双端队列，支持从一端推入、另一端窃取
 */
class WorkStealingQueue {
public:
    using Task = std::coroutine_handle<>;
    
    WorkStealingQueue() = default;
    
    void push(Task task) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(task);
    }
    
    bool pop(Task& task) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        task = queue_.back();
        queue_.pop_back();
        return true;
    }
    
    bool steal(Task& task) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        task = queue_.front();
        queue_.pop_front();
        return true;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
private:
    mutable std::mutex mutex_;
    std::deque<Task> queue_;
};

/**
 * @brief 协程调度器
 * 支持多线程调度、工作窃取、线程本地存储优化
 */
class Scheduler {
public:
    static constexpr size_t kDefaultThreadCount = 4;
    
    explicit Scheduler(size_t thread_count = kDefaultThreadCount)
        : thread_count_(thread_count > 0 ? thread_count : kDefaultThreadCount)
        , running_(false)
        , local_queues_(thread_count_)
    {
        start();
    }
    
    ~Scheduler() {
        stop();
    }
    
    // 禁止拷贝
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    
    /**
     * @brief 获取单例实例
     */
    static Scheduler& instance() {
        static Scheduler scheduler(std::thread::hardware_concurrency());
        return scheduler;
    }
    
    /**
     * @brief 调度协程执行
     * @param handle 协程句柄
     * @param thread_id 目标线程ID（-1表示自动选择）
     */
    void schedule(std::coroutine_handle<> handle, int thread_id = -1) {
        if (thread_id >= 0 && static_cast<size_t>(thread_id) < thread_count_) {
            local_queues_[thread_id].push(handle);
        } else {
            // 负载均衡：选择队列最短的线程
            size_t min_idx = 0;
            size_t min_size = local_queues_[0].size();
            for (size_t i = 1; i < thread_count_; ++i) {
                size_t size = local_queues_[i].size();
                if (size < min_size) {
                    min_size = size;
                    min_idx = i;
                }
            }
            local_queues_[min_idx].push(handle);
        }
        cv_.notify_one();
    }
    
    /**
     * @brief 批量调度协程
     */
    void schedule_batch(std::vector<std::coroutine_handle<>> handles) {
        for (auto handle : handles) {
            schedule(handle);
        }
    }
    
    /**
     * @brief 获取当前线程ID
     */
    int current_thread_id() const {
        return current_thread_id_;
    }
    
    /**
     * @brief 获取线程数量
     */
    size_t thread_count() const {
        return thread_count_;
    }
    
    /**
     * @brief 获取待执行任务数量
     */
    size_t pending_count() const {
        size_t total = 0;
        for (const auto& queue : local_queues_) {
            total += queue.size();
        }
        return total;
    }
    
private:
    size_t thread_count_;
    std::atomic<bool> running_;
    std::vector<std::thread> threads_;
    std::vector<WorkStealingQueue> local_queues_;
    std::condition_variable cv_;
    std::mutex cv_mutex_;
    
    // 线程本地存储
    static thread_local int current_thread_id_;
    
    void start() {
        running_.store(true, std::memory_order_release);
        threads_.reserve(thread_count_);
        
        for (size_t i = 0; i < thread_count_; ++i) {
            threads_.emplace_back([this, i] {
                current_thread_id_ = static_cast<int>(i);
                worker_loop(i);
            });
        }
    }
    
    void stop() {
        running_.store(false, std::memory_order_release);
        cv_.notify_all();
        
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    
    void worker_loop(size_t thread_id) {
        while (running_.load(std::memory_order_acquire)) {
            std::coroutine_handle<> task;
            
            // 先从本地队列获取
            if (local_queues_[thread_id].pop(task)) {
                task.resume();
                continue;
            }
            
            // 工作窃取：从其他队列窃取
            bool stolen = false;
            for (size_t i = 0; i < thread_count_; ++i) {
                if (i != thread_id && local_queues_[i].steal(task)) {
                    task.resume();
                    stolen = true;
                    break;
                }
            }
            
            if (stolen) {
                continue;
            }
            
            // 没有任务，等待
            std::unique_lock<std::mutex> lock(cv_mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(10), [this] {
                return !running_.load(std::memory_order_acquire) || pending_count() > 0;
            });
        }
    }
};

// 静态成员初始化
thread_local int Scheduler::current_thread_id_ = -1;

/**
 * @brief 协程Awaiter：在调度器上切换执行
 */
class ScheduleOn {
public:
    explicit ScheduleOn(Scheduler& scheduler, int thread_id = -1)
        : scheduler_(scheduler), thread_id_(thread_id) {}
    
    bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        scheduler_.schedule(handle, thread_id_);
    }
    
    void await_resume() const noexcept {}
    
private:
    Scheduler& scheduler_;
    int thread_id_;
};

/**
 * @brief 切换到指定线程执行
 */
inline auto schedule_on(int thread_id = -1) {
    return ScheduleOn(Scheduler::instance(), thread_id);
}

/**
 * @brief 让出CPU，允许其他协程执行
 */
inline auto yield() {
    struct YieldAwaiter {
        bool await_ready() const noexcept { return false; }
        
        void await_suspend(std::coroutine_handle<> handle) noexcept {
            Scheduler::instance().schedule(handle);
        }
        
        void await_resume() const noexcept {}
    };
    return YieldAwaiter{};
}

} // namespace coroutine
} // namespace rpc
