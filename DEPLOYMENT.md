# 🚀 服务器部署指南

> 从零开始将 RPC 框架部署到生产服务器的完整操作手册。

---

## 1. 服务器选型与要求

### 1.1 最低配置

| 项目 | 最低 | 推荐 | 说明 |
|------|------|------|------|
| **内核** | ≥ 5.4 | ≥ 5.15 | io_uring 硬性要求 |
| **CPU** | 2 核 | 4+ 核 | 多线程 EventLoop 需要多核 |
| **内存** | 2 GB | 8+ GB | 连接池 + Buffer 池占用 |
| **磁盘** | 20 GB | 50 GB SSD | Docker 镜像 + 日志 |
| **OS** | Ubuntu 22.04 | Ubuntu 24.04 | liburing 2.x + GCC 13 原生支持 |

### 1.2 华为云推荐规格

| 场景 | 规格 | 大致月费 | 说明 |
|------|------|---------|------|
| 开发测试 | c7.large (2C4G) | ~200 | 够跑通全部测试 |
| 小规模生产 | c7.xlarge (4C8G) | ~400 | 单机万级 QPS |
| 中规模生产 | c7.2xlarge (8C16G) | ~800 | 多线程 + 连接池 |

> 选 **c7（鲲鹏 ARM）** 或 **c6（x86）** 均可，项目纯 C++ 无架构依赖。

### 1.3 内核检查（部署第一步必做）

```bash
# SSH 登录服务器后
uname -r
# 期望输出: 5.15.0-xxx 或更高

# 如果内核太老，升级：
sudo apt update && sudo apt install -y linux-generic-hwe-22.04
sudo reboot
```

---

## 2. 环境初始化

### 2.1 一键初始化脚本

```bash
#!/bin/bash
# deploy_init.sh — 服务器环境初始化（Ubuntu 22.04/24.04）
set -e

echo "=== [1/5] 系统更新 ==="
sudo apt update && sudo apt upgrade -y

echo "=== [2/5] 安装 Docker ==="
if ! command -v docker &>/dev/null; then
    curl -fsSL https://get.docker.com | sudo sh
    sudo usermod -aG docker $USER
    echo "⚠️  Docker 已安装，请重新登录使 group 生效，然后重新运行此脚本"
    exit 0
fi

echo "=== [3/5] 安装 Docker Compose ==="
# Docker 24+ 自带 compose 插件，确认一下
docker compose version || {
    sudo apt install -y docker-compose-plugin
}

echo "=== [4/5] 系统参数调优 ==="
# 文件描述符上限（百万级长连接需要）
sudo tee /etc/security/limits.d/rpc.conf <<EOF
* soft nofile 1048576
* hard nofile 1048576
* soft nproc  65535
* hard nproc  65535
EOF

# 内核网络参数
sudo tee /etc/sysctl.d/99-rpc.conf <<EOF
# TCP 连接队列
net.core.somaxconn = 65535
net.core.netdev_max_backlog = 65535
# TCP 缓冲区
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216
net.ipv4.tcp_rmem = 4096 87380 16777216
net.ipv4.tcp_wmem = 4096 65536 16777216
# TIME_WAIT 复用
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_fin_timeout = 15
# 本地端口范围
net.ipv4.ip_local_port_range = 1024 65535
# io_uring 相关
fs.aio-max-nr = 1048576
EOF
sudo sysctl --system

echo "=== [5/5] 验证 ==="
echo "Kernel:  $(uname -r)"
echo "Docker:  $(docker --version)"
echo "Compose: $(docker compose version)"
echo "ulimit:  $(ulimit -n)"
echo "io_uring: $(cat /proc/sys/fs/aio-max-nr)"
echo ""
echo "✅ 环境初始化完成"
```

### 2.2 手动步骤（如果不想用脚本）

```bash
# 1. 安装 Docker
curl -fsSL https://get.docker.com | sudo sh
sudo usermod -aG docker $USER
# 重新登录

# 2. 调 ulimit（当前 session 生效）
ulimit -n 1048576

# 3. 调内核参数（永久生效）
sudo sysctl -w net.core.somaxconn=65535
sudo sysctl -w net.ipv4.tcp_tw_reuse=1
```

