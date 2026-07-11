# 🐳 Docker 容器化指南

## 容器能解决什么？

| 问题 | 容器是否解决 |
|------|-------------|
| ✅ 编译环境不一致（GCC/CMake/liburing 版本） | **是** |
| ✅ 依赖安装繁琐 | **是** |
| ✅ "我机器上能编"问题 | **是** |
| ✅ CI/CD 环境标准化 | **是** |
| ⚠️ io_uring 运行 | **部分**（见下） |
| ❌ 性能数据可信度 | **否**（容器网络栈有开销） |

## ⚠️ io_uring 的硬限制

**容器共享宿主机内核**。io_uring 是否可用取决于宿主内核版本：

| 宿主环境 | 内核版本 | io_uring 支持 |
|----------|---------|--------------|
| Ubuntu 22.04+ | 5.15+ | ✅ 完整支持 |
| Ubuntu 20.04 | 5.4 | ✅ 基础支持（部分高级特性缺） |
| macOS Docker Desktop | 5.15+ (VM) | ✅ 可用 |
| Windows WSL2 + Docker | 5.15+ (VM) | ✅ 可用 |
| CentOS 7 / Ubuntu 18.04 | 3.10 / 4.15 | ❌ **不支持** |

> **结论**：容器让项目在"任何有 Docker + 新内核的环境"下跑，但不能突破内核版本限制。

## 快速上手

### 前置条件

```bash
# 1. 确认宿主内核 ≥ 5.4
uname -r   # 例: 6.5.0-44-generic ✅

# 2. 确认 Docker 已安装
docker --version
docker compose version
```

### 一键编译 + 测试

```bash
# 编译（Release + ASan）
docker compose up build

# 运行全部测试
docker compose up test

# 跑性能 benchmark
docker compose up benchmark
```

### 交互式开发

```bash
# 进入带完整工具链的开发容器
docker compose run dev

# 容器内可自由操作：
# $ cmake -B build -DCMAKE_BUILD_TYPE=Debug
# $ cmake --build build -j$(nproc)
# $ cd build && ctest --output-on-failure
```

### 仅运行时

```bash
# 进入精简 runtime 容器（无编译工具，体积更小）
docker compose run shell
```

## 镜像说明

| 镜像 | 用途 | 预估体积 |
|------|------|---------|
| `rpc-framework:builder` | 编译 + 测试 + 开发 | ~800MB |
| `rpc-framework:runtime` | 仅运行二进制 | ~120MB |

多阶段构建确保 runtime 镜像不包含编译工具链，体积大幅缩小。

## 常见问题

### Q: macOS 上 io_uring 性能数据可信吗？

**不太可信**。Docker Desktop for Mac 使用虚拟化网络栈，io_uring 跑在 VM 内核上，网络 IO 路径有额外开销。协程调度和序列化的 benchmark 数据相对可信，网络相关数据建议在真实 Linux 主机上测。

### Q: 如何在容器内跑端到端测试？

端到端测试需要 server + client 两个进程。可以用 `docker compose` 的多服务拓扑，或直接在 dev 容器内手动启动：

```bash
docker compose run dev
# 容器内：
$ ./build-release/examples/echo_server &
$ ./build-release/examples/echo_client
```

### Q: CI/CD 怎么用？

```yaml
# GitHub Actions 示例
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build & Test
        run: docker compose up build test
```

### Q: 容器内能用 VS Code 远程开发吗？

可以。安装 [Dev Containers 扩展](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)，在项目根目录创建 `.devcontainer/devcontainer.json`：

```json
{
  "dockerComposeFile": "../docker-compose.yml",
  "service": "dev",
  "workspaceFolder": "/rpc"
}
```

## Dockerfile 结构

```
┌─────────────────────────────────┐
│  Stage 1: builder (Ubuntu 24.04) │
│  GCC-13, CMake, liburing, GTest  │
│  → 编译 Release + ASan           │
└──────────────┬──────────────────┘
               │ COPY --from=builder
┌──────────────▼──────────────────┐
│  Stage 2: runtime (Ubuntu 24.04) │
│  仅 liburing2 + 运行时库         │
│  → 精简镜像 ~120MB              │
└─────────────────────────────────┘
```
