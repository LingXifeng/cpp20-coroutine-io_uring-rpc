/**
 * @file io_uring.hpp
 * @brief io_uring异步IO封装 - Linux内核5.4+高性能异步IO
 *
 * io_uring相对于epoll的技术优势：
 * 1. 零拷贝：用户态与内核态共享内存，避免数据拷贝
 * 2. 批量提交：一次系统调用提交多个I/O请求
 * 3. 无锁设计：使用环形缓冲区，减少锁竞争
 * 4. 异步完成：提交后立即返回，完成时通知
 * 5. MSG_RING：零系统调用跨线程唤醒
 * 6. ASYNC_CANCEL：异步取消正在进行的操作
 * 7. Linked Timeout：操作级超时，自动取消
 */

#pragma once

#include <liburing.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <cstring>
#include <memory>
#include <functional>
#include <atomic>
#include <stdexcept>
#include <system_error>
#include <vector>
#include <span>
#include <chrono>

namespace rpc {
namespace io {

/**
 * @brief io_uring错误类型
 */
enum class IoUringError {
    SetupFailed = 1,
    QueueFull,
    SubmitFailed,
    InvalidParameter,
    Overflow,
    Timeout,
    Cancelled
};

/**
 * @brief io_uring异常类
 */
class IoUringException : public std::runtime_error {
public:
    IoUringException(IoUringError err, const std::string& msg)
        : std::runtime_error(msg), error_(err) {}

    IoUringError error() const { return error_; }

private:
    IoUringError error_;
};

/**
 * @brief 超时异常
 */
class TimeoutException : public std::runtime_error {
public:
    explicit TimeoutException(const std::string& msg = "operation timed out")
        : std::runtime_error(msg) {}
};

/**
 * @brief 取消异常
 */
class CancelledException : public std::runtime_error {
public:
    explicit CancelledException(const std::string& msg = "operation cancelled")
        : std::runtime_error(msg) {}
};

/**
 * @brief io_uring配置
 */
struct IoUringConfig {
    uint32_t queue_depth = 1024;
    uint32_t sq_thread_cpu = 0;
    uint32_t sq_thread_idle = 2000;
    bool sq_poll = false;
    bool iopoll = false;
    bool fixed_files = false;
    uint32_t fixed_files_count = 0;
    bool setup_coop_taskrun = false;  // IORING_SETUP_COOP_TASKRUN (kernel 5.19+)
};

/**
 * @brief 完成事件回调类型
 */
using CompletionCallback = std::function<void(int result, uint32_t flags)>;

/**
 * @brief io_uring实例封装
 *
 * 封装liburing，提供：
 * - SQE提交接口（含MSG_RING, ASYNC_CANCEL, linked timeout）
 * - CQE处理接口
 * - 批量操作支持
 * - 资源管理
 */
class IoUring {
public:
    explicit IoUring(const IoUringConfig& config = IoUringConfig{})
        : config_(config)
        , ring_{}
        , running_(false)
    {
        setup();
    }

    ~IoUring() {
        shutdown();
    }

    IoUring(const IoUring&) = delete;
    IoUring& operator=(const IoUring&) = delete;

    bool is_valid() const { return ring_.sq.ring_sz != 0; }

    IoUring(IoUring&& other) noexcept
        : config_(other.config_)
        , ring_(other.ring_)
        , running_(other.running_.load())
    {
        other.ring_ = {};
        other.running_.store(false);
    }

    IoUring& operator=(IoUring&& other) noexcept {
        if (this != &other) {
            shutdown();
            config_ = other.config_;
            ring_ = other.ring_;
            running_.store(other.running_.load());
            other.ring_ = {};
            other.running_.store(false);
        }
        return *this;
    }

    uint32_t queue_depth() const { return config_.queue_depth; }
    uint32_t available_sqe() const { return io_uring_sq_space_left(&ring_); }
    uint32_t pending_cqe() const { return io_uring_cq_ready(&ring_); }

    /**
     * @brief 获取ring的文件描述符（用于MSG_RING目标）
     */
    int ring_fd() const { return ring_.ring_fd; }

    // ==================== SQE提交接口 ====================

    io_uring_sqe* prepare_read(int fd, void* buffer, size_t size,
                               off_t offset, CompletionCallback callback) {
        auto* sqe = get_sqe();
        if (!sqe) throw IoUringException(IoUringError::QueueFull, "SQE queue is full");
        io_uring_prep_read(sqe, fd, buffer, size, offset);
        set_callback(sqe, std::move(callback));
        return sqe;
    }

    io_uring_sqe* prepare_write(int fd, const void* buffer, size_t size,
                                off_t offset, CompletionCallback callback) {
        auto* sqe = get_sqe();
        if (!sqe) throw IoUringException(IoUringError::QueueFull, "SQE queue is full");
        io_uring_prep_write(sqe, fd, buffer, size, offset);
        set_callback(sqe, std::move(callback));
        return sqe;
    }

