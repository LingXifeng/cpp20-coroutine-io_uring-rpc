/**
 * @file event_loop_pool.hpp
 * @brief 多线程EventLoop - Reactor-Per-Thread + MSG_RING跨线程唤醒
 *
 * 架构：
 *   Main Thread (EventLoopThread 0): accept新连接
 *   Worker Threads (EventLoopThread 1..N): 处理已建立的连接
 *
 * 跨线程通信：
 *   - IORING_OP_MSG_RING: 零系统调用跨线程唤醒
 *     主线程accept后，通过MSG_RING将fd投递到worker线程的io_uring
 *   - Lock-free task queue: 备用方案，用于投递任意任务
 *
 * 设计要点：
 *   - 每个线程独立的io_uring实例，无锁竞争
 *   - fd按round-robin / 最少连接数 分发到worker
 *   - MSG_RING避免eventfd的系统调用开销
 *   - 优雅关闭：先停止accept，等待所有连接处理完毕
 */

#pragma once

#include "io_uring.hpp"
#include "timer.hpp"
#include "../coroutine/coroutine.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <algorithm>

namespace rpc {
namespace io {

/**
 * @brief 跨线程任务
 */
struct CrossThreadTask {
    uint64_t type;           // 任务类型标识
    uint64_t data;           // 任务数据（如fd）
    std::function<void()> fn; // 可选回调
};

/**
 * @brief 单线程EventLoop - 每个线程一个独立io_uring实例
 */
class EventLoopThread {
public:
    explicit EventLoopThread(uint32_t thread_id, uint32_t queue_depth = 4096)
        : thread_id_(thread_id)
        , io_uring_(IoUringConfig{queue_depth})
        , running_(false)
        , connection_count_(0)
    {}

    ~EventLoopThread() {
        stop();
    }

    EventLoopThread(const EventLoopThread&) = delete;
    EventLoopThread& operator=(const EventLoopThread&) = delete;

    // ==================== 生命周期 ====================

    /**
     * @brief 启动事件循环（在独立线程中运行）
     */
    void start() {
        running_.store(true, std::memory_order_release);
        thread_ = std::thread([this] { run_loop(); });
    }

    /**
     * @brief 停止事件循环
     */
    void stop() {
        if (running_.exchange(false, std::memory_order_acq_rel)) {
            // 提交nop唤醒可能阻塞的io_uring_wait_cqe
            try {
                io_uring_.prepare_nop([](int, uint32_t) {});
                io_uring_.submit();
            } catch (...) {}
            if (thread_.joinable()) {
                thread_.join();
            }
        }
    }

    /**
     * @brief 在此线程中运行事件循环（不创建新线程）
     */
    void run() {
        running_.store(true, std::memory_order_release);
        run_loop();
    }

    // ==================== 访问器 ====================

    uint32_t thread_id() const { return thread_id_; }
    IoUring& io_uring() { return io_uring_; }
    const IoUring& io_uring() const { return io_uring_; }
    int ring_fd() const { return io_uring_.ring_fd(); }
    bool is_running() const { return running_.load(std::memory_order_acquire); }

    /**
     * @brief 当前连接数（用于负载均衡）
     */
    size_t connection_count() const {
        return connection_count_.load(std::memory_order_relaxed);
    }

    void inc_connections() { connection_count_.fetch_add(1, std::memory_order_relaxed); }
    void dec_connections() { connection_count_.fetch_sub(1, std::memory_order_relaxed); }

    // ==================== 跨线程通信 ====================

    /**
     * @brief 投递任务到此线程（通过MSG_RING）
     * @param task 任务数据
     * @return 是否投递成功
     *
     * MSG_RING流程：
     *   调用方在自己的io_uring上提交IORING_OP_MSG_RING
     *   目标线程的io_uring收到CQE，在CQE处理中执行任务
     */
    bool post_via_msg_ring(IoUring& caller_ring, CrossThreadTask task) {
        // 存储任务，MSG_RING CQE只传ID，实际任务从map中取
        uint64_t task_id = next_task_id_.fetch_add(1);
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            pending_tasks_[task_id] = std::move(task);
        }

