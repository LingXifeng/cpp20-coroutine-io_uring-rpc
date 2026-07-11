cmake_minimum_required(VERSION 3.20)
project(rpc_framework VERSION 1.0.0 LANGUAGES CXX)

# C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Sanitizer 选项
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

set(ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
set(UBSAN_FLAGS "-fsanitize=undefined -fno-omit-frame-pointer")
set(TSAN_FLAGS "-fsanitize=thread")

set(SAN_FLAGS "")
if(ENABLE_ASAN)
    set(SAN_FLAGS "${SAN_FLAGS} ${ASAN_FLAGS}")
endif()
if(ENABLE_UBSAN)
    set(SAN_FLAGS "${SAN_FLAGS} ${UBSAN_FLAGS}")
endif()
if(ENABLE_TSAN)
    set(SAN_FLAGS "${SAN_FLAGS} ${TSAN_FLAGS}")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 ${SAN_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g ${SAN_FLAGS}")

if(SAN_FLAGS)
    message(STATUS "  Sanitizers: ${SAN_FLAGS}")
endif()

# 查找依赖
find_package(Threads REQUIRED)

# 查找 liburing
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBURING QUIET liburing)
endif()

if(NOT LIBURING_FOUND)
    # 手动查找
    find_library(LIBURING_LIBRARY NAMES uring PATHS /usr/lib /usr/local/lib)
    find_path(LIBURING_INCLUDE_DIR NAMES liburing.h PATHS /usr/include /usr/local/include)
    if(LIBURING_LIBRARY AND LIBURING_INCLUDE_DIR)
        set(LIBURING_FOUND TRUE)
        set(LIBURING_LIBRARIES ${LIBURING_LIBRARY})
        set(LIBURING_INCLUDE_DIRS ${LIBURING_INCLUDE_DIR})
    endif()
endif()

if(LIBURING_FOUND)
    message(STATUS "io_uring support: YES (${LIBURING_LIBRARIES})")
    set(HAVE_IO_URING TRUE)
else()
    message(STATUS "io_uring support: NO (using epoll fallback)")
    set(HAVE_IO_URING FALSE)
endif()

# 头文件库
add_library(rpc_framework INTERFACE)
target_include_directories(rpc_framework INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(rpc_framework INTERFACE Threads::Threads)

if(HAVE_IO_URING)
    target_compile_definitions(rpc_framework INTERFACE HAVE_IO_URING=1)
    target_include_directories(rpc_framework INTERFACE ${LIBURING_INCLUDE_DIRS})
    target_link_libraries(rpc_framework INTERFACE ${LIBURING_LIBRARIES})
endif()

# 示例程序
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# 测试
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 安装
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/rpc
    FILES_MATCHING PATTERN "*.hpp"
)

# 打印配置摘要
message(STATUS "")
message(STATUS "RPC Framework Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  io_uring: ${HAVE_IO_URING}")
message(STATUS "  Examples: ${BUILD_EXAMPLES}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "")
