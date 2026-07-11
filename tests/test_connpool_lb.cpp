
/**
 * @file test_connpool_lb.cpp
 * @brief connection_pool + load_balancer unit tests
 */
#include "../framework.hpp"
#include "../rpc/loadbalance/load_balancer.hpp"
#include <iostream>
#include <cassert>
#include <map>

using namespace rpc;

static int pass_count = 0;
static int fail_count = 0;

#define TEST(name) do { std::cout << "[TEST] " << name << " ... "; } while(0)
#define PASS() do { std::cout << "PASS" << std::endl; ++pass_count; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << std::endl; ++fail_count; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

// ==================== LoadBalancer tests ====================

void test_round_robin_basic() {
    TEST("RoundRobin basic round-robin");
    RoundRobinLoadBalancer lb;
    std::vector<Endpoint> eps = {
        {"10.0.0.1", 8001, 1, true},
        {"10.0.0.2", 8002, 1, true},
        {"10.0.0.3", 8003, 1, true},
    };
    lb.update(eps);

    auto e1 = lb.select(); CHECK(e1 && e1->host == "10.0.0.1", "1st select");
    auto e2 = lb.select(); CHECK(e2 && e2->host == "10.0.0.2", "2nd select");
    auto e3 = lb.select(); CHECK(e3 && e3->host == "10.0.0.3", "3rd select");
    auto e4 = lb.select(); CHECK(e4 && e4->host == "10.0.0.1", "4th wraps around");
    PASS();
}

void test_round_robin_empty() {
    TEST("RoundRobin empty endpoints");
    RoundRobinLoadBalancer lb;
    auto e = lb.select();
    CHECK(!e.has_value(), "should return nullopt for empty");
    PASS();
}

void test_round_robin_single() {
    TEST("RoundRobin single endpoint");
    RoundRobinLoadBalancer lb;
    lb.update({{"10.0.0.1", 8001, 1, true}});
    for (int i = 0; i < 5; ++i) {
        auto e = lb.select();
        CHECK(e && e->host == "10.0.0.1", "always same endpoint");
    }
    PASS();
}

void test_weighted_round_robin() {
    TEST("WeightedRoundRobin weighted distribution");
    WeightedRoundRobinLoadBalancer lb;
    std::vector<Endpoint> eps = {
        {"A", 8001, 3, true},
        {"B", 8002, 1, true},
    };
    lb.update(eps);

    std::map<std::string, int> counts;
    for (int i = 0; i < 40; ++i) {
        auto e = lb.select();
        CHECK(e.has_value(), "select returns value");
        counts[e->host]++;
    }
    CHECK(counts["A"] == 30, "A selected 30 times");
    CHECK(counts["B"] == 10, "B selected 10 times");
    PASS();
}

void test_weighted_empty() {
    TEST("WeightedRoundRobin empty endpoints");
    WeightedRoundRobinLoadBalancer lb;
    auto e = lb.select();
    CHECK(!e.has_value(), "should return nullopt for empty");
    PASS();
}

void test_random_lb() {
    TEST("RandomLoadBalancer basic");
    RandomLoadBalancer lb;
    std::vector<Endpoint> eps = {
        {"10.0.0.1", 8001, 1, true},
        {"10.0.0.2", 8002, 1, true},
    };
    lb.update(eps);

    bool found_first = false, found_second = false;
    for (int i = 0; i < 100; ++i) {
        auto e = lb.select();
        CHECK(e.has_value(), "select returns value");
        if (e->host == "10.0.0.1") found_first = true;
        if (e->host == "10.0.0.2") found_second = true;
    }
    CHECK(found_first && found_second, "both endpoints selected at least once");
    PASS();
}

void test_random_lb_empty() {
    TEST("RandomLoadBalancer empty endpoints");
    RandomLoadBalancer lb;
    auto e = lb.select();
    CHECK(!e.has_value(), "should return nullopt for empty");
    PASS();
}

// ==================== ConnectionPool tests ====================

void test_conn_pool_config() {
    TEST("ConnectionPool config defaults");
    net::ConnectionPoolConfig config;
    CHECK(config.max_connections == 1000, "max_connections default");
    CHECK(config.min_idle == 10, "min_idle default");
    CHECK(config.max_idle == 100, "max_idle default");
    CHECK(config.idle_timeout_ms == 60000, "idle_timeout_ms default");
    CHECK(config.connect_timeout_ms == 5000, "connect_timeout_ms default");
    PASS();
}

void test_conn_pool_create_shutdown() {
    TEST("ConnectionPool create and shutdown");
    net::ConnectionPoolConfig config;
    config.max_connections = 5;
    net::ConnectionPool pool("127.0.0.1", 9999, config);
    CHECK(pool.total() == 0, "initial total=0");
    CHECK(pool.active() == 0, "initial active=0");
    pool.shutdown();
    CHECK(pool.total() == 0, "after shutdown total=0");
    CHECK(pool.active() == 0, "after shutdown active=0");
    PASS();
}

void test_conn_pool_stats() {
    TEST("ConnectionPool stats tracking");
    net::ConnectionPoolConfig config;
    config.max_connections = 10;
    net::ConnectionPool pool("127.0.0.1", 9999, config);

    auto conn = pool.acquire();
    if (conn) {
        CHECK(pool.total() >= 1, "total >= 1 after acquire");
        CHECK(pool.active() >= 1, "active >= 1 after acquire");

        pool.release(conn);
        CHECK(pool.active() == 0, "active=0 after release");
    }
    pool.shutdown();
    PASS();
}

void test_conn_pool_manager() {
    TEST("ConnectionPoolManager get/create pool");
    auto& mgr = net::ConnectionPoolManager::instance();
    auto pool1 = mgr.get_pool("10.0.0.1", 8001);
    auto pool2 = mgr.get_pool("10.0.0.1", 8001);
    CHECK(pool1 != nullptr, "pool1 not null");
    CHECK(pool2 != nullptr, "pool2 not null");
    CHECK(pool1.get() == pool2.get(), "same pool for same endpoint");

    auto pool3 = mgr.get_pool("10.0.0.2", 8002);
    CHECK(pool3 != nullptr, "pool3 not null");
    CHECK(pool1.get() != pool3.get(), "different pool for different endpoint");
    PASS();
}

void test_endpoint_to_string() {
    TEST("Endpoint to_string");
    Endpoint ep{"192.168.1.1", 8080, 1, true};
    CHECK(ep.to_string() == "192.168.1.1:8080", "to_string format");
    PASS();
}

int main() {
    std::cout << "=== ConnectionPool + LoadBalancer Unit Tests ===" << std::endl;
    std::cout << std::endl;

    test_round_robin_basic();
    test_round_robin_empty();
    test_round_robin_single();
    test_weighted_round_robin();
    test_weighted_empty();
    test_random_lb();
    test_random_lb_empty();

    test_conn_pool_config();
    test_conn_pool_create_shutdown();
    test_conn_pool_stats();
    test_conn_pool_manager();

    test_endpoint_to_string();

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Results: " << pass_count << " passed, " << fail_count << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return fail_count > 0 ? 1 : 0;
}