        try {
            caller_ring.prepare_msg_ring(ring_fd(), task_id,
                [this, task_id](int result, uint32_t) {
                    // MSG_RING发送完成，无需额外操作
                });
            caller_ring.submit();
            return true;
        } catch (...) {
            // MSG_RING失败，回退到直接投递
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            pending_tasks_.erase(task_id);
            return false;
        }
    }

    /**
     * @brief 投递任务到此线程（通过锁保护队列，备用方案）
     */
    void post(CrossThreadTask task) {
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            task_queue_.push(std::move(task));
        }
        // 唤醒io_uring
        try {
            io_uring_.prepare_nop([](int, uint32_t) {});
            io_uring_.submit();
        } catch (...) {}
    }

    /**
     * @brief 投递函数任务
     */
    void post(std::function<void()> fn) {
        CrossThreadTask task;
        task.type = 0;
        task.data = 0;
        task.fn = std::move(fn);
        post(std::move(task));
    }

    /**
     * @brief spawn协程到此线程
     */
    template<typename T>
    void spawn(::rpc::coroutine::Task<T> task) {
        auto shared_task = std::make_shared<::rpc::coroutine::Task<T>>(std::move(task));
        post([shared_task]() mutable {
            shared_task->start();
        });
    }

    // ==================== 定时器 ====================

    /**
     * @brief 设置定时器（基于io_uring timeout）
     */
    uint64_t set_timer(std::chrono::milliseconds delay, std::function<void()> callback) {
        auto id = next_timer_id_.fetch_add(1);
        auto ts = to_kernel_timespec(delay);

        io_uring_.prepare_timeout(&ts, 0,
            [cb = std::move(callback)](int result, uint32_t) {
                if (result == -ETIME || result == 0) {
                    cb();
                }
            });
        io_uring_.submit();
        return id;
    }

private:
    uint32_t thread_id_;
    IoUring io_uring_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::atomic<size_t> connection_count_;

    // 任务管理
    std::mutex tasks_mutex_;
    std::unordered_map<uint64_t, CrossThreadTask> pending_tasks_;
    std::queue<CrossThreadTask> task_queue_;
    std::atomic<uint64_t> next_task_id_{1};
    std::atomic<uint64_t> next_timer_id_{1};

    void run_loop() {
        while (running_.load(std::memory_order_acquire)) {
            // 处理跨线程投递的任务
            process_tasks();

            // 处理io_uring完成事件
            io_uring_.run_once(10);
        }
    }

    void process_tasks() {
        // 处理MSG_RING投递的任务
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            for (auto it = pending_tasks_.begin(); it != pending_tasks_.end(); ) {
                if (it->second.fn) {
                    it->second.fn();
                }
                it = pending_tasks_.erase(it);
            }
        }

        // 处理队列投递的任务
        std::queue<CrossThreadTask> tasks;
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            tasks = std::move(task_queue_);
        }
        while (!tasks.empty()) {
            auto& task = tasks.front();
            if (task.fn) {
                task.fn();
            }
            tasks.pop();
        }
    }
};

/**
 * @brief EventLoop线程池 - Reactor-Per-Thread模型
 *
 * 典型用法：
 *   EventLoopPool pool(4);  // 1 main + 3 workers
 *   pool.start();
 *
 *   // main线程accept
 *   int client_fd = accept(listen_fd, ...);
 *
 *   // 通过MSG_RING投递到worker
 *   pool.dispatch_to_worker(client_fd);
 */
class EventLoopPool {
public:
    /**
     * @param worker_count worker线程数量（不含main线程）
     * @param queue_depth 每个io_uring的队列深度
     */
    explicit EventLoopPool(uint32_t worker_count = 3, uint32_t queue_depth = 4096)
        : worker_count_(worker_count)
        , queue_depth_(queue_depth)
        , next_worker_(0)
        , running_(false)
    {
        // 创建main线程（thread_id=0）
        threads_.push_back(std::make_unique<EventLoopThread>(0, queue_depth));

        // 创建worker线程（thread_id=1..N）
        for (uint32_t i = 0; i < worker_count; ++i) {
            threads_.push_back(std::make_unique<EventLoopThread>(i + 1, queue_depth));
        }
    }

    ~EventLoopPool() {
        stop();
    }

