# C++20 协程核心原理

## 1. 什么是协程？

协程是一种可以**暂停执行并稍后恢复**的函数。与线程不同：
- 协程暂停时不阻塞线程
- 协程恢复时从暂停点继续执行
- 协程切换开销极低（仅保存/恢复寄存器状态）

## 2. C++20 协程机制

### 2.1 关键字

```cpp
co_await  // 暂停协程，等待表达式完成
co_yield  // 暂停协程，返回一个值（生成器）
co_return // 结束协程，返回最终值
```

### 2.2 协程帧

编译器为每个协程生成一个**协程帧**（Coroutine Frame）：
```
┌─────────────────────────────────────┐
│        Coroutine Frame              │
├─────────────────────────────────────┤
│  Promise Object                     │
│  - 存储返回值                        │
│  - 存储异常                          │
├─────────────────────────────────────┤
│  Resume Point (状态机当前状态)       │
├─────────────────────────────────────┤
│  Local Variables (跨暂停点存活)      │
├─────────────────────────────────────┤
│  Parameters Copy                    │
└─────────────────────────────────────┘
```

### 2.3 Promise Type

Promise Type 控制协程的行为：

```cpp
template<typename T>
struct promise_type {
    T value;                    // 存储返回值
    std::exception_ptr ex;      // 存储异常
    
    // 协程创建时调用
    auto get_return_object();
    
    // 初始化行为
    auto initial_suspend();     // 是否立即暂停
    
    // 结束行为
    auto final_suspend();       // 结束时是否暂停
    
    // 返回值处理
    void return_value(T v);     // co_return
    void return_void();         // co_return;
    
    // 异常处理
    void unhandled_exception();
    
    // co_yield 支持
    auto yield_value(T v);
};
```

## 3. Task<T> 实现

### 3.1 核心结构

```cpp
template<typename T>
class Task {
public:
    struct promise_type {
        T value_;
        std::coroutine_handle<> continuation_;
        
        Task get_return_object() {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }
        
        std::suspend_always initial_suspend() noexcept { return {}; }
        
        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }
                
                void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    // 恢复调用者
                    auto& promise = h.promise();
                    if (promise.continuation_) {
                        promise.continuation_.resume();
                    }
                }
                
                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }
        
        void return_value(T value) { value_ = std::move(value); }
        void unhandled_exception() { std::terminate(); }
    };
    
private:
    std::coroutine_handle<promise_type> handle_;
};
```

### 3.2 co_await 实现

```cpp
template<typename T>
auto Task<T>::operator co_await() {
    struct Awaiter {
        std::coroutine_handle<promise_type> handle;
        
        bool await_ready() {
            // 如果协程已完成，不需要暂停
            return handle.done();
        }
        
        auto await_suspend(std::coroutine_handle<> continuation) {
            // 保存调用者，以便恢复
            handle.promise().continuation_ = continuation;
            // 恢复被等待的协程
            return handle;
        }
        
        T await_resume() {
            // 返回协程结果
            return std::move(handle.promise().value_);
        }
    };
    
    return Awaiter{handle_};
}
```

## 4. 状态机转换

协程被编译为**状态机**：

```cpp
// 源代码
Task<int> example() {
    int a = co_await get_a();    // 状态1: 等待 get_a
    int b = co_await get_b();    // 状态2: 等待 get_b
    co_return a + b;             // 状态3: 完成
}
```

编译后等价于：

```cpp
struct example_frame {
    int state = 0;
    int a, b;
    
    void resume() {
        switch (state) {
            case 0:
                // 发起 get_a
                state = 1;
                return;
            case 1:
                a = /* get_a 结果 */;
                // 发起 get_b
                state = 2;
                return;
            case 2:
                b = /* get_b 结果 */;
                // 返回 a + b
                state = 3;
                return;
        }
    }
};
```

## 5. 与回调/Promise 对比

### 5.1 回调地狱

```cpp
// 回调方式
void get_user(int id, function<void(User)> cb) {
    fetch_from_db(id, [=](auto data) {
        parse_user(data, [=](auto user) {
            fetch_avatar(user.avatar_id, [=](auto avatar) {
                user.avatar = avatar;
                cb(user);
            });
        });
    });
}
```

### 5.2 Promise 链

```cpp
// Promise 方式
auto user = fetch_from_db(id)
    .then([](auto data) { return parse_user(data); })
    .then([](auto user) { 
        return fetch_avatar(user.avatar_id)
            .then([user](auto avatar) {
                user.avatar = avatar;
                return user;
            });
    });
```

### 5.3 协程

```cpp
// 协程方式 - 线性代码！
Task<User> get_user(int id) {
    auto data = co_await fetch_from_db(id);
    auto user = co_await parse_user(data);
    user.avatar = co_await fetch_avatar(user.avatar_id);
    co_return user;
}
```

## 6. 性能分析

### 6.1 内存开销

| 类型 | 大小 |
|------|------|
| 协程帧 | ~64-256 字节（取决于局部变量） |
| Task<T> | 1 个指针（协程句柄） |
| 线程栈 | 1-8 MB |

### 6.2 切换开销

| 操作 | 时间 |
|------|------|
| 协程切换 | ~10-50 ns |
| 线程切换 | ~1-10 μs |
| 进程切换 | ~10-100 μs |

### 6.3 与其他语言对比

| 语言 | 协程实现 | 栈类型 |
|------|----------|--------|
| C++20 | 编译器状态机 | 无栈 |
| Go | goroutine | 有栈（动态增长） |
| Rust | async/await | 无栈 |
| Python | async/await | 有栈 |
| Kotlin | 协程 | 有栈 |

## 7. 最佳实践

### 7.1 避免协程泄漏

```cpp
// 错误：创建了协程但未等待
void bad() {
    auto task = some_coroutine();  // 创建但未执行
    // 函数结束，task 析构，协程泄漏！
}

// 正确：等待协程完成
Task<void> good() {
    co_await some_coroutine();  // 等待完成
}
```

### 7.2 使用 RAII 管理资源

```cpp
Task<void> safe_operation() {
    ResourceGuard guard(acquire_resource());
    co_await do_something();
    // guard 自动释放，即使协程被取消
}
```

### 7.3 异常处理

```cpp
Task<void> with_exception_handling() {
    try {
        co_await risky_operation();
    } catch (const std::exception& e) {
        // 异常在协程内捕获
        co_return;
    }
}
