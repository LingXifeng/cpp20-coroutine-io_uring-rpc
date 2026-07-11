# 示例程序

add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server PRIVATE rpc_framework)

add_executable(echo_client echo_client.cpp)
target_link_libraries(echo_client PRIVATE rpc_framework)

add_executable(rpc_example rpc_example.cpp)
target_link_libraries(rpc_example PRIVATE rpc_framework)
