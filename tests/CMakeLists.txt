# 测试

add_executable(test_coroutine test_coroutine.cpp)
target_link_libraries(test_coroutine PRIVATE rpc_framework)
add_test(NAME coroutine COMMAND test_coroutine)

add_executable(test_buffer test_buffer.cpp)
target_link_libraries(test_buffer PRIVATE rpc_framework)
add_test(NAME buffer COMMAND test_buffer)

add_executable(test_serializer test_serializer.cpp)
target_link_libraries(test_serializer PRIVATE rpc_framework)
add_test(NAME serializer COMMAND test_serializer)

# 性能基准测试
add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE rpc_framework)

# 端到端测试
add_executable(test_e2e test_e2e.cpp)
target_link_libraries(test_e2e PRIVATE rpc_framework)
add_test(NAME e2e COMMAND test_e2e)

# 协程+io_uring TCP echo 端到端测试
add_executable(test_coro_tcp test_coro_tcp.cpp)
target_link_libraries(test_coro_tcp PRIVATE rpc_framework)
add_test(NAME coro_tcp COMMAND test_coro_tcp)

# 真实 RPC 完整调用示例
add_executable(test_rpc_call test_rpc_call.cpp)
target_link_libraries(test_rpc_call PRIVATE rpc_framework)
add_test(NAME rpc_call COMMAND test_rpc_call)

# ConnectionPool + LoadBalancer 单元测试
add_executable(test_connpool_lb test_connpool_lb.cpp)
target_link_libraries(test_connpool_lb PRIVATE rpc_framework)
add_test(NAME connpool_lb COMMAND test_connpool_lb)

# 超时机制单元测试
add_executable(test_timeout test_timeout.cpp)
target_link_libraries(test_timeout PRIVATE rpc_framework)
add_test(NAME timeout COMMAND test_timeout)

# 多线程 EventLoop 单元测试
add_executable(test_eventloop_pool test_eventloop_pool.cpp)
target_link_libraries(test_eventloop_pool PRIVATE rpc_framework)
add_test(NAME eventloop_pool COMMAND test_eventloop_pool)
