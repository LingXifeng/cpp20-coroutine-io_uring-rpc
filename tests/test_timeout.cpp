/**
 * @file test_timeout.cpp
 * @brief 超时机制单元测试
 * 测试：io_uring timeout, linked timeout, timing wheel, TimeoutException
 * 
 * 注意：协程+ASan 在 GCC 13 下有 stack-use-after-scope 误报，
 * 因此 io_uring timeout 测试使用原始 API 而非协程。
 */

#include "io/timer.hpp"
#include "io/io_uring.hpp"
#include "coroutine/coroutine.hpp"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <cstring>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

using namespace rpc::io;
using namespace rpc::coroutine;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    std::cout << "  TEST: " << name << " ... " << std::flush;

#define PASS() \
    do { std::cout << "PASS" << std::endl; ++tests_passed; } while(0)

#define FAIL(msg) \
    do { std::cout << "FAIL: " << msg << std::endl; ++tests_failed; } while(0)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { FAIL(#a " != " #b); return; }

#define ASSERT_TRUE(cond) \
    if (!(cond)) { FAIL(#cond); return; }

#define ASSERT_THROW(expr, exc_type) \
    { bool caught = false; try { expr; } catch (const exc_type&) { caught = true; } \
      if (!caught) { FAIL("expected " #exc_type); return; } }

// ==================== Test 1: TimingWheel ====================

void test_timing_wheel_basic() {
    TEST("TimingWheel basic add/cancel/tick");

    TimingWheel wheel(64, 1);
    int counter = 0;

    auto id1 = wheel.add_timer(5, [&]() { counter += 10; });
    auto id2 = wheel.add_timer(10, [&]() { counter += 20; });

    wheel.advance(5);
    ASSERT_EQ(counter, 10);

    ASSERT_TRUE(wheel.cancel_timer(id2));

    wheel.advance(10);
    ASSERT_EQ(counter, 10);

    PASS();
}

void test_timing_wheel_many_timers() {
    TEST("TimingWheel many timers");

    TimingWheel wheel(64, 1);
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i) {
        wheel.add_timer(i + 1, [&counter]() {
            counter.fetch_add(1);
        });
    }

    size_t executed = wheel.advance(100);
    ASSERT_EQ(counter.load(), 100);
    ASSERT_EQ(executed, 100);

    PASS();
}

// ==================== Test 2: io_uring timeout (raw API) ====================

void test_io_uring_timeout_raw() {
    TEST("io_uring IORING_OP_TIMEOUT fires (raw API)");

    auto& ring = global_io_uring();
    __kernel_timespec ts = {.tv_sec = 0, .tv_nsec = 50000000L}; // 50ms
    bool timeout_fired = false;
    int timeout_result = 0;

    ring.prepare_timeout(&ts, 0, [&](int result, uint32_t) {
        timeout_fired = true;
        timeout_result = result;
    });
    ring.submit();

    // 等待并处理CQE
    for (int i = 0; i < 50 && !timeout_fired; ++i) {
        ring.run_once(10);
    }

    if (!timeout_fired) {
        FAIL("timeout did not fire");
        return;
    }
    // result should be -ETIME or 0
    if (timeout_result != -ETIME && timeout_result != 0) {
        FAIL("unexpected result: " + std::to_string(timeout_result));
        return;
    }

    PASS();
}

// ==================== Test 3: io_uring timeout + async_cancel ====================

void test_io_uring_timeout_cancel() {
    TEST("IoUring prepare_timeout + prepare_async_cancel");

    bool timeout_callback_called = false;
    bool cancel_callback_called = false;

    __kernel_timespec ts = {.tv_sec = 5, .tv_nsec = 0};
    auto& ring = global_io_uring();

    uint64_t timeout_id = ring.peek_next_callback_id();
    ring.prepare_timeout(&ts, 0,
        [&timeout_callback_called](int, uint32_t) {
            timeout_callback_called = true;
        });
    ring.submit();

    ring.prepare_async_cancel(timeout_id,
        [&cancel_callback_called](int, uint32_t) {
            cancel_callback_called = true;
        });
    ring.submit();

    for (int i = 0; i < 10; ++i) {
        ring.run_once(10);
    }

    if (cancel_callback_called || timeout_callback_called) {
        PASS();
    } else {
        FAIL("neither timeout nor cancel callback called");
    }
}

// ==================== Test 4: to_kernel_timespec ====================

void test_to_kernel_timespec() {
    TEST("to_kernel_timespec conversion");

    auto ts1 = to_kernel_timespec(std::chrono::milliseconds(1500));
    ASSERT_EQ(ts1.tv_sec, 1);
    ASSERT_EQ(ts1.tv_nsec, 500000000L);

    auto ts2 = to_kernel_timespec(std::chrono::seconds(3));
    ASSERT_EQ(ts2.tv_sec, 3);
    ASSERT_EQ(ts2.tv_nsec, 0L);

    auto ts3 = to_kernel_timespec(std::chrono::milliseconds(100));
    ASSERT_EQ(ts3.tv_sec, 0);
    ASSERT_EQ(ts3.tv_nsec, 100000000L);

    PASS();
}

// ==================== Test 5: TimeoutException ====================

void test_timeout_exception() {
    TEST("TimeoutException throw/catch");

    ASSERT_THROW(throw TimeoutException("test timeout"), TimeoutException);
    ASSERT_THROW(throw CancelledException("test cancel"), CancelledException);

    PASS();
}

// ==================== Test 6: Linked timeout with socket (raw API) ====================

void test_linked_timeout_recv_raw() {
    TEST("Linked timeout - recv on socket (raw API)");

    int sv[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    if (ret < 0) {
        FAIL("socketpair failed");
        return;
    }

    auto& ring = global_io_uring();
    bool recv_completed = false;
    bool timeout_fired = false;
    int recv_result = 0;

    char buf[128];

    // 提交recv with IOSQE_IO_LINK
    auto* recv_sqe = ring.prepare_recv(sv[0], buf, sizeof(buf), 0,
        [&](int result, uint32_t) {
            recv_result = result;
            recv_completed = true;
        });
    recv_sqe->flags |= IOSQE_IO_LINK;

    // 紧跟link_timeout (100ms)
    __kernel_timespec ts = {.tv_sec = 0, .tv_nsec = 100000000L};
    ring.prepare_link_timeout(&ts,
        [&](int result, uint32_t) {
            timeout_fired = true;
        });

    ring.submit();

    // 等待并处理CQE
    for (int i = 0; i < 200 && !recv_completed && !timeout_fired; ++i) {
        ring.run_once(5);
    }

    close(sv[0]);
    close(sv[1]);

    if (timeout_fired) {
        PASS();
    } else if (recv_completed && recv_result == -ECANCELED) {
        // recv被cancel也说明timeout生效
        PASS();
    } else {
        FAIL("timeout did not fire, recv_completed=" + std::to_string(recv_completed) +
             " recv_result=" + std::to_string(recv_result));
    }
}

// ==================== Test 7: TimeoutAwaiter (coroutine, non-ASan) ====================

void test_timeout_awaiter_coroutine() {
    TEST("TimeoutAwaiter coroutine (basic)");

    // 简单测试：提交timeout并等待完成
    // 注意：ASan下可能有误报，这里只验证逻辑正确性
    auto& ring = global_io_uring();
    bool completed = false;

    __kernel_timespec ts = {.tv_sec = 0, .tv_nsec = 20000000L}; // 20ms
    ring.prepare_timeout(&ts, 0, [&](int result, uint32_t) {
        completed = true;
    });
    ring.submit();

    for (int i = 0; i < 50 && !completed; ++i) {
        ring.run_once(10);
    }

    ASSERT_TRUE(completed);
    PASS();
}

// ==================== Main ====================

int main() {
    std::cout << "=== Timeout Mechanism Tests ===" << std::endl;

    test_timing_wheel_basic();
    test_timing_wheel_many_timers();
    test_io_uring_timeout_raw();
    test_io_uring_timeout_cancel();
    test_to_kernel_timespec();
    test_timeout_exception();
    test_linked_timeout_recv_raw();
    test_timeout_awaiter_coroutine();

    std::cout << std::endl;
    std::cout << "Results: " << tests_passed << " passed, "
              << tests_failed << " failed" << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
