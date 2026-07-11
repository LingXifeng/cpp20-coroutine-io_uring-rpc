# 零基础复刻：C++20 协程 + io_uring 高性能 RPC 框架学习路线

> **目标**：从零开始，用 8~12 周时间，独立复刻一个基于 C++20 协程与 io_uring 的高性能网络/RPC 框架，达到大厂秋招面试可用水平。
>
> **参考项目**：`~/rpc` — 完整源码约 8,700 行，6 大模块（coroutine / io / net / rpc / idl / build），9 个测试套件 38 个用例全通过，ASan+UBSan 零内存泄漏。

---

## 📋 路线总览

| 阶段 | 主题 | 前置知识 | 周数 | 产出 |
|------|------|----------|------|------|
| Phase 0 | 环境搭建 & 前置知识 | C++ 基础 + Linux 系统编程 + CMake + Git | 1 | 可编译的空项目 |
| Phase 1 | 协程运行时 | Phase 0 + 协程语法/语义 + 协程帧生命周期 + GTest | 2 | coroutine 模块 + 单元测试 |
| Phase 2 | io_uring IO 引擎 | Phase 1 + io_uring 内核机制 + liburing API + 时间轮 | 1.5 | io 模块 + echo benchmark |
| Phase 3 | 网络层 | Phase 2 + TCP/socket + Reactor/Proactor + Buffer 设计 | 1.5 | net 模块 + TCP echo 端到端 |
| Phase 4 | RPC 框架 | Phase 3 + RPC 原理 + 序列化/VarInt + 协议设计 + 服务注册 | 2 | rpc 模块 + 端到端调用 |
| Phase 5 | IDL + 构建 + 测试 | Phase 4 + 编译原理基础 + CMake 高级 + 测试方法论 | 1.5 | idl 模块 + CMake + 全量测试 |
| Phase 6 | 性能优化 & 文档 & 面试 | Phase 5 + perf/flamegraph + io_uring 高级 + 多线程 + 技术写作 | 1.5 | Benchmark + README + 面试材料 |

**总计**：~11 周（全职投入可压缩至 6~8 周）

### 📊 前置知识学习量总览

| 阶段 | 前置知识学习时间 | 编码时间 | 占比 |
|------|-----------------|---------|------|
| Phase 0 | ~30h（C++ 15h + Linux 10h + CMake 3h + Git 1h + 协程/io_uring 入门 1h） | ~5h | 86% 学 + 14% 写 |
| Phase 1 | ~13h（协程语法 8h + 生命周期 3h + GTest 2h） | ~40h | 25% 学 + 75% 写 |
| Phase 2 | ~14h（io_uring 6h + liburing 4h + 桥接 2h + 时间轮 2h） | ~30h | 32% 学 + 68% 写 |
| Phase 3 | ~12h（TCP 6h + Reactor 2h + Buffer 2h + 连接池 2h） | ~30h | 29% 学 + 71% 写 |
| Phase 4 | ~13h（RPC 4h + 序列化 4h + 协议 3h + 注册 2h） | ~40h | 25% 学 + 75% 写 |
| Phase 5 | ~10h（编译原理 4h + IDL 2h + CMake 2h + 测试 2h） | ~30h | 25% 学 + 75% 写 |
| Phase 6 | ~12h（perf 4h + io_uring 高级 3h + 多线程 3h + 写作 2h） | ~30h | 29% 学 + 71% 写 |
| **合计** | **~104h** | **~205h** | **34% 学 + 66% 写** |

> 💡 **零基础总投入约 300 小时**（~104h 学前置 + ~205h 编码调试）。全职 40h/周 ≈ 7.5 周，兼职 20h/周 ≈ 15 周。Phase 0 学习占比最高（86%），后续阶段逐渐变为"边学边做"。

---

## Phase 0：环境搭建 & 前置知识（第 1 周）

### 🎯 目标
搭建 Linux 开发环境，安装必要工具链，建立对 C++20 协程和 io_uring 的基本认知。

### 📖 前置知识（零基础必补）

> ⚠️ 如果你已有 C++/Linux 基础，可跳过本节直接看「环境要求」。以下内容按依赖顺序排列，**必须按顺序学**。

