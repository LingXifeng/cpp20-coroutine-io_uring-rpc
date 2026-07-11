/**
 * @file benchmark.cpp
 * @brief 性能基准测试
 * 
 * 测试项目：
 * 1. 协程创建和切换开销
 * 2. 序列化/反序列化吞吐
 * 3. Buffer读写性能
 * 4. io_uring I/O性能
 * 5. RPC调用QPS
 */

#include "../framework.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>

using namespace rpc;

// 计时器
class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }
    
    double elapsed_us() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start_).count();
    }
    
private:
    std::chrono::high_resolution_clock::time_point start_;
};

// ============ 协程性能测试 ============

coroutine::Task<int> bench_task(int value) {
    co_return value + 1;
}

coroutine::Task<void> bench_void_task() {
    co_return;
}

void benchmark_coroutine() {
    std::cout << "\n=== Coroutine Performance ===" << std::endl;
    
    const int N = 100000;
    
    // Task<int> 创建+启动+获取
    {
        Timer t;
        volatile int sum = 0;
        for (int i = 0; i < N; ++i) {
            auto task = bench_task(i);
            sum += task.start().get();
        }
        double ms = t.elapsed_ms();
        double qps = N / (ms / 1000.0);
        std::cout << "Task<int> create+run: " << N << " iterations in " 
                  << ms << " ms (" << (ms * 1000 / N) << " us/op)" << std::endl;
        std::cout << "  QPS: " << (long long)qps << std::endl;
    }
    
    // Task<void> 创建+启动
    {
        Timer t;
        for (int i = 0; i < N; ++i) {
            auto task = bench_void_task();
            task.start().get();
        }
        double ms = t.elapsed_ms();
        std::cout << "Task<void> create+run: " << N << " iterations in " 
                  << ms << " ms (" << (ms * 1000 / N) << " us/op)" << std::endl;
    }
}

// ============ 序列化性能测试 ============

void benchmark_serializer() {
    std::cout << "\n=== Serializer Performance ===" << std::endl;
    
    const int N = 1000000;
    
    // int32 encode
    {
        Timer t;
        std::vector<uint8_t> buf;
        buf.reserve(128);
        for (int i = 0; i < N; ++i) {
            buf.clear();
            BinarySerializer::encode(buf, i);
        }
        double ms = t.elapsed_ms();
        double qps = N / (ms / 1000.0);
        std::cout << "int32 encode: " << N << " ops in " << ms << " ms" << std::endl;
        std::cout << "  QPS: " << (long long)qps << std::endl;
    }
    
    // int32 decode
    {
        std::vector<uint8_t> buf;
        BinarySerializer::encode(buf, 42);
        
        Timer t;
        int32_t val;
        for (int i = 0; i < N; ++i) {
            BinarySerializer::decode(buf.data(), buf.size(), val);
        }
        double ms = t.elapsed_ms();
        double qps = N / (ms / 1000.0);
        std::cout << "int32 decode: " << N << " ops in " << ms << " ms" << std::endl;
        std::cout << "  QPS: " << (long long)qps << std::endl;
    }
    
    // string encode
    {
        Timer t;
        std::vector<uint8_t> buf;
        buf.reserve(256);
        std::string s = "Hello, RPC Framework Benchmark!";
        for (int i = 0; i < N; ++i) {
            buf.clear();
            BinarySerializer::encode(buf, s);
        }
        double ms = t.elapsed_ms();
        double qps = N / (ms / 1000.0);
        std::cout << "string encode: " << N << " ops in " << ms << " ms" << std::endl;
        std::cout << "  QPS: " << (long long)qps << std::endl;
    }
}

// ============ Buffer性能测试 ============