    io_uring_sqe* prepare_recv(int fd, void* buffer, size_t size,
                               int flags, CompletionCallback callback) {
        auto* sqe = get_sqe();
        if (!sqe) throw IoUringException(IoUringError::QueueFull, "SQE queue is full");
        io_uring_prep_recv(sqe, fd, buffer, size, flags);
        set_callback(sqe, std::move(callback));
        return sqe;
    }

    io_uring_sqe* prepare_send(int fd, const void* buffer, size_t size,
                               int flags, CompletionCallback callback) {
        auto* sqe = get_sqe();
        if (!sqe) throw IoUringException(IoUringError::QueueFull, "SQE queue is full");
        io_uring_prep_send(sqe, fd, buffer, size, flags);
        set_callback(sqe, std::move(callback));
        return sqe;
    }

    io_uring_sqe* prepare_accept(int fd, sockaddr* addr, socklen_t* addrlen,
                                 int flags, CompletionCallback callback) {
        auto* sqe = get_sqe();
        if (!sqe) throw IoUringException(IoUringError::QueueFull, "SQE queue is full");
        io_uring_prep_accept(sqe, fd, addr, addrlen, flags);
        set_callback(sqe, std::move(callback));
        return sqe;
    }

    io_uring_sqe* prepare_connect(int fd, const sockaddr* addr, socklen_t addrlen,
                                  CompletionCallback callback) {
        auto* sqe = get_sqe();
        if (!sqe) throw IoUringException(IoUringError::QueueFull, "SQE queue is full");
        io_uring_prep_connect(sqe, fd, addr, addrlen);
        set_callback(sqe, std::move(callback));
        return sqe;
    }

    /**
     * @brief 准备超时操作 (IORING_OP_TIMEOUT)
     * @param ts 超时时间
     * @param count 等待CQE数量（0=纯时间超时）
     * @param flags SQE flags (如 IOSQE_IO_LINK)
     */
    io_uring_sqe* prepare_timeout(__kernel_timespec* ts, uint32_t count,
                                  CompletionCallback callback, uint32_t sqe_flags = 0) {
        auto* sqe = get_sqe();
        if (!sqe) throw IoUringException(IoUringError::QueueFull, "SQE queue is full");
        io_uring_prep_timeout(sqe, ts, count, 0);
        sqe->flags |= sqe_flags;
        set_callback(sqe, std::move(callback));
        return sqe;
    }

    /**
     * @brief 准备链接超时 (IOSQE_IO_LINK + IORING_OP_TIMEOUT)
     * 前一个SQE完成后如果超时，自动触发此timeout SQE
     * @param ts 超时时间
     * @param callback 超时回调
     * @return timeout SQE指针（需紧跟在主操作SQE之后提交）
     *
     * 用法：
     *   auto* main_sqe = ring.prepare_recv(...);
     *   main_sqe->flags |= IOSQE_IO_LINK;
     *   ring.prepare_link_timeout(&ts, callback);
     *   ring.submit();
     */
    io_uring_sqe* prepare_link_timeout(__kernel_timespec* ts,
                                       CompletionCallback callback) {
        auto* sqe = get_sqe();
        if (!sqe) throw IoUringException(IoUringError::QueueFull, "SQE queue is full");
        io_uring_prep_link_timeout(sqe, ts, 0);
        set_callback(sqe, std::move(callback));
        return sqe;
    }

    /**
     * @brief 准备异步取消操作 (IORING_OP_ASYNC_CANCEL)
     * @param user_data_to_cancel 要取消的SQE的user_data
     * @param callback 取消完成回调
     */
    io_uring_sqe* prepare_async_cancel(uint64_t user_data_to_cancel,
                                       CompletionCallback callback) {
        auto* sqe = get_sqe();
        if (!sqe) throw IoUringException(IoUringError::QueueFull, "SQE queue is full");
        io_uring_prep_cancel(sqe, reinterpret_cast<void*>(user_data_to_cancel), 0);
        set_callback(sqe, std::move(callback));
        return sqe;
    }

    /**
     * @brief 准备MSG_RING操作 (IORING_OP_MSG_RING)
     * 零系统调用跨线程唤醒：向目标io_uring实例发送消息
     * @param target_ring_fd 目标ring的文件描述符
     * @param user_data_msg 发送的消息数据（目标ring的CQE会收到此值）
     * @param callback 完成回调
     */
    io_uring_sqe* prepare_msg_ring(int target_ring_fd, uint64_t user_data_msg,
                                  CompletionCallback callback) {
        auto* sqe = get_sqe();
        if (!sqe) throw IoUringException(IoUringError::QueueFull, "SQE queue is full");
        io_uring_prep_msg_ring(sqe, target_ring_fd, 0, user_data_msg, 0);
        set_callback(sqe, std::move(callback));
        return sqe;
    }

    /**
     * @brief 准备nop操作（用于测试/唤醒）
     */
    io_uring_sqe* prepare_nop(CompletionCallback callback) {
        auto* sqe = get_sqe();
        if (!sqe) throw IoUringException(IoUringError::QueueFull, "SQE queue is full");
        io_uring_prep_nop(sqe);
        set_callback(sqe, std::move(callback));
        return sqe;
    }