#### 0-A：C++ 基础（~15 小时，如果你只会 C 或 Java）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| 类与对象、RAII | 整个框架基于 RAII 管理资源 | 《C++ Primer》第 7~13 章 | 4h |
| 模板基础 | Task\<T\>、Serializer 等全是模板 | 《C++ Primer》第 16 章 + [cppreference templates](https://en.cppreference.com/w/cpp/language/templates) | 3h |
| `std::unique_ptr` / `std::shared_ptr` | Buffer 链表、Connection 生命周期管理 | 《Effective Modern C++》Item 18~22 | 2h |
| `std::optional` / `std::variant` / `std::any` | RpcResult、WhenAny 返回类型 | [cppreference optional](https://en.cppreference.com/w/cpp/utility/optional) | 1h |
| lambda 与 `std::function` | 服务注册的 handler、回调 | 《C++ Primer》10.3 节 | 1h |
| move 语义与完美转发 | 协程返回值、Buffer 零拷贝 | 《Effective Modern C++》Item 23~30 | 2h |
| `constexpr` / `static_assert` | 编译期常量与检查 | [cppreference constexpr](https://en.cppreference.com/w/cpp/language/constexpr) | 1h |
| `namespace` 与头文件组织 | 项目结构 `rpc::coroutine` 等 | 实践中学习 | 1h |

**最小自测**：能写出以下代码并解释每一行：
```cpp
template<typename T>
class Wrapper {
    std::unique_ptr<T> data_;
public:
    Wrapper(T val) : data_(std::make_unique<T>(std::move(val))) {}
    T& get() { return *data_; }
};
// 解释：为什么用 unique_ptr？为什么 std::move？为什么模板？
```

#### 0-B：Linux 系统编程基础（~10 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| 文件描述符 (fd) | socket 就是 fd，io_uring 操作 fd | 《APUE》第 3 章 | 1h |
| socket API：socket/bind/listen/accept/recv/send | 网络层的基础 | [Beej's Guide](https://beej.us/guide/bgnet/) 前 5 章 | 3h |
| epoll 基础 | 理解 io_uring 的"进化版"，面试对比题 | [epoll 教程](https://man7.org/linux/man-pages/man7/epoll.7.html) | 2h |
| 系统调用与 errno | io_uring 封装需要理解返回值语义 | 《APUE》第 1~2 章 | 1h |
| 进程/线程基础 | 多线程 EventLoop 需要 | 《APUE》第 11~12 章 | 2h |
| `mmap` 与共享内存 | io_uring SQ/CQ 就是共享内存 | 《APUE》第 14 章 | 1h |

**最小自测**：能写出用 epoll 的 TCP echo 服务器（~100 行），理解 ET vs LT。

#### 0-C：CMake 基础（~3 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| `add_executable` / `add_library` | 编译目标和依赖 | [CMake 官方教程](https://cmake.org/cmake/help/latest/guide/tutorial/index.html) | 1h |
| `target_link_libraries` | 链接 liburing、GTest | 同上 | 0.5h |
| `FetchContent` | 自动拉取 Google Test | [CMake FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html) | 0.5h |
| `option` + 条件编译 | ASan/UBSan 开关 | 实践中学习 | 1h |

**最小自测**：能写一个 CMakeLists.txt 编译多文件项目并链接外部库。

#### 0-D：Git 基础（~1 小时，如果你不会 Git）

| 知识点 | 学习方式 |
|--------|---------|
| init / add / commit / log | [Git 官方教程](https://git-scm.com/book/zh/v2) 第 1~2 章 |
| .gitignore | 同上 |
| branch / merge（可选） | 同上第 3 章 |

### 📦 环境要求

| 工具 | 最低版本 | 推荐版本 | 安装命令 |
|------|----------|----------|----------|
| GCC | 12 | 13+ | `sudo apt install g++-13` |
| CMake | 3.20 | 3.28+ | `sudo apt install cmake` |
| liburing | 2.3 | 2.5+ | `git clone https://github.com/axboe/liburing && cd liburing && make && sudo make install` |
| Google Test | 1.12 | latest | CMake FetchContent 自动拉取 |
| clang-format | 14+ | 17+ | `sudo apt install clang-format` |
| ASan/UBSan | — | GCC 内置 | 编译选项 `-fsanitize=address,undefined` |

> **内核要求**：io_uring 需要 Linux 5.4+，推荐 6.1+（更多高级特性如 SQPOLL、register_files）。

### 📚 必读材料（按优先级排序）

1. **C++20 协程**（~4 小时）
   - [cppreference: coroutine](https://en.cppreference.com/w/cpp/language/coroutines) — 语法速查
   - [Lewis Baker: C++ Coroutines Under the Hood](https://lewissbaker.github.io/) — **必读系列**，理解 `promise_type` 生命周期
   - [Asymmetric Transfer](https://lewissbaker.github.io/2020/05/11/understanding_symmetric_transfer) — 理解 `await_suspend` 返回 coroutine_handle 的优化

2. **io_uring**（~3 小时）
   - [io_uring 官方文档](https://unixism.net/loti/) — 入门必读
   - [Lord of the io_uring](https://unixism.net/loti/tutorial/liburing.html) — liburing 教程
   - [io_uring 英文白皮书](https://kernel.dk/io_uring.pdf) — 理解 SQ/CQ 设计

3. **网络编程基础**（~2 小时，如果你已有基础可跳过）
   - 《TCP/IP 详解 卷1》第 17~22 章 — TCP 状态机与拥塞控制
   - [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) — socket API 速查

### 🛠️ 动手任务

```bash
# 1. 创建项目目录
mkdir -p ~/rpc/{src/{coroutine,io,net,rpc,idl},include/{coroutine,io,net,rpc,idl},tests,benchmarks,docs}
cd ~/rpc

# 2. 初始化 Git
git init
cat > .gitignore << 'EOF'
build/
.cache/
compile_commands.json
*.o
*.a
*.so
EOF

# 3. 写最小 CMakeLists.txt
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.20)
project(rpc_framework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)
add_subdirectory(tests)
EOF

# 4. 验证编译
mkdir build && cd build && cmake .. && make
```

### ✅ 阶段检查点
- [ ] `g++ --version` ≥ 12
- [ ] `cmake --version` ≥ 3.20
- [ ] io_uring 系统调用可用（写一个最小测试：`io_uring_setup` + `io_uring_enter`）
- [ ] 空项目可编译，`compile_commands.json` 生成
- [ ] 理解 coroutine `promise_type` 的三个核心回调：`initial_suspend`、`final_suspend`、`get_return_object`
- [ ] 理解 io_uring 的 SQ（提交队列）和 CQ（完成队列）模型

---

## Phase 1：协程运行时（第 2~3 周）

### 🎯 目标
实现完整的协程调度器，包括 Task、Generator、WhenAll、WhenAny、WithTimeout 等组合原语。这是整个框架的核心基石。

### 📖 前置知识

> Phase 0 的 C++ 基础（模板、move 语义、unique_ptr）是硬性前提。本阶段需要**深入理解**协程机制，不能只看一遍就动手。

#### 1-A：C++20 协程语法与语义（~8 小时，核心必学）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| `co_await` / `co_return` / `co_yield` 语法 | 三个协程关键字，所有代码都在用 | [cppreference: coroutines](https://en.cppreference.com/w/cpp/language/coroutines) 通读 | 1h |
| `promise_type` 的 6 个核心回调 | Task 的灵魂，每个回调决定协程行为 | [Lewis Baker: The C++ Coroutine Model](https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await/) **必读** | 2h |
| `coroutine_handle` — 协程的"指针" | 恢复协程、对称转移都需要 handle | 同上 | 1h |
| `await_ready` / `await_suspend` / `await_resume` | 每个 Awaiter 必须实现的三件套 | [Lewis Baker: Understanding Operator co_await](https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await/) | 2h |
| Symmetric Transfer（对称转移） | `await_suspend` 返回 handle，避免栈溢出 | [Asymmetric Transfer](https://lewissbaker.github.io/2020/05/11/understanding_symmetric_transfer) **必读** | 1h |
| `std::suspend_always` / `std::suspend_never` | initial_suspend / final_suspend 的返回类型 | cppreference | 0.5h |
| `std::noop_coroutine()` | final_suspend 无等待者时返回"空操作" | cppreference C++20 | 0.5h |

**最小自测**：不查资料，在纸上画出以下代码的执行流程：
```cpp
Task<int> foo() {
    auto val = co_await bar();  // bar 返回 42
    co_return val + 1;          // 最终返回 43
}
// 写出：promise_type 的哪些回调被调用？按什么顺序？
```

#### 1-B：协程帧与生命周期（~3 小时，面试必考）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| 协程帧 = 堆上分配的对象 | 理解为什么 `final_suspend` 必须返回 `suspend_always` | [Lewis Baker: Coroutine Lifetime](https://lewissbaker.github.io/2018/09/05/understanding-the-promise-type/) | 1h |
| 协程帧包含：局部变量 + promise + resume 点 | 理解协程"挂起"时数据存在哪 | 同上 | 1h |
| 悬垂 coroutine_handle 的危险 | Bug #7 的根因——lambda 捕获局部引用 | 自己写一个悬垂示例，用 ASan 检测 | 1h |

#### 1-C：Google Test 基础（~2 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| `TEST` / `EXPECT_EQ` / `ASSERT_TRUE` | 所有模块的单元测试 | [GTest Primer](https://google.github.io/googletest/primer.html) | 1h |
| `TEST_F` + fixture | 共享测试状态（如 IoUring 实例） | 同上 | 0.5h |
| `co_await` 在测试中的用法 | `sync_wait` 包装协程用于测试 | 本项目 `sync_wait.hpp` | 0.5h |

### 🧠 核心概念

```
┌─────────────────────────────────────────────────┐
│              C++20 Coroutine Mechanism           │
│                                                   │
│  co_await expr                                    │
│    └─> await_ready()  ──true──> 直接继续          │
│    └─> await_suspend(handle) ──void──> 挂起       │
│    └─> await_suspend(handle) ──handle──> 切换     │
│    └─> await_resume() ──> 获取返回值              │
│                                                   │
│  promise_type 生命周期:                           │
│    get_return_object() → 创建 Task                │
│    initial_suspend() → 决定是否立即挂起            │
│    yield_value() → co_yield 的处理                │
│    return_value() → co_return 的处理              │
│    unhandled_exception() → 异常处理               │
│    final_suspend() → 决定是否通知等待者            │
└─────────────────────────────────────────────────┘
```

### 📁 需实现的文件

| 文件 | 行数 | 核心内容 |
|------|------|----------|
| `coroutine/task.hpp` | ~200 | Task\<T\>，promise_type，co_await 支持 |
| `coroutine/generator.hpp` | ~120 | Generator\<T\>，懒求值序列 |
| `coroutine/sync_wait.hpp` | ~80 | 同步等待协程完成（测试用） |
| `coroutine/when_all.hpp` | ~150 | 并发等待所有协程 |
| `coroutine/when_any.hpp` | ~120 | 竞争等待任一协程 |
| `coroutine/with_timeout.hpp` | ~100 | 协程超时包装 |
| `coroutine/lazy.hpp` | ~80 | 惰性求值协程 |

### 🔨 实现步骤

#### Step 1.1：Task\<T\> — 最核心的类型（2 天）

```cpp
// include/coroutine/task.hpp — 骨架
namespace rpc::coroutine {

template<typename T = void>
class Task {
public:
    // promise_type 是协程的"灵魂"
    struct promise_type {
        // 1. 创建返回对象
        Task get_return_object() {
            return Task{coroutine_handle::from_promise(*this)};
        }
        // 2. 初始挂起 → 惰性求值
        auto initial_suspend() { return std::suspend_always{}; }
        // 3. 最终挂起 → 通知等待者（关键！）
        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }
                // symmetric transfer：直接返回等待者的 handle
                coroutine_handle await_suspend(coroutine_handle h) noexcept {
                    auto& promise = h.promise();
                    if (promise.continuation_) {
                        return promise.continuation_;
                    }
                    return std::noop_coroutine(); // 无人等待
                }
                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }
        // 4. 返回值存储
        void return_value(T value) { result_ = std::move(value); }
        // 5. 异常存储
        void unhandled_exception() { exception_ = std::current_exception(); }
        // 6. co_await 支持
        auto await_transform(Task&& inner) { /* ... */ }

        std::optional<T> result_;
        std::exception_ptr exception_;
        coroutine_handle continuation_;
    };

    // co_await 操作符
    auto operator co_await() { /* ... */ }

private:
    coroutine_handle handle_;
};

} // namespace rpc::coroutine
```

**关键陷阱**：
- `final_suspend` 必须返回 `suspend_always` 或自定义 awaiter，**绝不能**返回 `suspend_never`，否则协程帧会被提前销毁
- `await_suspend` 返回 `coroutine_handle` 实现对称转移（symmetric transfer），比返回 void 更高效（无栈消耗）
- `promise_type` 的 `await_transform` 是全局拦截点，慎用

#### Step 1.2：Generator\<T\>（1 天）

```cpp
// 懒求值序列：co_yield 逐个产出
template<typename T>
class Generator {
    struct promise_type {
        T current_value_;
        auto yield_value(T value) {
            current_value_ = std::move(value);
            return std::suspend_always{};
        }
        // ...
    };
    class Iterator {
        // operator++ → 恢复协程，取下一个值
    };
};
```

#### Step 1.3：WhenAll / WhenAny（2 天）

```cpp
// WhenAll: 并发启动所有子协程，全部完成后返回
template<typename... Tasks>
Task<std::tuple<typename Tasks::value_type...>> WhenAll(Tasks... tasks);

// WhenAny: 并发启动，任一完成即返回（其余取消）
template<typename... Tasks>
Task<std::variant<...>> WhenAny(Tasks... tasks);
```

**实现要点**：
- WhenAll 需要一个共享计数器，最后一个完成的子协程恢复父协程
- WhenAny 需要一个原子标志，第一个完成的设置标志并恢复父协程，其余检查标志后自行销毁

#### Step 1.4：WithTimeout（1 天）

```cpp
// 超时包装：内部启动定时器协程与工作协程，用 WhenAny 竞争
template<typename T>
Task<T> WithTimeout(Task<T> task, std::chrono::milliseconds timeout);
```

#### Step 1.5：单元测试（2 天）

```cpp
// tests/test_coroutine.cpp
TEST(Coroutine, BasicTask) {
    auto task = []() -> Task<int> { co_return 42; }();
    auto result = sync_wait(task);
    EXPECT_EQ(result, 42);
}

TEST(Coroutine, WhenAll) {
    auto task = []() -> Task<int> {
        auto [a, b, c] = co_await WhenAll(
            async_op(1), async_op(2), async_op(3)
        );
        co_return a + b + c;  // 6
    }();
    EXPECT_EQ(sync_wait(task), 6);
}

TEST(Coroutine, WithTimeout) {
    // 正常完成
    auto ok = sync_wait(WithTimeout(fast_op(), 100ms));
    EXPECT_TRUE(ok.has_value());
    // 超时
    auto timeout = sync_wait(WithTimeout(slow_op(), 1ms));
    EXPECT_TRUE(timeout.is_timeout());
}
```

### ✅ 阶段检查点
- [ ] Task\<T\> 支持 void / 非 void 返回值
- [ ] co_await Task 链式调用正确
- [ ] 异常通过 co_await 正确传播
- [ ] Generator 迭代器 ++ / * 语义正确
- [ ] WhenAll 全部完成时返回所有结果
- [ ] WhenAny 任一完成时立即返回
- [ ] WithTimeout 正常/超时两种路径正确
- [ ] 所有测试通过，ASan 无泄漏

---

## Phase 2：io_uring IO 引擎（第 4~5.5 周）

### 🎯 目标
基于 liburing 实现高性能异步 IO 引擎，封装 accept / recv / send / connect 等操作为协程友好的 API。

### 📖 前置知识

> Phase 1 的协程机制是硬性前提。本阶段需要理解 io_uring 内核接口 + liburing 用户态封装。

#### 2-A：io_uring 内核机制（~6 小时，核心必学）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| SQ（Submission Queue）与 CQ（Completion Queue） | 理解 IO 提交和完成的整个流程 | [Lord of the io_uring: intro](https://unixism.net/loti/ref-liburing/intro.html) | 1h |
| SQE（Submission Queue Entry）与 CQE（Completion Queue Entry） | 每个 IO 操作对应一个 SQE，完成后产生 CQE | 同上 | 1h |
| `io_uring_setup` / `io_uring_enter` / `io_uring_register` | 三个系统调用的作用和参数 | [io_uring 白皮书](https://kernel.dk/io_uring.pdf) 前半部分 | 1h |
| 共享内存环形缓冲区 | SQ/CQ 与内核共享内存，零拷贝的基础 | 同上 | 1h |
| `user_data` 字段 | CQE 的 user_data 关联回 SQE，**协程恢复的关键** | [liburing 源码](https://github.com/axboe/liburing) 看 `io_uring_cqe` 结构体 | 1h |
| 批量提交 vs 逐个提交 | 性能差异 2~3 倍，面试必考 | 自己写对比 benchmark | 1h |

**最小自测**：用 liburing 写一个最简程序——提交一个 NOP 操作，等待 CQE，打印结果（~20 行）。

#### 2-B：liburing C API（~4 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| `io_uring_queue_init` / `io_uring_queue_exit` | IoUring 类的构造/析构 | [liburing man pages](https://manpages.debian.org/unstable/liburing-dev/io_uring_queue_init.3.en.html) | 0.5h |
| `io_uring_get_sqe` + `io_uring_prep_*` | 提交 accept/recv/send 等操作 | 同上，逐个 prep 函数看一遍 | 1h |
| `io_uring_submit` / `io_uring_submit_and_wait` | 提交 SQE 到内核 | 同上 | 0.5h |
| `io_uring_peek_cqe` / `io_uring_wait_cqe` / `io_uring_cqe_seen` | 收割 CQE（批量 vs 阻塞） | 同上 | 1h |
| `IORING_OP_TIMEOUT` / `IORING_OP_LINK_TIMEOUT` | 定时器基于 io_uring timeout 操作 | [io_uring opcode 列表](https://manpages.debian.org/unstable/liburing-dev/io_uring_enter.2.en.html) | 1h |

**最小自测**：用 liburing 写一个文件读取程序（`prep_read` + `submit` + `wait_cqe`），能正确读出文件内容。

#### 2-C：协程与 IO 的桥接（~2 小时，关键设计）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| `user_data` 存储 `coroutine_handle` | CQE 完成时恢复对应协程 | 本项目 `operation.hpp` 的设计 | 1h |
| Awaiter 模式：`await_suspend` 提交 IO | 协程挂起 = 提交 SQE | 回顾 Phase 1 的 Awaiter 三件套 | 0.5h |
| EventLoop 驱动：`peek_cqe` → `resume(handle)` | 主循环收割 CQE 并恢复协程 | 画流程图理解 | 0.5h |

#### 2-D：时间轮算法（~2 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| 哈希时间轮原理 | 定时器实现，O(1) 添加/删除 | [Hashed and Hierarchical Timing Wheels](https://www.cs.columbia.edu/~nahum/w6998/papers/ton97-timing-wheels.pdf) 论文 | 1h |
| 单级 vs 多级时间轮 | 长超时（小时级）需要多级 | 同上 | 0.5h |
| 时间轮 vs 红黑树 vs 最小堆 | 面试对比题 | 自己总结三种方案的优劣 | 0.5h |

### 🧠 核心概念

```
┌──────────────── io_uring 工作模型 ────────────────┐
│                                                     │
│  用户态                    内核态                    │
│  ┌──────┐  SQE  ┌──────┐  ┌──────┐  CQE  ┌──────┐ │
│  │ App  │ ────> │  SQ  │ ──>│Kernel│ ────> │  CQ  │ │
│  │      │ <──── │      │ <──│      │ <──── │      │ │
│  └──────┘       └──────┘   └──────┘       └──────┘ │
│                                                     │
│  核心优势:                                          │
│  1. 批量提交：一次 io_uring_enter 处理多个 IO       │
│  2. 零拷贝：SQ/CQ 与内核共享内存                    │
│  3. 无系统调用开销（SQPOLL 模式下）                 │
│  4. 多路复用替代：一个 fd 不再需要 epoll_wait       │
└─────────────────────────────────────────────────────┘
```

### 📁 需实现的文件

| 文件 | 行数 | 核心内容 |
|------|------|----------|
| `io/io_uring.hpp` | ~450 | IoUring 封装，submit/peek_for_cqe 批量处理 |
| `io/operation.hpp` | ~200 | AcceptOp / RecvOp / SendOp / ConnectOp |
| `io/event_loop.hpp` | ~300 | 事件循环，驱动 io_uring + 定时器 |
| `io/timer.hpp` | ~475 | 定时器（时间轮 + io_uring timeout） |

### 🔨 实现步骤

#### Step 2.1：IoUring 基础封装（2 天）

```cpp
// include/io/io_uring.hpp
namespace rpc::io {

class IoUring {
public:
    explicit IoUring(unsigned entries = 1024, unsigned flags = 0);
    ~IoUring();

    // 提交 SQE
    void prep_accept(int fd, sockaddr* addr, socklen_t* addrlen);
    void prep_recv(int fd, void* buf, size_t len);
    void prep_send(int fd, const void* buf, size_t len);
    void prep_connect(int fd, const sockaddr* addr, socklen_t addrlen);
    void prep_timeout(__kernel_timespec* ts);

    // 提交 & 等待完成
    int submit();
    int submit_and_wait(unsigned wait_nr);

    // 批量收割 CQE（关键优化点！）
    // ⚠️ 不要逐个 io_uring_wait_cqe，用 peek 批量处理
    unsigned peek_for_cqe(std::function<void(io_uring_cqe*)> callback, unsigned max = 128);

    // 获取 SQE（高级用法）
    io_uring_sqe* get_sqe();

private:
    io_uring ring_;
};

} // namespace rpc::io
```

**关键陷阱**：
- CQE 批量收割：`io_uring_peek_cqe` + `io_uring_cqe_seen` 循环，不要用 `io_uring_wait_cqe` 逐个等
- SQE 溢出：`get_sqe()` 可能返回 nullptr（SQ 满了），必须检查或先 submit
- 多线程：默认 io_uring 不是线程安全的，多线程需要 `IORING_SETUP_CLAMP` + per-thread ring

#### Step 2.2：协程化 IO 操作（2 天）

```cpp
// include/io/operation.hpp
namespace rpc::io {

// Accept 操作：挂起协程，等 CQE 回调后恢复
class AcceptAwaiter {
public:
    bool await_ready() { return false; }
    void await_suspend(coroutine_handle h) {
        // 1. 保存 handle 到 user_data
        // 2. prep_accept，设置 user_data = handle 地址
        // 3. submit
    }
    int await_resume() { return accepted_fd_; }
};

// Recv / Send 同理
class RecvAwaiter { /* ... */ };
class SendAwaiter { /* ... */ };
class ConnectAwaiter { /* ... */ };

// 便捷函数
Task<int> async_accept(IoUring& ring, int fd);
Task<size_t> async_recv(IoUring& ring, int fd, void* buf, size_t len);
Task<size_t> async_send(IoUring& ring, int fd, const void* buf, size_t len);
Task<void> async_connect(IoUring& ring, int fd, const sockaddr* addr);

} // namespace rpc::io
```

**CQE → 协程恢复的关键链路**：
```
1. co_await async_recv(ring, fd, buf, len)
2. → RecvAwaiter::await_suspend(handle)  // 保存 handle 到 SQE::user_data
3. → io_uring_submit()                    // 提交到内核
4. → 内核完成 IO，写入 CQE               // CQE::user_data == handle
5. → EventLoop::peek_for_cqe()           // 收割 CQE
6. → 从 CQE::user_data 取出 handle       // 恢复对应协程
7. → handle.resume()                      // 协程继续执行
```

#### Step 2.3：EventLoop（2 天）

```cpp
// include/io/event_loop.hpp
namespace rpc::io {

class EventLoop {
public:
    static EventLoop& instance(); // 单例（Phase 0 修复的关键 bug）

    void run();      // 主循环：submit + peek_cqe + process_timers
    void run_once(); // 单次迭代
    void stop();

    // 注册回调
    void on_cqe(io_uring_cqe* cqe);

private:
    IoUring ring_;
    TimerWheel timers_;
    bool running_{true};

    void process_cqe_batch(); // 批量处理 CQE
    void process_timers();    // 处理到期定时器
};

} // namespace rpc::io
```

#### Step 2.4：定时器 — 时间轮（2 天）

```cpp
// include/io/timer.hpp
namespace rpc::io {

class TimerWheel {
public:
    explicit TimerWheel(size_t wheel_size = 1024, Duration tick = 1ms);

    // 添加定时器，返回 TimerId
    TimerId add_timer(Duration delay, Callback cb);

    // 取消定时器
    void cancel(TimerId id);

    // 推进时间轮（每次 EventLoop 迭代调用）
    void tick();

private:
    std::vector<std::list<TimerEntry>> wheel_; // 环形数组
    size_t current_slot_{0};
    Duration tick_duration_;
    // 多级时间轮：秒级 → 分钟级 → 小时级
};

} // namespace rpc::io
```

**关键陷阱**：
- 时间轮 off-by-one：`slot = (current + delay / tick) % size`，注意边界
- 定时器取消后 CQE 仍可能到达（内核已提交），需在回调中检查 TimerId 是否已取消
- io_uring timeout 的 `IORING_OP_TIMEOUT` 可与时间轮配合使用

#### Step 2.5：TCP Echo Benchmark（1 天）

```cpp
// benchmarks/echo_benchmark.cpp
// 验证 io_uring + 协程的基本性能
Task<void> echo_handler(int fd) {
    char buf[4096];
    while (true) {
        auto n = co_await async_recv(ring, fd, buf, sizeof(buf));
        if (n <= 0) break;
        co_await async_send(ring, fd, buf, n);
    }
}
// 目标：单线程 QPS > 10 万
```

### ✅ 阶段检查点
- [ ] IoUring 封装编译通过，基本 submit/cqe 正确
- [ ] Accept/Recv/Send/Connect awaiter 正确挂起和恢复
- [ ] EventLoop 主循环可运行，CQE 批量处理正确
- [ ] 时间轮添加/取消/tick 正确
- [ ] TCP echo 服务器可启动，客户端可连接
- [ ] Echo benchmark 单线程 QPS > 5 万（VM 环境下）

---

## Phase 3：网络层（第 5.5~7 周）

### 🎯 目标
在 IO 引擎之上构建网络抽象层，包括 Buffer、TcpServer/TcpClient、Connection 管理。

### 📖 前置知识

> Phase 2 的 IoUring + 协程化 IO 操作是硬性前提。本阶段需要理解 TCP 协议细节 + 网络编程模式。

#### 3-A：TCP 协议与 socket 编程（~6 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| TCP 三次握手 / 四次挥手 | 理解 connect/accept/close 的内核行为 | 《TCP/IP 详解 卷1》第 18 章 | 1h |
| TCP 状态机（CLOSED → SYN_SENT → ESTABLISHED → ...） | 排查连接问题，面试必考 | 同上，画状态机图 | 1h |
| `setsockopt`：SO_REUSEADDR / SO_REUSEPORT / TCP_NODELAY | 服务器必设选项 | [Beej's Guide: setsockopt](https://beej.us/guide/bgnet/html/#setsockoptmanually) | 1h |
| 非阻塞 fd + `fcntl(fd, F_SETFL, O_NONBLOCK)` | io_uring 操作的 fd 应设非阻塞 | [Beej's Guide: Non-blocking](https://beej.us/guide/bgnet/html/#blocking) | 0.5h |
| `recv` 返回 0 = 对端关闭，返回 -1 + EAGAIN = 暂无数据 | 处理连接生命周期 | 实践中理解 | 0.5h |
| Nagle 算法与 TCP_NODELAY | RPC 框架需要禁用 Nagle（延迟敏感） | [TCP_NODELAY 解释](https://developerweb.net/?q=TCP_NODELAY) | 0.5h |
| TIME_WAIT 状态与 SO_REUSEADDR | 服务器重启后端口复用 | 《TCP/IP 详解 卷1》第 18.6 节 | 0.5h |
| 粘包/半包问题 | RPC 协议需要正确处理消息边界 | 自己写代码复现粘包 | 1h |

**最小自测**：能解释以下问题：
- 为什么服务器需要 `SO_REUSEADDR`？
- `recv` 返回 0 和返回 -1 + EAGAIN 有什么区别？
- 什么是粘包？RPC 协议怎么解决？

#### 3-B：Reactor / Proactor 模式（~2 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| Reactor 模式（epoll + 回调） | 理解传统事件驱动模型 | [POSA2 第 7 章](https://www.dre.vanderbilt.edu/~schmidt/PDF/POSA.pdf) 或 [muduo 文档](https://github.com/chenshuo/muduo) | 1h |
| Proactor 模式（io_uring + 协程） | 本项目采用的模式——IO 完成后回调 | 对比 Reactor 理解 | 0.5h |
| 本项目的"协程 Proactor"：EventLoop → CQE → resume | 桥接 Phase 2 的设计 | 画时序图 | 0.5h |

#### 3-C：Buffer 设计模式（~2 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| 固定大小块 vs 动态扩容 | 链式 Buffer 的设计动机 | muduo `Buffer` 源码（连续内存方案） | 0.5h |
| 链表 vs 环形缓冲区 vs 双缓冲 | 三种 Buffer 方案的取舍 | 自己对比分析 | 0.5h |
| 零拷贝：`writev` / `sendmsg` | 减少用户态拷贝 | [scatter-gather IO](https://man7.org/linux/man-pages/man2/writev.2.html) | 0.5h |
| `std::string_view` 的生命周期陷阱 | Bug 常见来源——悬垂 view | 自己写一个悬垂示例 | 0.5h |

#### 3-D：连接池与负载均衡（~2 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| 连接池模式：acquire → use → release | RPC 客户端复用长连接 | [连接池设计](https://medium.com/@mustafa.khan/connection-pool-design-5bb8c9df6b47) | 0.5h |
| 负载均衡策略：轮询 / 随机 / 加权 / 一致性哈希 | 多后端选择策略 | [负载均衡算法对比](https://samritha.medium.com/load-balancing-algorithms-6c5b394e7b9c) | 1h |
| 长连接 vs 短连接的取舍 | RPC 框架设计决策 | 自己总结两种方案的适用场景 | 0.5h |

### 📁 需实现的文件

| 文件 | 行数 | 核心内容 |
|------|------|----------|
| `net/buffer.hpp` | ~200 | 链式 Buffer（BufferBlock 链表） |
| `net/tcp_server.hpp` | ~150 | TCP 服务器，accept 循环 + 协程分发 |
| `net/tcp_client.hpp` | ~100 | TCP 客户端，connect + 发送/接收 |
| `net/connection.hpp` | ~200 | 连接抽象，读写缓冲 + 状态管理 |
| `net/connection_pool.hpp` | ~150 | 连接池，复用长连接 |
| `net/load_balancer.hpp` | ~120 | 负载均衡（轮询 / 随机 / 加权） |

### 🔨 实现步骤

#### Step 3.1：链式 Buffer（2 天）

```cpp
// include/net/buffer.hpp
namespace rpc::net {

// BufferBlock: 固定大小块（默认 4KB）
struct BufferBlock {
    static constexpr size_t BLOCK_SIZE = 4096;
    char data[BLOCK_SIZE];
    size_t read_pos{0};
    size_t write_pos{0};
    std::unique_ptr<BufferBlock> next; // 链表

    size_t readable() const { return write_pos - read_pos; }
    size_t writable() const { return BLOCK_SIZE - write_pos; }
};

// Buffer: 链式缓冲区
class Buffer {
public:
    size_t read(void* dst, size_t len);     // 读取数据
    size_t write(const void* src, size_t len); // 写入数据
    void compact();                          // 压缩已读空间
    std::string_view peek(size_t len);       // 零拷贝窥视

private:
    std::unique_ptr<BufferBlock> head_;
    BufferBlock* tail_;
    size_t total_readable_{0};
};

} // namespace rpc::net
```

**关键陷阱**：
- BufferBlock 析构必须递归释放链表（`next` unique_ptr 自动处理，但注意栈溢出风险——超长链表改用循环释放）
- `peek` 返回的 `string_view` 生命周期受 Buffer 控制，不能跨操作持有
- 零拷贝 send：直接从 BufferBlock 的 `data + read_pos` 发送，避免额外拷贝

#### Step 3.2：TcpServer（2 天）

```cpp
// include/net/tcp_server.hpp
namespace rpc::net {

class TcpServer {
public:
    using Handler = std::function<Task<void>(Connection&)>;

    TcpServer(IoUring& ring, std::string ip, uint16_t port);
    void start(Handler handler);
    void stop();

private:
    IoUring& ring_;
    int listen_fd_;
    Handler handler_;

    // accept 循环：协程化
    Task<void> accept_loop() {
        while (running_) {
            int fd = co_await async_accept(ring_, listen_fd_);
            if (fd < 0) continue;
            // 为每个连接启动独立协程
            auto conn = std::make_shared<Connection>(ring_, fd);
            spawn(handler_(*conn)); // fire-and-forget 协程
        }
    }
};

} // namespace rpc::net
```

#### Step 3.3：Connection + ConnectionPool + LoadBalancer（2 天）

```cpp
// 连接状态
enum class ConnState { Idle, Busy, Closed };

class Connection {
    IoUring& ring_;
    int fd_;
    Buffer read_buf_;
    Buffer write_buf_;
    ConnState state_{ConnState::Idle};
    // ...
};

// 连接池：复用空闲连接
class ConnectionPool {
public:
    std::shared_ptr<Connection> acquire(const std::string& addr);
    void release(std::shared_ptr<Connection> conn);
    // ...
};

// 负载均衡策略
class LoadBalancer {
public:
    enum Strategy { RoundRobin, Random, Weighted };
    std::string next_server(); // 选下一个后端
    // ...
};
```

#### Step 3.4：TCP Echo 端到端测试（1 天）

```cpp
// tests/test_tcp_echo.cpp
TEST(TcpEcho, EndToEnd) {
    // 1. 启动 echo server
    TcpServer server(ring, "127.0.0.1", 0);
    server.start([](Connection& conn) -> Task<void> {
        char buf[1024];
        while (true) {
            auto n = co_await conn.recv(buf, sizeof(buf));
            if (n <= 0) co_return;
            co_await conn.send(buf, n);
        }
    });

    // 2. 客户端连接发送
    TcpClient client(ring);
    co_await client.connect("127.0.0.1", server.port());
    co_await client.send("hello", 5);
    char buf[1024];
    auto n = co_await client.recv(buf, sizeof(buf));
    EXPECT_EQ(std::string_view(buf, n), "hello");
}
```

### ✅ 阶段检查点
- [ ] Buffer 读写正确，链式扩展正确
- [ ] Buffer 零拷贝 peek/send 正确
- [ ] TcpServer accept 循环正常，多客户端可同时连接
- [ ] Connection 状态转换正确（Idle → Busy → Idle/Closed）
- [ ] ConnectionPool acquire/release 正确
- [ ] LoadBalancer 三种策略正确
- [ ] TCP Echo 端到端测试通过

---

## Phase 4：RPC 框架（第 7~8.5 周）

### 🎯 目标
构建完整的 RPC 框架，包括序列化、协议编解码、服务注册/发现、Stub 生成，实现端到端 RPC 调用链。

### 📖 前置知识

> Phase 3 的网络层（TcpServer/Client/Connection）是硬性前提。本阶段需要理解 RPC 原理 + 序列化 + 协议设计。

#### 4-A：RPC 基本原理（~4 小时，核心必学）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| RPC 是什么：远程过程调用 = 本地调用 + 网络传输 + 序列化 | 理解整个框架的目标 | [RPC 原理详解](https://waylau.com/remoting-procedure-call/) 或 Google "RPC 原理" | 1h |
| RPC 调用链：Client → 序列化 → 网络 → 反序列化 → Server → 执行 → 返回 | 本阶段的核心实现路径 | 画完整调用链时序图 | 1h |
| Stub / Proxy / Skeleton 的概念 | RPC 客户端代理 + 服务端骨架 | [gRPC 概念](https://grpc.io/docs/what-is-grpc/core-concepts/) | 1h |
| 同步 RPC vs 异步 RPC vs 协程 RPC | 本项目是协程 RPC，理解三种模型的区别 | 自己写三种版本的对比代码 | 1h |

**最小自测**：能解释以下问题：
- RPC 和 HTTP API 的区别？
- 为什么需要序列化？直接传结构体行不行？
- Stub 的作用是什么？

#### 4-B：序列化与编码（~4 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| 序列化 vs 编码 vs 编解码 | 术语区分 | [Protocol Buffers 编码](https://protobuf.dev/programming-guides/encoding/) | 0.5h |
| 二进制序列化 vs 文本序列化（JSON/XML） | 设计决策——本项目选二进制 | 对比两种方案的优劣 | 0.5h |
| VarInt（变长整数）编码 | Protobuf 也用 VarInt，本项目自研 | [VarInt 编码原理](https://protobuf.dev/programming-guides/encoding/#varints) **必读** | 1h |
| 字节序（大端 vs 小端） | 网络传输需要统一字节序 | [endianness](https://en.cppreference.com/w/cpp/language/endian) | 0.5h |
| 定长 vs 变长字段 | 协议设计：header 定长 + body 变长 | 自己设计一个简单协议 | 1h |
| 类型擦除与 `std::any` / `void*` | ServiceRegistry 存储不同签名的 handler | [std::any](https://en.cppreference.com/w/cpp/utility/any) | 0.5h |

**最小自测**：手动实现 VarInt 编解码：
```cpp
// 编码：7 位一组，最高位为 continuation bit
void write_varint(uint64_t val, std::vector<uint8_t>& buf) {
    while (val >= 0x80) {
        buf.push_back((val & 0x7F) | 0x80);
        val >>= 7;
    }
    buf.push_back(val);
}
// 解码：读字节直到最高位为 0
uint64_t read_varint(/* ... */);
// 验证：write_varint(300) → [0xAC, 0x02]，read_varint → 300
```

#### 4-C：协议设计（~3 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| 魔数（Magic Number） | 协议识别 + 防误读 | 看几个开源 RPC 框架的协议头 | 0.5h |
| 请求/响应匹配：request_id | 异步 RPC 需要匹配请求和响应 | [HTTP/2 stream ID](https://httpwg.org/specs/rfc9113.html#StreamIdentifiers) 类比 | 0.5h |
| 错误码设计 | RpcResult 的 error_code 体系 | [gRPC status codes](https://grpc.github.io/grpc/core/md_2include_2grpcpp_2status_8h.html) 参考 | 0.5h |
| 心跳 / 保活机制 | 长连接需要检测对端存活 | 自己设计心跳协议 | 0.5h |
| 粘包处理：长度前缀法 | 每个消息前加 body_length | 本项目 Protocol 的设计 | 1h |

#### 4-D：服务注册与发现（~2 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| 本地注册表：`unordered_map<string, handler>` | 本项目的简化实现 | 直接实现 | 0.5h |
| 分布式注册中心：ZooKeeper / etcd / Consul | 面试扩展——如果做分布式版本 | [服务注册发现原理](https://s2.51cto.com/images/100/064/064.png) 或 Google | 1h |
| 服务发现模式：客户端发现 vs 服务端发现 | 面试对比题 | 同上 | 0.5h |

### 🧠 架构全景

```
┌─────────────────── RPC 调用链 ───────────────────┐
│                                                     │
│  Client                         Server              │
│  ┌──────────┐                   ┌──────────┐       │
│  │ Proxy    │ ──serialize──>    │ Stub     │       │
│  │ (用户侧) │                   │ (服务侧) │       │
│  └────┬─────┘                   └────┬─────┘       │
│       │                              │             │
│  ┌────▼─────┐                   ┌────▼─────┐       │
│  │ Serializer│                   │Serializer│       │
│  └────┬─────┘                   └────┬─────┘       │
│       │                              │             │
│  ┌────▼─────┐    TCP/网络     ┌────▼─────┐       │
│  │ Protocol │ ──────────────> │ Protocol │       │
│  │ 编解码   │    Request      │ 编解码   │       │
│  └────┬─────┘                 └────┬─────┘       │
│       │ <────────────────────       │             │
│       │        Response             │             │
│  ┌────▼─────┐                 ┌────▼─────┐       │
│  │ Transport│                 │Transport │       │
│  └──────────┘                 └──────────┘       │
│                                                     │
│  协议格式:                                          │
│  ┌──────┬──────┬──────┬──────┬──────┬──────┐      │
│  │ magic│ ver  │ type │ req_id│ len  │ body │      │
│  │ 4B   │ 1B   │ 1B   │ 4B   │ 4B   │ len  │      │
│  └──────┴──────┴──────┴──────┴──────┴──────┘      │
└─────────────────────────────────────────────────────┘
```

### 📁 需实现的文件

| 文件 | 行数 | 核心内容 |
|------|------|----------|
| `rpc/serializer.hpp` | ~200 | 二进制序列化（变长整数 + 类型推导） |
| `rpc/protocol.hpp` | ~150 | 消息头编解码 |
| `rpc/rpc_result.hpp` | ~80 | RpcResult\<T\> 统一返回 |
| `rpc/service_registry.hpp` | ~120 | 服务注册表（方法名 → handler） |
| `rpc/rpc_server.hpp` | ~150 | RPC 服务器（接收请求 → 查表 → 调用 → 返回） |
| `rpc/rpc_client.hpp` | ~150 | RPC 客户端（发送请求 → 等待响应） |
| `rpc/rpc_stub.hpp` | ~100 | 代理生成（类型安全的远程调用接口） |

### 🔨 实现步骤

#### Step 4.1：序列化器（2 天）

```cpp
// include/rpc/serializer.hpp
namespace rpc::rpc {

class Serializer {
public:
    // 基本类型
    void write_int32(int32_t val);
    void write_int64(int64_t val);
    void write_string(std::string_view val);
    void write_float(float val);
    void write_double(double val);

    // 变长整数（VarInt）—— 小值省空间
    void write_varint(uint64_t val);
    uint64_t read_varint();

    // 容器
    template<typename T>
    void write_vector(const std::vector<T>& vec);
    template<typename K, typename V>
    void write_map(const std::unordered_map<K, V>& map);

    // 反序列化
    int32_t read_int32();
    std::string read_string();
    // ...

private:
    Buffer buffer_;
};

} // namespace rpc::rpc
```

**设计决策**：
- 使用二进制协议而非 JSON/Protobuf：性能优先，面试加分
- VarInt 编码：小整数 1 字节，大整数最多 10 字节，平均节省 30%+ 空间
- 字段顺序编码（无 tag），依赖编译期类型信息——比 Protobuf 简单但不够自描述

#### Step 4.2：协议编解码（1 天）

```cpp
// include/rpc/protocol.hpp
namespace rpc::rpc {

struct MessageHeader {
    static constexpr uint32_t MAGIC = 0xRPC1;
    uint32_t magic{MAGIC};
    uint8_t version{1};
    uint8_t type;       // Request / Response / Heartbeat
    uint32_t request_id;
    uint32_t body_length;
};

struct RpcRequest {
    MessageHeader header;
    std::string service_name;
    std::string method_name;
    std::vector<uint8_t> args;  // 序列化后的参数
};

struct RpcResponse {
    MessageHeader header;
    int32_t error_code{0};      // 0 = success
    std::vector<uint8_t> result; // 序列化后的返回值
};

// 编解码
std::vector<uint8_t> encode(const RpcRequest& req);
std::vector<uint8_t> encode(const RpcResponse& resp);
RpcRequest decode_request(const uint8_t* data, size_t len);
RpcResponse decode_response(const uint8_t* data, size_t len);

} // namespace rpc::rpc
```

#### Step 4.3：RpcResult\<T\>（0.5 天）

```cpp
// include/rpc/rpc_result.hpp
namespace rpc::rpc {

template<typename T>
class RpcResult {
public:
    // 成功
    static RpcResult ok(T value) { return {0, std::move(value)}; }
    // 失败
    static RpcResult error(int32_t code, std::string msg = "") {
        return {code, std::nullopt, std::move(msg)};
    }

    bool ok() const { return error_code_ == 0; }
    const T& value() const { return value_.value(); }
    int32_t error_code() const { return error_code_; }
    const std::string& error_message() const { return error_msg_; }

private:
    int32_t error_code_{0};
    std::optional<T> value_;
    std::string error_msg_;
};

} // namespace rpc::rpc
```

#### Step 4.4：服务注册表 + RPC Server（2 天）

```cpp
// include/rpc/service_registry.hpp
namespace rpc::rpc {

class ServiceRegistry {
public:
    // 注册方法：service.method → handler
    template<typename Req, typename Resp>
    void register_method(const std::string& service,
                         const std::string& method,
                         std::function<Task<Resp>(Req)> handler);

    // 查找方法
    std::optional<MethodHandler> lookup(const std::string& service,
                                         const std::string& method);

private:
    // key = "service.method"
    std::unordered_map<std::string, MethodHandler> methods_;
};

} // namespace rpc::rpc

// include/rpc/rpc_server.hpp
namespace rpc::rpc {

class RpcServer {
public:
    RpcServer(IoUring& ring, std::string ip, uint16_t port);

    // 注册服务
    template<typename Req, typename Resp>
    void register_method(const std::string& service,
                         const std::string& method,
                         std::function<Task<Resp>(Req)> handler);

    // 启动服务
    Task<void> start();

private:
    TcpServer server_;
    ServiceRegistry registry_;

    // 处理单个连接
    Task<void> handle_connection(Connection& conn);

    // 处理单个请求
    Task<void> handle_request(RpcRequest req, Connection& conn);
};

} // namespace rpc::rpc
```

#### Step 4.5：RPC Client + Stub（2 天）

```cpp
// include/rpc/rpc_client.hpp
namespace rpc::rpc {

class RpcClient {
public:
    RpcClient(IoUring& ring);

    // 连接服务器
    Task<void> connect(const std::string& ip, uint16_t port);

    // 通用调用
    template<typename Req, typename Resp>
    Task<RpcResult<Resp>> call(const std::string& service,
                                const std::string& method,
                                const Req& request);

private:
    ConnectionPool pool_;
    std::atomic<uint32_t> next_request_id_{0};
};

} // namespace rpc::rpc

// include/rpc/rpc_stub.hpp
// 代理类：编译期类型安全的远程调用
namespace rpc::rpc {

// 示例：用户服务代理
class UserServiceStub {
public:
    explicit UserServiceStub(RpcClient& client)
        : client_(client) {}

    Task<RpcResult<UserInfo>> get_user(int64_t user_id) {
        GetUserRequest req{.user_id = user_id};
        co_return co_await client_.call<GetUserRequest, UserInfo>(
            "UserService", "get_user", req
        );
    }

private:
    RpcClient& client_;
};

} // namespace rpc::rpc
```

#### Step 4.6：端到端 RPC 测试（1 天）

```cpp
// tests/test_rpc_e2e.cpp
struct AddRequest { int a, b; };
struct AddResponse { int result; };

TEST(RpcE2E, BasicCall) {
    // Server
    RpcServer server(ring, "127.0.0.1", 0);
    server.register_method<AddRequest, AddResponse>(
        "CalcService", "add",
        [](AddRequest req) -> Task<AddResponse> {
            co_return AddResponse{req.a + req.b};
        }
    );
    // 启动 server（后台协程）

    // Client
    RpcClient client(ring);
    co_await client.connect("127.0.0.1", server.port());

    auto result = co_await client.call<AddRequest, AddResponse>(
        "CalcService", "add", AddRequest{3, 4}
    );
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value().result, 7);
}
```

### ✅ 阶段检查点
- [ ] Serializer 基本类型 + 容器类型正确
- [ ] VarInt 编解码一致
- [ ] Protocol 编解码往返一致
- [ ] ServiceRegistry 注册/查找正确
- [ ] RpcServer 可启动，接收请求并返回响应
- [ ] RpcClient 可连接，发送请求并接收响应
- [ ] 端到端 RPC 调用：请求 → 序列化 → 网络 → 反序列化 → 调用 → 返回
- [ ] RpcResult ok/error 两种路径正确

---

## Phase 5：IDL + 构建系统 + 全量测试（第 8.5~10 周）

### 🎯 目标
实现 IDL 解析器（接口定义语言），完善 CMake 构建系统，补全所有单元测试和 Benchmark。

### 📖 前置知识

> Phase 4 的 RPC 框架是硬性前提。本阶段需要理解编译原理基础 + CMake 高级用法 + 测试方法论。

#### 5-A：编译原理基础（~4 小时，IDL 解析器需要）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| 词法分析（Tokenization） | IDL 文本 → Token 流 | 《编译原理》第 2 章 或 [Crafting Interpreters: Scanning](http://craftinginterpreters.com/scanning.html) | 1h |
| 语法分析（Parsing） | Token 流 → AST | [递归下降解析器](http://craftinginterpreters.com/parsing-expressions.html) **推荐** | 1h |
| AST（抽象语法树） | IDL 的中间表示 | 同上 | 0.5h |
| 递归下降 vs LR vs LL | 选择解析策略——本项目用递归下降（最简单） | [Parsing 策略对比](https://eli.thegreenplace.net/2012/08/02/parsing-expressions-by-recursive-descent) | 0.5h |
| 代码生成（Codegen） | AST → C++ Stub 代码 | 理解模板字符串拼接即可 | 1h |

**最小自测**：写一个简单的算术表达式解析器（支持 `+` `-` `*` `/` 和括号），用递归下降实现（~80 行）。

#### 5-B：IDL 与 Protobuf（~2 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| Protobuf IDL 语法 | 本项目的 IDL 借鉴 Protobuf 风格 | [Protobuf Language Guide](https://protobuf.dev/programming-guides/proto3/) | 1h |
| `service` / `rpc` / `message` 关键字 | IDL 的核心概念 | 同上 | 0.5h |
| `protoc` 代码生成流程 | 理解 IDL → 代码的完整链路 | 自己用 protoc 生成一次代码 | 0.5h |

#### 5-C：CMake 高级用法（~2 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| `FetchContent` 自动拉取依赖 | Google Test / benchmark | [CMake FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html) | 0.5h |
| `option` + generator expression | ASan/UBSan/TSan 编译选项 | [CMake option](https://cmake.org/cmake/help/latest/command/option.html) | 0.5h |
| `install` + `find_package`（可选） | 项目可被其他项目引用 | [CMake install](https://cmake.org/cmake/help/latest/command/install.html) | 0.5h |
| `ctest` 测试驱动 | 运行所有测试 | [CTest](https://cmake.org/cmake/help/latest/manual/ctest.1.html) | 0.5h |

#### 5-D：测试方法论（~2 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| 单元测试 vs 集成测试 vs 端到端测试 | 三层测试策略 | [Google Testing Blog](https://testing.googleblog.com/) | 0.5h |
| 边界值 + 异常路径测试 | Buffer 边界、序列化溢出、连接断开 | 实践中学习 | 0.5h |
| ASan / UBSan / TSan 原理 | 内存安全保证 | [ASan 详解](https://github.com/google/sanitizers/wiki/AddressSanitizer) | 0.5h |
| Benchmark 设计：避免优化掉结果 | `benchmark::DoNotOptimize` | [Google Benchmark](https://github.com/google/benchmark) | 0.5h |

### 📁 需实现的文件

| 文件 | 行数 | 核心内容 |
|------|------|----------|
| `idl/parser.hpp` | ~245 | IDL 文法解析（service / method / field） |
| `idl/codegen.hpp` | ~150 | 从 AST 生成 C++ Stub 代码 |
| `CMakeLists.txt` | ~200 | 顶层 + 子目录，FetchContent GTest |
| `tests/CMakeLists.txt` | ~50 | 测试目标注册 |
| `benchmarks/CMakeLists.txt` | ~30 | Benchmark 目标注册 |

### 🔨 实现步骤

#### Step 5.1：IDL 解析器（2 天）

```proto
// IDL 示例（类 Protobuf 语法）
service UserService {
    rpc get_user(GetUserRequest) returns (UserInfo);
    rpc create_user(CreateUserRequest) returns (UserInfo);
}

message GetUserRequest {
    int64 user_id = 1;
}

message UserInfo {
    int64 id = 1;
    string name = 2;
    int32 age = 3;
}
```

```cpp
// include/idl/parser.hpp
namespace rpc::idl {

struct Field { std::string name; std::string type; int number; };
struct Method { std::string name; std::string req_type; std::string resp_type; };
struct Service { std::string name; std::vector<Method> methods; };
struct Message { std::string name; std::vector<Field> fields; };

struct IDLFile {
    std::vector<Service> services;
    std::vector<Message> messages;
};

class Parser {
public:
    IDLFile parse(std::string_view source);
private:
    // 递归下降解析
    Service parse_service();
    Method parse_method();
    Message parse_message();
    Field parse_field();
};

} // namespace rpc::idl
```

#### Step 5.2：代码生成器（1 天）

```cpp
// include/idl/codegen.hpp
namespace rpc::idl {

class CodeGenerator {
public:
    // 从 IDL 生成 C++ Stub 代码
    std::string generate(const IDLFile& idl);

private:
    std::string gen_message_struct(const Message& msg);
    std::string gen_service_stub(const Service& svc);
    std::string gen_serializer(const Message& msg);
};

} // namespace rpc::idl
```

#### Step 5.3：CMake 构建系统（1 天）

```cmake
# CMakeLists.txt（顶层）
cmake_minimum_required(VERSION 3.20)
project(rpc_framework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译选项
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UBSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# 依赖：liburing
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED liburing)

# 依赖：Google Test（FetchContent）
include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(benchmarks)
```

#### Step 5.4：补全单元测试（2 天）

需要覆盖的测试套件：

| 套件 | 用例数 | 覆盖模块 |
|------|--------|----------|
| test_buffer | 6 | Buffer 读写、链式扩展、零拷贝 |
| test_serializer | 5 | 基本类型、容器、VarInt |
| test_coroutine | 8 | Task、Generator、WhenAll、WhenAny、WithTimeout |
| test_io_uring | 4 | submit/cqe、accept/recv/send |
| test_timer | 3 | 添加/取消/tick |
| test_tcp | 3 | echo 端到端 |
| test_rpc | 5 | 注册/查找/调用/错误处理 |
| test_connection_pool | 4 | acquire/release/超时 |
| test_load_balancer | 3 | 轮询/随机/加权 |

#### Step 5.5：Benchmark 套件（1 天）

```cpp
// benchmarks/bench_coroutine.cpp — 协程调度 QPS
// benchmarks/bench_serializer.cpp — 序列化 QPS
// benchmarks/bench_buffer.cpp — Buffer 读写 QPS
// benchmarks/bench_rpc_lookup.cpp — RPC 方法查找 QPS
```

**参考数据**（本项目实测）：
- 协程调度：~40M QPS
- 序列化：~525M QPS
- Buffer 读写：~2.5B QPS
- RPC 查找：~64M QPS

### ✅ 阶段检查点
- [ ] IDL 解析器可解析 service + message 定义
- [ ] 代码生成器可输出 C++ Stub 代码
- [ ] CMake 全量编译通过（Debug + Release + ASan）
- [ ] 所有 9 个测试套件通过（38 个用例）
- [ ] ASan + UBSan 零内存泄漏
- [ ] 4 个 Benchmark 可运行并输出 QPS

---

## Phase 6：性能优化 & 文档 & 面试准备（第 10~11.5 周）

### 🎯 目标
优化关键路径性能，撰写项目文档，整理面试知识点，达到秋招可用水平。

### 📖 前置知识

> Phase 5 的全量测试是硬性前提。本阶段需要理解性能分析工具 + io_uring 高级特性 + 面试表达。

#### 6-A：性能分析与 Benchmark（~4 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| `perf` + `perf record` / `perf report` | 找热点函数 | [perf 教程](https://perf.wiki.kernel.org/index.php/Tutorial) | 1h |
| `flamegraph` 火焰图 | 可视化性能瓶颈 | [FlameGraph](https://github.com/brendangregg/FlameGraph) | 0.5h |
| `strace` 系统调用追踪 | 确认 io_uring 确实减少了 syscall | `strace -c ./your_program` | 0.5h |
| Google Benchmark 框架 | 写规范的 benchmark | [Google Benchmark](https://github.com/google/benchmark) 快速入门 | 1h |
| QPS / 延迟 / P99 的含义 | 性能指标定义 | [Latency Numbers Every Programmer Should Know](https://gist.github.com/jboner/884db79141dba2e15e6e) | 0.5h |
| 微 benchmark 的陷阱（缓存/分支预测/编译器优化） | 避免测出假数据 | [Benchmark 陷阱](https://google.github.io/benchmark/benchmark_overview.html#preventing-optimization) | 0.5h |

**最小自测**：用 `perf record` + `flamegraph` 分析你的 TCP echo 服务器，找到热点函数。

#### 6-B：io_uring 高级特性（~3 小时，P1 优化需要）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| `register_files`：固定文件描述符表 | 减少 fd 查找开销 | [io_uring register_files](https://manpages.debian.org/unstable/liburing-dev/io_uring_register_files.3.en.html) | 1h |
| `register_buffers`：固定缓冲区 | 零拷贝 IO | [io_uring register_buffers](https://manpages.debian.org/unstable/liburing-dev/io_uring_register_buffers.3.en.html) | 0.5h |
| SQPOLL：内核轮询线程 | 省掉 `io_uring_enter` 系统调用 | [io_uring SQPOLL](https://unixism.net/loti/ref-liburing/sqpoll.html) | 1h |
| `IORING_SETUP_COOP_TASKRUN` | 内核合作调度，减少上下文切换 | Linux 5.19+ changelog | 0.5h |

#### 6-C：多线程与并发（~3 小时，EventLoopPool 需要）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| `std::thread` + `std::atomic` | 多线程 EventLoop | 《C++ Concurrency in Action》第 2~5 章 | 1h |
| per-thread io_uring 实例 | io_uring 非线程安全，每个线程独立 ring | [io_uring 多线程](https://unixism.net/loti/ref-liburing/multithread.html) | 1h |
| 线程间通信：`eventfd` 或 pipe | 主线程通知 IO 线程有新任务 | [eventfd](https://man7.org/linux/man-pages/man2/eventfd.2.html) | 0.5h |
| CPU 亲和性：`pthread_setaffinity_np` | 减少 CPU 迁移 | [CPU affinity](https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html) | 0.5h |

#### 6-D：技术写作与面试表达（~2 小时）

| 知识点 | 为什么需要 | 学习方式 | 预计时间 |
|--------|-----------|---------|---------|
| README 结构：Badges + Quick Start + Benchmark + Test | 项目门面 | 看优秀开源项目的 README（brpc、muduo） | 0.5h |
| 架构文档：模块图 + 数据流 + 设计决策 | 面试时展示系统设计能力 | [C4 Model](https://c4model.com/) 轻量版 | 0.5h |
| 面试项目介绍：1 分钟 / 3 分钟 / 5 分钟版本 | 不同面试场景 | 自己写 + 模拟练习 | 0.5h |
| STAR 法则：Situation → Task → Action → Result | 结构化表达 | [STAR 面试法](https://www.themuse.com/advice/interview-interview-what-is-the-star-method) | 0.5h |

### 🔨 实现步骤

#### Step 6.1：性能优化（2 天）

**优化清单**（按收益排序）：

| 优化项 | 预期收益 | 难度 |
|--------|----------|------|
| CQE 批量收割（一次 peek 多个） | 2~3x QPS | 低 |
| `IORING_SETUP_COOP_TASKRUN` 内核合作调度 | 减少上下文切换 | 中 |
| `register_files` 固定文件描述符表 | 减少 fd 查找开销 | 中 |
| `register_buffers` 固定缓冲区 | 零拷贝 IO | 高 |
| SQPOLL 内核轮询线程 | 省系统调用 | 高 |
| 多线程 EventLoop（per-thread ring） | 多核扩展 | 中 |
| 热路径 `__builtin_expect` 分支预测 | 微优化 | 低 |
| 内存池替代 malloc | 减少内存分配开销 | 中 |

**benchmark 驱动优化**：每次只改一个变量，跑 benchmark 对比。

#### Step 6.2：项目文档（2 天）

| 文档 | 内容 |
|------|------|
| `README.md` | 项目简介 + Badge + 快速开始 + Benchmark + 测试汇总 |
| `ARCHITECTURE.md` | 架构图 + 模块关系 + 关键设计决策 |
| `PERFORMANCE_REPORT.md` | Benchmark 数据 + 优化记录 + 环境说明 |
| `INTERVIEW_QA.md` | 40+ 面试问答 + 追问速查表 |

**README Badge 示例**：
```markdown
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![io_uring](https://img.shields.io/badge/IO-io__uring-green)
![Tests](https://img.shields.io/badge/Tests-38%20passed-success)
![ASan](https://img.shields.io/badge/ASan-0%20leaks-success)
![License](https://img.shields.io/badge/License-MIT-yellow)
```

#### Step 6.3：面试知识整理（2 天）

**核心面试题方向**：

1. **协程底层**（必考）
   - `promise_type` 生命周期？协程帧分配在哪？
   - `co_await` 的展开过程？`await_ready/suspend/resume` 三个回调的调用时机？
   - symmetric transfer vs asymmetric transfer？
   - 协程与线程的区别？协程的调度开销？

2. **io_uring**（高频）
   - io_uring vs epoll 的本质区别？
   - SQ/CQ 的共享内存机制？
   - SQPOLL 模式的原理和适用场景？
   - io_uring 的零拷贝是怎么实现的？

3. **RPC 框架设计**（高频）
   - 为什么用二进制协议而不是 Protobuf/JSON？
   - 序列化的 VarInt 编码原理？
   - 服务注册/发现的设计？如果要做分布式版本？
   - 连接池的设计？长连接 vs 短连接？
   - 超时机制怎么实现？协程级超时 vs 连接级超时？

4. **性能优化**（中频）
   - CQE 批量收割为什么快？批量的最优大小？
   - 时间轮 vs 红黑树定时器的取舍？
   - 内存池的设计？arena vs object pool？

5. **C++ 高级特性**（中频）
   - RAII 在项目中的应用？
   - move 语义和完美转发的使用场景？
   - 模板元编程在序列化中的应用？
   - `std::optional` / `std::variant` 的使用？

#### Step 6.4：CI/CD（0.5 天）

```yaml
# .github/workflows/ci.yml
name: CI
on: [push, pull_request]
jobs:
  build-and-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: |
          sudo apt install liburing-dev
      - name: Configure
        run: cmake -B build -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
      - name: Build
        run: cmake --build build -j$(nproc)
      - name: Test
        run: cd build && ctest --output-on-failure
```

### ✅ 阶段检查点
- [ ] 至少 2 项性能优化完成，benchmark 有提升
- [ ] README 完整（Badge + 快速开始 + Benchmark 表）
- [ ] ARCHITECTURE.md 包含架构图和设计决策
- [ ] INTERVIEW_QA.md 包含 40+ 问答
- [ ] CI 配置完成，GitHub Actions 可触发
- [ ] 项目可 `git clone` → `cmake -B build` → `cmake --build build` → `ctest` 一键通过

---

## 📎 附录

### A. 常见 Bug 与陷阱速查

本项目开发过程中遇到的 7 个关键 Bug，复刻时务必注意：

| # | Bug | 严重度 | 根因 | 修复 |
|---|-----|--------|------|------|
| 1 | EventLoop 单例未初始化 | Critical | 多处创建独立 IoUring 实例 | 改为 `static instance()` 单例 |
| 2 | run_once 批量 CQE 未处理 | Critical | 只取一个 CQE 就返回 | 改为循环 peek 直到队列空 |
| 3 | BufferBlock 析构栈溢出 | Medium | 递归 unique_ptr 链表过长 | 改为循环释放 |
| 4 | TimingWheel off-by-one | Medium | 插入 slot 计算差一 | 修正 `(current + ticks) % size` |
| 5 | TimedRecv/Send nop 唤醒 | Medium | 未实际提交 IO 操作 | 确保 SQE 已提交 |
| 6 | msg_ring 签名不匹配 | Medium | liburing 版本 API 变更 | 统一使用 liburing 2.5+ API |
| 7 | WithTimeoutAwaiter lambda 捕获 | Medium | lambda 捕获局部引用导致悬垂 | 改为值捕获或 shared_ptr |

### B. 推荐学习资源汇总

#### C++20 协程
| 资源 | 类型 | 难度 | 链接 |
|------|------|------|------|
| Lewis Baker 博客系列 | 文章 | ★★★ | https://lewissbaker.github.io/ |
| cppreference coroutines | 参考 | ★★ | https://en.cppreference.com/w/cpp/language/coroutines |
| CppCon 2019: Gor Nishanov | 视频 | ★★★ | YouTube 搜索 "CppCon 2019 Nishanov" |
| 《C++20 权威指南》第 10 章 | 书籍 | ★★ | — |

#### io_uring
| 资源 | 类型 | 难度 | 链接 |
|------|------|------|------|
| Lord of the io_uring | 教程 | ★★ | https://unixism.net/loti/ |
| io_uring 白皮书 | 论文 | ★★★ | https://kernel.dk/io_uring.pdf |
| liburing 源码 | 代码 | ★★★ | https://github.com/axboe/liburing |
| LWN io_uring 系列 | 文章 | ★★★ | lwn.net 搜索 "io_uring" |

#### 网络编程
| 资源 | 类型 | 难度 | 链接 |
|------|------|------|------|
| Beej's Guide | 教程 | ★ | https://beej.us/guide/bgnet/ |
| 《TCP/IP 详解 卷1》 | 书籍 | ★★ | — |
| muduo 源码 | 代码 | ★★★ | https://github.com/chenshuo/muduo |

#### RPC 框架
| 资源 | 类型 | 难度 | 链接 |
|------|------|------|------|
| brpc 设计文档 | 文档 | ★★★ | https://github.com/apache/brpc/docs |
| grpc 源码 | 代码 | ★★★★ | https://github.com/grpc/grpc |
| 《大规模 C++ 项目设计》 | 书籍 | ★★★ | — |

### C. 项目目录结构

```
~/rpc/
├── CMakeLists.txt              # 顶层构建
├── README.md                   # 项目说明
├── ARCHITECTURE.md             # 架构文档
├── PERFORMANCE_REPORT.md       # 性能报告
├── INTERVIEW_QA.md             # 面试问答
├── LICENSE                     # MIT
├── .clang-format               # 代码格式
├── .gitignore
├── .github/workflows/ci.yml   # CI
├── include/
│   ├── coroutine/              # 协程运行时
│   │   ├── task.hpp
│   │   ├── generator.hpp
│   │   ├── when_all.hpp
│   │   ├── when_any.hpp
│   │   ├── with_timeout.hpp
│   │   ├── lazy.hpp
│   │   └── sync_wait.hpp
│   ├── io/                     # IO 引擎
│   │   ├── io_uring.hpp
│   │   ├── operation.hpp
│   │   ├── event_loop.hpp
│   │   ├── event_loop_pool.hpp
│   │   └── timer.hpp
│   ├── net/                    # 网络层
│   │   ├── buffer.hpp
│   │   ├── tcp_server.hpp
│   │   ├── tcp_client.hpp
│   │   ├── connection.hpp
│   │   ├── connection_pool.hpp
│   │   └── load_balancer.hpp
│   ├── rpc/                    # RPC 框架
│   │   ├── serializer.hpp
│   │   ├── protocol.hpp
│   │   ├── rpc_result.hpp
│   │   ├── service_registry.hpp
│   │   ├── rpc_server.hpp
│   │   ├── rpc_client.hpp
│   │   └── rpc_stub.hpp
│   └── idl/                    # IDL 解析
│       ├── parser.hpp
│       └── codegen.hpp
├── tests/
│   ├── test_buffer.cpp
│   ├── test_serializer.cpp
│   ├── test_coroutine.cpp
│   ├── test_io_uring.cpp
│   ├── test_timer.cpp
│   ├── test_tcp.cpp
│   ├── test_rpc_e2e.cpp
│   ├── test_connection_pool.cpp
│   └── test_load_balancer.cpp
└── benchmarks/
    ├── bench_coroutine.cpp
    ├── bench_serializer.cpp
    ├── bench_buffer.cpp
    └── bench_rpc_lookup.cpp
```

### D. 时间分配建议

```
全职投入（40h/周）：6~8 周完成
兼职投入（20h/周）：10~14 周完成
周末投入（10h/周）：16~22 周完成

推荐节奏：
- 周一~周五：每天 2~3 小时编码
- 周六：4~6 小时集中攻坚（难点模块）
- 周日：1~2 小时整理笔记 + 复盘

关键里程碑：
✅ 第 3 周末：协程模块全部测试通过
✅ 第 5 周末：TCP Echo 端到端跑通
✅ 第 8 周末：RPC 端到端调用跑通
✅ 第 10 周末：全量测试 + ASan 通过
✅ 第 11 周末：文档 + 面试材料完成
```

### E. 面试话术模板

**项目介绍（1 分钟版）**：
> 我独立开发了一个基于 C++20 协程和 io_uring 的高性能 RPC 框架。整个框架约 8,700 行代码，分为协程运行时、IO 引擎、网络层、RPC 核心和 IDL 五个模块。协程层实现了 Task、WhenAll、WhenAny、WithTimeout 等组合原语，支持对称转移优化；IO 层基于 io_uring 封装了异步 accept/recv/send，配合时间轮定时器实现协程级超时；网络层实现了链式 Buffer、连接池和负载均衡；RPC 层使用自研二进制序列化（VarInt 编码）和协议编解码，支持服务注册、Stub 生成和端到端调用。Benchmark 数据：协程调度 QPS 4000 万+，序列化 QPS 5.25 亿，全量 38 个测试用例通过，ASan 零内存泄漏。

**亮点展开点**（面试官追问时）：
1. symmetric transfer 如何减少栈消耗
2. io_uring CQE 批量收割的 2~3x QPS 提升
3. 时间轮 vs 红黑树的取舍
4. VarInt 编码的空间节省
5. ASan + UBSan 的零泄漏保证

---

> 💡 **最后提醒**：复刻的目的是**理解原理**，不是抄代码。每个模块先自己想设计，遇到卡点再参考源码。面试官看重的是你对底层原理的理解深度，而非代码行数。
