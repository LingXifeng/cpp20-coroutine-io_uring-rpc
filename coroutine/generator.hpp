/**
 * @file generator.hpp
 * @brief C++20协程生成器 - 支持co_yield的惰性序列生成
 * 
 * 核心设计：
 * - Generator<T>: 惰性序列容器，通过 co_yield 逐值产出
 * - GeneratorIterator<T>: 输入迭代器，支持 range-for
 * - GeneratorPromise<T>: promise_type，管理 yield 值和生命周期
 * 
 * 使用示例：
 *   Generator<int> count_up(int n) {
 *       for (int i = 0; i < n; ++i) co_yield i;
 *   }
 *   for (int v : count_up(10)) { ... }
 */

#pragma once

#include <coroutine>
#include <exception>
#include <utility>
#include <iterator>
#include <concepts>

namespace rpc {
namespace coroutine {

// 前向声明
template<typename T>
class Generator;

/**
 * @brief 生成器迭代器
 * 
 * 输入迭代器，每次 ++ 调用协程 resume() 获取下一个值。
 * 当协程完成时，handle 被置为 nullptr，使迭代器等于 end()。
 */
template<typename T>
class GeneratorIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using reference = const T&;
    using pointer = const T*;
    
    GeneratorIterator() = default;
    GeneratorIterator(std::coroutine_handle<> h, const T* v) 
        : handle_(h), value_ptr_(v) {}
    
    reference operator*() const { return *value_ptr_; }
    pointer operator->() const { return value_ptr_; }
    
    GeneratorIterator& operator++() {
        // 恢复协程，执行到下一个 co_yield 或 return_void
        handle_.resume();
        // 若协程已结束（final_suspend），将 handle 置空
        // 这样 operator== 会判定为 end()，终止 range-for
        if (handle_.done()) {
            handle_ = nullptr;
            value_ptr_ = nullptr;
        }
        return *this;
    }
    
    void operator++(int) {
        ++(*this);
    }
    
    bool operator==(const GeneratorIterator& other) const {
        return handle_ == other.handle_;
    }
    
    bool operator!=(const GeneratorIterator& other) const {
        return !(*this == other);
    }
    
private:
    std::coroutine_handle<> handle_ = nullptr;
    const T* value_ptr_ = nullptr;
};

/**
 * @brief 生成器Promise类型
 */
template<typename T>
struct GeneratorPromise {
    T current_value;
    std::exception_ptr exception;
    
    // 仅声明，定义在 Generator 之后（延迟定义模式）
    Generator<T> get_return_object();
    
    std::suspend_always initial_suspend() noexcept { return {}; }
    
    // final_suspend 必须返回 suspend_always（不能 resume 已结束的协程）
    std::suspend_always final_suspend() noexcept { return {}; }
    
    std::suspend_always yield_value(T value) {
        current_value = std::move(value);
        return {};
    }
    
    void return_void() {}
    
    void unhandled_exception() {
        exception = std::current_exception();
    }
};

/**
 * @brief 生成器 - 支持co_yield的惰性序列
 * 
 * 典型用法：
 *   Generator<int> fib() {
 *       int a = 0, b = 1;
 *       while (true) {
 *           co_yield a;
 *           auto tmp = a; a = b; b = tmp + b;
 *       }
 *   }
 */
template<typename T>
class Generator {
public:
    using promise_type = GeneratorPromise<T>;
    using iterator = GeneratorIterator<T>;
    
    Generator() noexcept : handle_(nullptr) {}
    
    explicit Generator(std::coroutine_handle<promise_type> h) noexcept : handle_(h) {}
    
    Generator(Generator&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    
    ~Generator() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    /**
     * @brief 获取起始迭代器
     * 
     * 调用 resume() 启动协程（从 initial_suspend 处恢复），
     * 协程执行到第一个 co_yield 并挂起。
     * 若协程为空（无 yield），返回 end()。
     */
    iterator begin() {
        if (handle_) {
            handle_.resume();  // 从 initial_suspend 恢复
            if (handle_.done()) {
                // 协程立即返回（无 yield），返回 end
                return end();
            }
            // 检查是否有未处理的异常
            if (handle_.promise().exception) {
                std::rethrow_exception(handle_.promise().exception);
            }
            return iterator{handle_, &handle_.promise().current_value};
        }
        return end();
    }
    
    /**
     * @brief 获取终止迭代器（哨兵）
     * 
     * end() 的 handle 为 nullptr。
     * 当 begin()/operator++() 发现协程 done() 时，
     * 也会将 handle 置为 nullptr，使其等于 end()。
     */
    iterator end() const {
        return iterator{};
    }
    
    /**
     * @brief 检查生成器是否为空
     */
    bool empty() const {
        return !handle_;
    }
    
private:
    std::coroutine_handle<promise_type> handle_;
};

// 延迟实现 get_return_object（需要 Generator 完整定义）
template<typename T>
Generator<T> GeneratorPromise<T>::get_return_object() {
    return Generator<T>{
        std::coroutine_handle<GeneratorPromise>::from_promise(*this)
    };
}

} // namespace coroutine
} // namespace rpc