---

## 3. 部署项目

### 3.1 方式 A：Docker 部署（推荐）

```bash
# 1. 拉取代码
git clone https://github.com/YOUR_USERNAME/rpc.git ~/rpc
cd ~/rpc

# 2. 构建镜像（约 5-10 分钟，首次较慢）
docker compose build

# 3. 验证：运行全部测试
docker compose up test

# 4. 启动 RPC 服务
docker compose up -d rpc-server   # 后台运行
```

### 3.2 方式 B：原生部署（性能最优）

```bash
# 1. 安装编译依赖
sudo apt install -y build-essential cmake g++-13 \
    liburing-dev libgoogle-glog-dev libgflags-dev \
    libgtest-dev libbenchmark-dev

# 2. 设置 GCC-13
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100

# 3. 拉取代码并编译
git clone https://github.com/YOUR_USERNAME/rpc.git ~/rpc
cd ~/rpc
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 4. 运行测试
cd build && ctest --output-on-failure

# 5. 启动服务（见第 4 节）
```

### 3.3 方式 C：从本机传二进制（无需编译环境）

```bash
# 在开发机上编译后，直接传二进制到服务器
# 开发机：
scp build/librpc_core.a build/examples/echo_server \
    user@server:/opt/rpc/

# 服务器只需装运行时库：
# sudo apt install -y liburing2 libgoogle-glog0v6 libgflags2.2
```

---
## 4. 服务运行与管理

### 4.1 systemd 服务文件（原生部署推荐）

```ini
# /etc/systemd/system/rpc-server.service
[Unit]
Description=RPC Framework Server
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=rpc
Group=rpc
WorkingDirectory=/opt/rpc
ExecStart=/opt/rpc/build/examples/echo_server --port 8080 --threads 4
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5
LimitNOFILE=1048576

# 安全加固
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/opt/rpc/logs

# 环境变量
Environment=RPC_LOG_LEVEL=1
Environment=RPC_IO_URING_ENTRIES=4096
Environment=RPC_BUFFER_POOL_SIZE=1024

[Install]
WantedBy=multi-user.target
```

```bash
# 创建服务用户
sudo useradd -r -s /bin/false rpc

# 部署二进制
sudo mkdir -p /opt/rpc/logs
sudo cp -r ~/rpc/build /opt/rpc/build
sudo chown -R rpc:rpc /opt/rpc

# 启动服务
sudo systemctl daemon-reload
sudo systemctl enable rpc-server
sudo systemctl start rpc-server

# 查看状态
sudo systemctl status rpc-server
journalctl -u rpc-server -f   # 实时日志
```

### 4.2 Docker Compose 生产配置

在 `docker-compose.yml` 中追加生产服务定义：

```yaml
  # -------- 生产 RPC 服务 --------
  rpc-server:
    build:
      context: .
      dockerfile: Dockerfile
      target: runtime
    image: rpc-framework:runtime
    ports:
      - "8080:8080"
    environment:
      - RPC_LOG_LEVEL=1            # 0=INFO 1=WARNING 2=ERROR
      - RPC_IO_URING_ENTRIES=4096
      - RPC_BUFFER_POOL_SIZE=1024
      - RPC_THREADS=4
    ulimits:
      nofile:
        soft: 1048576
        hard: 1048576
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "nc", "-z", "localhost", "8080"]
      interval: 10s
      timeout: 5s
      retries: 3
    logging:
      driver: json-file
      options:
        max-size: "100m"
        max-file: "5"
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 8G
        reservations:
          cpus: '2'
          memory: 4G
```

```bash
# 启动
docker compose up -d rpc-server

# 查看日志
docker compose logs -f rpc-server

# 健康检查
docker compose ps
curl http://localhost:8080/health   # 如果有健康检查端点

# 重启（零停机更新）
docker compose up -d --no-deps --build rpc-server
```

### 4.3 配置管理

推荐用环境变量 + 配置文件分层：