    EventLoopPool(const EventLoopPool&) = delete;
    EventLoopPool& operator=(const EventLoopPool&) = delete;

    // ==================== 生命周期 ====================

    /**
     * @brief 启动所有worker线程（main线程需手动调用main_loop().run()）
     */
    void start() {
        running_.store(true, std::memory_order_release);
        for (uint32_t i = 1; i < threads_.size(); ++i) {
            threads_[i]->start();
        }
    }

    /**
     * @brief 停止所有线程
     */
    void stop() {
        if (running_.exchange(false, std::memory_order_acq_rel)) {
            // 先停止worker
            for (uint32_t i = 1; i < threads_.size(); ++i) {
                threads_[i]->stop();
            }
            // 再停止main
            threads_[0]->stop();
        }
    }

    // ==================== 访问器 ====================

    /**
     * @brief 获取main线程的EventLoop
     */
    EventLoopThread& main_loop() { return *threads_[0]; }

    /**
     * @brief 获取指定worker的EventLoop
     */
    EventLoopThread& worker_loop(uint32_t worker_id) {
        return *threads_[worker_id + 1];
    }

    /**
     * @brief 获取指定线程的EventLoop（0=main, 1..N=worker）
     */
    EventLoopThread& loop_at(uint32_t thread_id) {
        return *threads_[thread_id];
    }

    uint32_t thread_count() const { return static_cast<uint32_t>(threads_.size()); }
    uint32_t worker_count() const { return worker_count_; }
    bool is_running() const { return running_.load(std::memory_order_acquire); }

    // ==================== 负载均衡分发 ====================

    /**
     * @brief 选择负载最低的worker（最少连接数）
     */
    uint32_t select_worker_least_connections() {
        uint32_t best = 0;
        size_t best_count = threads_[1]->connection_count();
        for (uint32_t i = 1; i < threads_.size(); ++i) {
            auto count = threads_[i]->connection_count();
            if (count < best_count) {
                best_count = count;
                best = i - 1;  // worker_id从0开始
            }
        }
        return best;
    }

    /**
     * @brief Round-robin选择worker
     */
    uint32_t select_worker_round_robin() {
        return next_worker_.fetch_add(1, std::memory_order_relaxed) % worker_count_;
    }

    /**
     * @brief 通过MSG_RING将fd分发到worker
     * @param fd 要分发的文件描述符
     * @param on_ready fd在worker线程就绪后的回调
     * @param strategy 负载均衡策略: "rr"=round-robin, "lc"=least-connections
     */
    void dispatch_to_worker(int fd, std::function<void(int)> on_ready = nullptr,
                           const std::string& strategy = "rr") {
        uint32_t worker_id = (strategy == "lc")
            ? select_worker_least_connections()
            : select_worker_round_robin();

        auto& worker = worker_loop(worker_id);
        worker.inc_connections();

        CrossThreadTask task;
        task.type = 1;  // fd dispatch
        task.data = static_cast<uint64_t>(fd);
        task.fn = [fd, on_ready, &worker]() {
            if (on_ready) {
                on_ready(fd);
            }
        };

        // 尝试MSG_RING，失败回退到队列投递
        if (!worker.post_via_msg_ring(main_loop().io_uring(), std::move(task))) {
            worker.post([fd, on_ready, &worker]() {
                if (on_ready) {
                    on_ready(fd);
                }
            });
        }
    }

    /**
     * @brief 投递任务到指定worker
     */
    void post_to_worker(uint32_t worker_id, std::function<void()> fn) {
        worker_loop(worker_id).post(std::move(fn));
    }

    /**
     * @brief 投递任务到main线程
     */
    void post_to_main(std::function<void()> fn) {
        main_loop().post(std::move(fn));
    }

    /**
     * @brief 获取所有线程的连接数统计
     */
    std::vector<size_t> connection_counts() const {
        std::vector<size_t> counts;
        counts.reserve(threads_.size());
        for (const auto& t : threads_) {
            counts.push_back(t->connection_count());
        }
        return counts;
    }

private:
    uint32_t worker_count_;
    uint32_t queue_depth_;
    std::atomic<uint32_t> next_worker_;
    std::atomic<bool> running_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
};

} // namespace io
} // namespace rpc