void benchmark_buffer() {
    std::cout << "\n=== Buffer Performance ===" << std::endl;
    
    const int N = 1000000;
    const size_t BUF_SIZE = 4096;
    
    // 小块写入
    {
        Timer t;
        net::Buffer buf(BUF_SIZE);
        const char* data = "Hello";
        size_t len = 5;
        for (int i = 0; i < N; ++i) {
            if (buf.writable() < len) {
                buf.reset();
            }
            buf.write(data, len);
        }
        double ms = t.elapsed_ms();
        double qps = N / (ms / 1000.0);
        std::cout << "Small write (5B): " << N << " ops in " << ms << " ms" << std::endl;
        std::cout << "  QPS: " << (long long)qps << std::endl;
    }
    
    // 大块写入
    {
        Timer t;
        net::Buffer buf(BUF_SIZE * 4);
        std::vector<char> data(1024, 0x58);
        for (int i = 0; i < N; ++i) {
            if (buf.writable() < 1024) {
                buf.reset();
            }
            buf.write(data.data(), 1024);
        }
        double ms = t.elapsed_ms();
        double qps = N / (ms / 1000.0);
        std::cout << "Large write (1KB): " << N << " ops in " << ms << " ms" << std::endl;
        std::cout << "  QPS: " << (long long)qps << std::endl;
    }
}

// ============ io_uring性能测试 ============

void benchmark_io_uring() {
    std::cout << "\n=== io_uring Performance ===" << std::endl;
    
#ifdef HAVE_IO_URING
    const int N = 100000;
    
    // nop操作吞吐
    {
        io::IoUring ring;
        if (!ring.is_valid()) {
            std::cout << "io_uring init failed, skipping" << std::endl;
            return;
        }
        
        Timer t;
        int completed = 0;
        for (int i = 0; i < N; ++i) {
            ring.prepare_nop([&completed](int res, uint32_t) {
                if (res == 0) ++completed;
            });
            if ((i + 1) % 256 == 0) {
                ring.submit_and_wait(1);
            }
        }
        ring.submit_and_wait(256);
        // Drain remaining completions
        for (int i = 0; i < 10; ++i) {
            ring.run_once(1);
        }
        double ms = t.elapsed_ms();
        double qps = completed / (ms / 1000.0);
        std::cout << "nop operations: " << completed << "/" << N 
                  << " completed in " << ms << " ms" << std::endl;
        std::cout << "  QPS: " << (long long)qps << std::endl;
    }
#else
    std::cout << "io_uring not available, skipping" << std::endl;
#endif
}

// ============ RPC调用性能测试 ============

void benchmark_rpc() {
    std::cout << "\n=== RPC Call Performance ===" << std::endl;
    
    const int N = 100000;
    
    // 服务注册+查找
    {
        // 注册
        ServiceRegistry::instance().register_method(
            "BenchService", "echo",
            [](const std::vector<uint8_t>& data) 
                -> coroutine::Task<std::vector<uint8_t>> {
                co_return data;
            },
            "Echo method"
        );
        
        // 查找
        Timer t;
        for (int i = 0; i < N; ++i) {
            auto m = ServiceRegistry::instance().find_method("BenchService", "echo");
            (void)m;
        }
        double ms = t.elapsed_ms();
        double qps = N / (ms / 1000.0);
        std::cout << "Service lookup: " << N << " ops in " << ms << " ms" << std::endl;
        std::cout << "  QPS: " << (long long)qps << std::endl;
    }
}

// ============ 主函数 ============

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  RPC Framework Performance Benchmark" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Compiler: GCC " << __GNUC__ << "." << __GNUC_MINOR__ << std::endl;
    std::cout << "C++ Standard: C++" << __cplusplus << std::endl;
#ifdef HAVE_IO_URING
    std::cout << "io_uring: Enabled" << std::endl;
#else
    std::cout << "io_uring: Disabled" << std::endl;
#endif
    
    benchmark_coroutine();
    benchmark_serializer();
    benchmark_buffer();
    benchmark_io_uring();
    benchmark_rpc();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Benchmark Complete!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