```bash
# /opt/rpc/config/rpc.conf
RPC_PORT=8080
RPC_THREADS=4
RPC_IO_URING_ENTRIES=4096
RPC_IO_URING_SQPOLL=0          # 0=关闭 1=开启(需CAP_SYS_NICE)
RPC_BUFFER_POOL_SIZE=1024
RPC_BUFFER_BLOCK_SIZE=4096
RPC_LOG_LEVEL=1
RPC_LOG_DIR=/opt/rpc/logs
RPC_CONNECTION_POOL_SIZE=256
RPC_LOAD_BALANCER=round_robin  # round_robin | consistent_hash | least_conn
RPC_TIMEOUT_MS=5000
RPC_RETRY_MAX=3
RPC_RETRY_BACKOFF_MS=100
```

---

## 5. 多节点部署

### 5.1 架构图

```
                    ┌─────────────┐
                    │   Nginx /   │
                    │   ELB / HLB │  ← 第 7 层负载均衡
                    └──────┬──────┘
               ┌───────────┼───────────┐
               ▼           ▼           ▼
        ┌────────────┐ ┌────────────┐ ┌────────────┐
        │ RPC Server │ │ RPC Server │ │ RPC Server │
        │  Node 1    │ │  Node 2    │ │  Node 3    │
        │ :8080      │ │ :8080      │ │ :8080      │
        └────────────┘ └────────────┘ └────────────┘
               │           │           │
               └───────────┼───────────┘
                           ▼
                    ┌─────────────┐
                    │   服务发现   │  ← etcd / Consul / 静态配置
                    └─────────────┘
```

### 5.2 Nginx 反向代理配置

```nginx
# /etc/nginx/conf.d/rpc-upstream.conf
upstream rpc_backend {
    least_conn;                    # 最少连接数策略
    server 10.0.1.10:8080 weight=1;
    server 10.0.1.11:8080 weight=1;
    server 10.0.1.12:8080 weight=1;
    keepalive 256;                 # 长连接池
}

server {
    listen 9090;
    location / {
        proxy_pass http://rpc_backend;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_connect_timeout 5s;
        proxy_read_timeout 30s;
        proxy_send_timeout 30s;
    }
}
```

### 5.3 Docker Swarm 多节点（轻量方案）

```bash
# 在管理节点上初始化
docker swarm init --advertise-addr 10.0.1.10

# 工作节点加入
docker swarm join --token SWMTKN-xxx 10.0.1.10:2377

# 部署服务（3 副本）
docker stack deploy -c docker-compose.yml rpc

# 扩缩容
docker service scale rpc_rpc-server=5
```

---
## 6. 监控与日志

### 6.1 进程监控

```bash
# 原生部署：systemd 自带
sudo systemctl status rpc-server

# Docker 部署
docker compose ps
docker inspect --format='{{.State.Health.Status}}' rpc-rpc-server-1
```

### 6.2 Prometheus 指标（预留接口）

在 `docker-compose.yml` 中追加：

```yaml
  prometheus:
    image: prom/prometheus:latest
    ports:
      - "9091:9090"
    volumes:
      - ./deploy/prometheus.yml:/etc/prometheus/prometheus.yml

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
```

### 6.3 日志轮转

```bash
# 原生部署：logrotate
sudo tee /etc/logrotate.d/rpc <<EOF
/opt/rpc/logs/*.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
    copytruncate
}
EOF

# Docker 部署：json-file driver 自带轮转（见 4.2 配置）
```

---

## 7. 性能调优

### 7.1 io_uring 参数

| 参数 | 默认 | 生产建议 | 说明 |
|------|------|---------|------|
| `entries` | 256 | 4096~8192 | SQ 队列深度，越大吞吐越高 |
| `cq_entries` | 2×SQ | 自动 | CQ 队列通常自动 2 倍 |
| `SQPOLL` | 关闭 | 按需开启 | 内核线程轮询，省系统调用但需 CAP_SYS_NICE |
| `register_files` | 关闭 | 开启 | 固定文件描述符表，减少内核查表 |
| `register_buffers` | 关闭 | 按需开启 | 固定缓冲区，零拷贝前提 |

### 7.2 系统级调优