    /**
     * @brief 提交所有已准备的SQE
     */
    int submit() {
        int ret = io_uring_submit(&ring_);
        if (ret < 0) {
            throw IoUringException(IoUringError::SubmitFailed,
                                   "io_uring_submit failed: " + std::string(strerror(-ret)));
        }
        return ret;
    }

    /**
     * @brief 提交并等待完成
     */
    int submit_and_wait(uint32_t wait_nr) {
        int ret = io_uring_submit_and_wait(&ring_, wait_nr);
        if (ret < 0) {
            throw IoUringException(IoUringError::SubmitFailed,
                                   "io_uring_submit_and_wait failed: " + std::string(strerror(-ret)));
        }
        return ret;
    }

    // ==================== CQE处理接口 ====================

    bool wait_cqe(io_uring_cqe** cqe) {
        int ret = io_uring_wait_cqe(&ring_, cqe);
        return ret == 0;
    }

    bool peek_cqe(io_uring_cqe** cqe) {
        int ret = io_uring_peek_cqe(&ring_, cqe);
        return ret == 0;
    }

    uint32_t peek_batch_cqe(io_uring_cqe** cqes, uint32_t count) {
        return io_uring_peek_batch_cqe(&ring_, cqes, count);
    }

    void process_cqe(io_uring_cqe* cqe) {
        auto* callback = get_callback(cqe);
        if (callback) {
            (*callback)(cqe->res, cqe->flags);
        }
        io_uring_cqe_seen(&ring_, cqe);
    }

    /**
     * @brief 处理所有待完成的CQE
     */
    uint32_t process_all_cqe() {
        uint32_t processed = 0;
        io_uring_cqe* cqe;
        while (peek_cqe(&cqe)) {
            process_cqe(cqe);
            ++processed;
        }
        return processed;
    }

    /**
     * @brief 事件循环一次迭代
     */
    uint32_t run_once(int timeout_ms = -1) {
        io_uring_cqe* cqe = nullptr;

        if (timeout_ms >= 0) {
            __kernel_timespec ts = {
                .tv_sec = timeout_ms / 1000,
                .tv_nsec = (timeout_ms % 1000) * 1000000L
            };
            auto* sqe = get_sqe();
            if (sqe) {
                io_uring_prep_timeout(sqe, &ts, 1, 0);
                sqe->user_data = 0;
                submit();
            }
        }

        if (wait_cqe(&cqe)) {
            if (cqe->user_data != 0) {
                process_cqe(cqe);
            } else {
                io_uring_cqe_seen(&ring_, cqe);
            }
        }

        return 1 + process_all_cqe();
    }

    /**
     * @brief 获取下一个callback ID（用于linked timeout等需要提前知道ID的场景）
     */
    uint64_t peek_next_callback_id() const {
        return next_callback_id_.load(std::memory_order_relaxed);
    }

private:
    IoUringConfig config_;
    io_uring ring_;
    std::atomic<bool> running_;

    static constexpr size_t kMaxCallbacks = 65536;
    std::vector<CompletionCallback> callbacks_{kMaxCallbacks};
    std::atomic<uint64_t> next_callback_id_{1};

    void setup() {
        io_uring_params params{};

        if (config_.sq_poll) {
            params.flags |= IORING_SETUP_SQPOLL;
            params.sq_thread_cpu = config_.sq_thread_cpu;
            params.sq_thread_idle = config_.sq_thread_idle;
        }

        if (config_.iopoll) {
            params.flags |= IORING_SETUP_IOPOLL;
        }

        if (config_.setup_coop_taskrun) {
            params.flags |= IORING_SETUP_COOP_TASKRUN;
        }

        int ret = io_uring_queue_init_params(config_.queue_depth, &ring_, &params);
        if (ret < 0) {
            throw IoUringException(IoUringError::SetupFailed,
                                   "io_uring_queue_init_params failed: " + std::string(strerror(-ret)));
        }

        running_.store(true);
    }

    void shutdown() {
        if (running_.exchange(false)) {
            io_uring_queue_exit(&ring_);
        }
    }

    io_uring_sqe* get_sqe() {
        return io_uring_get_sqe(&ring_);
    }

    void set_callback(io_uring_sqe* sqe, CompletionCallback callback) {
        uint64_t id = next_callback_id_.fetch_add(1) % kMaxCallbacks;
        callbacks_[id] = std::move(callback);
        sqe->user_data = id;
    }

    CompletionCallback* get_callback(io_uring_cqe* cqe) {
        if (cqe->user_data == 0 || cqe->user_data >= kMaxCallbacks) {
            return nullptr;
        }
        return &callbacks_[cqe->user_data];
    }
};

/**
 * @brief io_uring单例获取
 */
inline IoUring& global_io_uring() {
    static IoUring instance;
    return instance;
}

} // namespace io
} // namespace rpc
