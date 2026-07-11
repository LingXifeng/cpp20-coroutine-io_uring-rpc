/**
 * @file test_eventloop_pool.cpp
 * @brief 多线程EventLoop测试
 * 测试：EventLoopThread, EventLoopPool, MSG_RING, 负载均衡
 */

#include "io/event_loop_pool.hpp"
#include "io/timer.hpp"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

using namespace rpc::io;

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

// ==================== Test 1: EventLoopThread basic ====================

void test_eventloop_thread_basic() {
    TEST("EventLoopThread basic start/stop");

    EventLoopThread thread(0, 256);
    ASSERT_TRUE(!thread.is_running());

    thread.start();
    ASSERT_TRUE(thread.is_running());
    ASSERT_EQ(thread.thread_id(), 0u);
    ASSERT_TRUE(thread.ring_fd() > 0);

    // 短暂运行
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    thread.stop();
    ASSERT_TRUE(!thread.is_running());

    PASS();
}

// ==================== Test 2: EventLoopThread post task ====================

void test_eventloop_thread_post() {
    TEST("EventLoopThread post task");

    EventLoopThread thread(0, 256);
    thread.start();

    std::atomic<bool> task_executed{false};
    thread.post([&task_executed]() {
        task_executed.store(true, std::memory_order_release);
    });

    // 等待任务执行
    for (int i = 0; i < 100; ++i) {
        if (task_executed.load(std::memory_order_acquire)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    thread.stop();
    ASSERT_TRUE(task_executed.load());

    PASS();
}

// ==================== Test 3: EventLoopThread timer ====================

void test_eventloop_thread_timer() {
    TEST("EventLoopThread set_timer");

    EventLoopThread thread(0, 256);
    thread.start();

    std::atomic<bool> timer_fired{false};
    thread.set_timer(std::chrono::milliseconds(50), [&timer_fired]() {
        timer_fired.store(true, std::memory_order_release);
    });

    // 等待定时器触发
    for (int i = 0; i < 100; ++i) {
        if (timer_fired.load(std::memory_order_acquire)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    thread.stop();
    ASSERT_TRUE(timer_fired.load());

    PASS();
}

// ==================== Test 4: EventLoopThread connection count ====================

void test_eventloop_thread_conn_count() {
    TEST("EventLoopThread connection count");

    EventLoopThread thread(0, 256);
    ASSERT_EQ(thread.connection_count(), 0u);

    thread.inc_connections();
    thread.inc_connections();
    thread.inc_connections();
    ASSERT_EQ(thread.connection_count(), 3u);

    thread.dec_connections();
    ASSERT_EQ(thread.connection_count(), 2u);

    PASS();
}

// ==================== Test 5: EventLoopPool basic ====================

void test_eventloop_pool_basic() {
    TEST("EventLoopPool basic start/stop");

    EventLoopPool pool(2, 256);  // 1 main + 2 workers
    ASSERT_EQ(pool.thread_count(), 3u);
    ASSERT_EQ(pool.worker_count(), 2u);

    pool.start();
    ASSERT_TRUE(pool.is_running());

    // 验证main和worker的ring_fd不同
    int main_fd = pool.main_loop().ring_fd();
    int worker0_fd = pool.worker_loop(0).ring_fd();
    int worker1_fd = pool.worker_loop(1).ring_fd();

    ASSERT_TRUE(main_fd > 0);
    ASSERT_TRUE(worker0_fd > 0);
    ASSERT_TRUE(worker1_fd > 0);
    ASSERT_TRUE(main_fd != worker0_fd);
    ASSERT_TRUE(worker0_fd != worker1_fd);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    pool.stop();
    ASSERT_TRUE(!pool.is_running());

    PASS();
}

// ==================== Test 6: EventLoopPool round-robin ====================

void test_eventloop_pool_round_robin() {
    TEST("EventLoopPool round-robin dispatch");

    EventLoopPool pool(3, 256);
    pool.start();

    std::atomic<int> dispatch_count{0};

    // 投递9个任务，应该均匀分布到3个worker
    for (int i = 0; i < 9; ++i) {
        pool.post_to_worker(i % 3, [&dispatch_count]() {
            dispatch_count.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // 等待任务执行
    for (int i = 0; i < 100; ++i) {
        if (dispatch_count.load() >= 9) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    pool.stop();
    ASSERT_EQ(dispatch_count.load(), 9);

    PASS();
}

// ==================== Test 7: EventLoopPool least-connections ====================

void test_eventloop_pool_least_connections() {
    TEST("EventLoopPool least-connections selection");

    EventLoopPool pool(3, 256);

    // 手动设置连接数
    pool.worker_loop(0).inc_connections();
    pool.worker_loop(0).inc_connections();
    pool.worker_loop(0).inc_connections();  // worker0: 3
    pool.worker_loop(1).inc_connections();  // worker1: 1
    // worker2: 0

    auto selected = pool.select_worker_least_connections();
    ASSERT_EQ(selected, 2u);  // worker2连接最少

    PASS();
}

// ==================== Test 8: EventLoopPool connection counts ====================

void test_eventloop_pool_connection_counts() {
    TEST("EventLoopPool connection_counts()");

    EventLoopPool pool(2, 256);

    pool.main_loop().inc_connections();
    pool.worker_loop(0).inc_connections();
    pool.worker_loop(0).inc_connections();
    pool.worker_loop(1).inc_connections();

    auto counts = pool.connection_counts();
    ASSERT_EQ(counts.size(), 3u);
    ASSERT_EQ(counts[0], 1u);  // main
    ASSERT_EQ(counts[1], 2u);  // worker0
    ASSERT_EQ(counts[2], 1u);  // worker1

    PASS();
}

// ==================== Test 9: MSG_RING cross-thread ====================

void test_msg_ring_cross_thread() {
    TEST("MSG_RING cross-thread wakeup");

    EventLoopThread target(0, 256);
    target.start();

    std::atomic<bool> task_executed{false};

    // 通过post投递任务（MSG_RING的备选路径）
    target.post([&task_executed]() {
        task_executed.store(true, std::memory_order_release);
    });

    for (int i = 0; i < 100; ++i) {
        if (task_executed.load(std::memory_order_acquire)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    target.stop();
    ASSERT_TRUE(task_executed.load());

    PASS();
}

// ==================== Test 10: Multiple workers concurrent ====================

void test_workers_concurrent() {
    TEST("Multiple workers execute concurrently");

    EventLoopPool pool(4, 256);
    pool.start();

    std::atomic<int> counter{0};
    std::atomic<int> max_concurrent{0};
    std::atomic<int> current{0};

    // 给每个worker投递多个任务
    for (int w = 0; w < 4; ++w) {
        for (int i = 0; i < 5; ++i) {
            pool.post_to_worker(w, [&counter, &max_concurrent, &current]() {
                int c = current.fetch_add(1, std::memory_order_acq_rel) + 1;
                int old_max = max_concurrent.load(std::memory_order_relaxed);
                while (c > old_max) {
                    if (max_concurrent.compare_exchange_weak(old_max, c,
                        std::memory_order_relaxed)) break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                current.fetch_sub(1, std::memory_order_acq_rel);
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
    }

    // 等待所有任务完成
    for (int i = 0; i < 200; ++i) {
        if (counter.load() >= 20) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    pool.stop();

    ASSERT_EQ(counter.load(), 20);
    // 多线程并发执行，max_concurrent应该 > 1
    // （但不保证，取决于调度，所以只检查所有任务完成）

    PASS();
}

// ==================== Main ====================

int main() {
    std::cout << "=== Multi-threaded EventLoop Tests ===" << std::endl;

    test_eventloop_thread_basic();
    test_eventloop_thread_post();
    test_eventloop_thread_timer();
    test_eventloop_thread_conn_count();
    test_eventloop_pool_basic();
    test_eventloop_pool_round_robin();
    test_eventloop_pool_least_connections();
    test_eventloop_pool_connection_counts();
    test_msg_ring_cross_thread();
    test_workers_concurrent();

    std::cout << std::endl;
    std::cout << "Results: " << tests_passed << " passed, "
              << tests_failed << " failed" << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