```bash
# io_uring 相关
sudo sysctl -w fs.aio-max-nr=1048576     # 异步 IO 上限
sudo sysctl -w vm.max_map_count=262144   # mmap 区域上限

# 网络缓冲区
sudo sysctl -w net.core.rmem_max=16777216
sudo sysctl -w net.core.wmem_max=16777216

# CPU 亲和性（绑定进程到特定核）
taskset -c 0-3 /opt/rpc/build/examples/echo_server

# 关闭 NUMA 自动平衡（减少跨节点内存访问）
sudo sysctl -w kernel.numa_balancing=0
```

### 7.3 Benchmark 验证

```bash
# 部署后务必在服务器上跑 benchmark（不是 VM！）
cd /opt/rpc
./build/benchmark/bench_coroutine
./build/benchmark/bench_serializer
./build/benchmark/bench_buffer
./build/benchmark/bench_rpc_lookup

# 网络性能（需要真实网络栈）
./build/examples/echo_server &
./build/examples/echo_client --concurrency=100 --requests=100000
```

---

## 8. 安全加固

### 8.1 网络安全

```bash
# 仅开放必要端口
sudo ufw allow 22/tcp     # SSH
sudo ufw allow 8080/tcp   # RPC 服务
sudo ufw enable

# 或用 iptables
sudo iptables -A INPUT -p tcp --dport 8080 -j ACCEPT
sudo iptables -A INPUT -j DROP
```

### 8.2 容器安全

```yaml
# docker-compose.yml 安全选项
security_opt:
  - no-new-privileges:true
read_only: true
tmpfs:
  - /tmp
cap_drop:
  - ALL
cap_add:
  - NET_BIND_SERVICE    # 绑定 1024 以下端口
```

### 8.3 通信加密（生产必做）

RPC 框架当前是明文 TCP，生产环境需要加密：

| 方案 | 复杂度 | 性能 | 推荐 |
|------|--------|------|------|
| Nginx/HAProxy TLS 终结 | 低 | 高（内核 TLS） | ✅ 推荐 |
| 应用层 TLS（OpenSSL） | 中 | 中 | 按需 |
| WireGuard 隧道 | 低 | 高 | 内网通信推荐 |

---

## 9. 更新与回滚

### 9.1 滚动更新

```bash
# Docker 方式
docker compose build rpc-server          # 构建新镜像
docker compose up -d --no-deps rpc-server  # 只更新该服务

# 原生方式
cd ~/rpc && git pull
cmake --build build -j$(nproc)
sudo systemctl restart rpc-server
```

### 9.2 回滚

```bash
# Docker：回退到上一版本镜像
docker compose down
docker tag rpc-framework:runtime rpc-framework:runtime-broken
docker tag rpc-framework:runtime-prev rpc-framework:runtime
docker compose up -d rpc-server

# 原生：Git 回退
cd ~/rpc && git checkout HEAD~1
cmake --build build -j$(nproc)
sudo systemctl restart rpc-server
```

---

## 10. 完整部署检查清单

- [ ] 服务器内核 ≥ 5.4（`uname -r`）
- [ ] Docker 已安装（`docker --version`）
- [ ] ulimit ≥ 1048576（`ulimit -n`）
- [ ] 内核网络参数已调优
- [ ] 代码已 clone / 二进制已部署
- [ ] 全部测试通过（`docker compose up test` 或 `ctest`）
- [ ] 服务可启动（`systemctl start` 或 `docker compose up`）
- [ ] 健康检查通过
- [ ] 日志正常输出
- [ ] Benchmark 数据记录
- [ ] 防火墙仅开放必要端口
- [ ] TLS/加密已配置（生产必做）
- [ ] 监控已接入
- [ ] 回滚方案已验证

---

## 快速部署速查

```bash
# ====== 全流程 5 分钟速查（Ubuntu 24.04 服务器）======

# 1. 环境
curl -fsSL https://get.docker.com | sudo sh
sudo usermod -aG docker $USER  # 重新登录

# 2. 拉代码
git clone https://github.com/YOUR_USERNAME/rpc.git ~/rpc && cd ~/rpc

# 3. 编译 + 测试
docker compose up build test

# 4. 启动
docker compose up -d rpc-server

# 5. 验证
docker compose ps
curl localhost:8080
```
