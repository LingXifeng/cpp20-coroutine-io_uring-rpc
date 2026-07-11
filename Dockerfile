# ============================================================
# C++20 Coroutine + io_uring RPC Framework — Dev Container
# ============================================================
# 多阶段构建：builder(编译) → runtime(精简运行)
#
# ⚠️  io_uring 依赖宿主机内核 ≥ 5.4（容器共享宿主内核）
#     macOS Docker Desktop: 内核通常 5.15+ ✓
#     Windows WSL2:        内核通常 5.15+ ✓
#     老旧 Linux 主机:      需确认 uname -r
# ============================================================

# ---------- Stage 1: Builder ----------
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# 系统依赖 + 编译工具链
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    g++-13 \
    gcc-13 \
    liburing-dev \
    liburing2 \
    libgoogle-glog-dev \
    libgflags-dev \
    libgtest-dev \
    libbenchmark-dev \
    clang-format \
    clang-tidy \
    git \
    ca-certificates \
 && apt-get clean

# 设置 GCC-13 为默认编译器
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100

WORKDIR /rpc

# 先拷贝 CMake 配置，利用 Docker 缓存层
COPY CMakeLists.txt cmake/ ./cmake/
COPY include/ ./include/
COPY src/ ./src/
COPY tests/ ./tests/
COPY benchmark/ ./benchmark/
COPY examples/ ./examples/

# Release 编译
RUN cmake -B build-release \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc-13 \
    -DCMAKE_CXX_COMPILER=g++-13 \
 && cmake --build build-release -j$(nproc)

# Debug + ASan 编译
RUN cmake -B build-asan \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=gcc-13 \
    -DCMAKE_CXX_COMPILER=g++-13 \
    -DENABLE_ASAN=ON \
    -DENABLE_UBSAN=ON \
 && cmake --build build-asan -j$(nproc)

# ---------- Stage 2: Runtime (精简镜像) ----------
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    liburing2 \
    libgoogle-glog0v6 \
    libgflags2.2 \
    ca-certificates \
 && apt-get clean

WORKDIR /rpc

# 从 builder 拷贝编译产物
COPY --from=builder /rpc/build-release ./build-release
COPY --from=builder /rpc/build-asan    ./build-asan

# 拷贝源码（方便在容器内开发/调试）
COPY --from=builder /rpc/include  ./include
COPY --from=builder /rpc/src      ./src
COPY --from=builder /rpc/tests    ./tests
COPY --from=builder /rpc/benchmark ./benchmark
COPY --from=builder /rpc/examples ./examples
COPY --from=builder /rpc/CMakeLists.txt ./

# 默认：运行全部测试
CMD ["./build-release/tests/test_buffer"]
