# 📖 零基础上传项目到 GitHub 完整指南

> 从"没有 GitHub 账号"到"项目成功上线"，每一步都写清楚。

---

## 第一步：注册 GitHub 账号

1. 打开 https://github.com/signup
2. 填写：
   - **Email**：你的常用邮箱（学校邮箱优先，可以申请学生包）
   - **Password**：设一个密码
   - **Username**：选一个简短好记的，比如 `zhangsan` 或 `zhangsan-dev`
3. 完成验证（可能要解一个拼图）
4. 去邮箱点确认链接
5. ✅ 注册完成

> 💡 **学生优惠**：注册后去 https://education.github.com/pack 申请 Student Developer Pack，免费送你 Copilot、私有仓库等一堆福利。

---

## 第二步：在 GitHub 网页上创建仓库

1. 登录 GitHub 后，点右上角 **`+`** → **`New repository`**
2. 填写：

| 字段 | 填什么 | 说明 |
|------|--------|------|
| Repository name | `cpp20-coroutine-io_uring-rpc` | 项目名，用小写+短横线 |
| Description | `High-performance RPC framework based on C++20 coroutine and io_uring` | 一句话描述 |
| Public / Private | 选 **Public** | 秋招要让面试官看到！ |

3. ⚠️ **不要勾选**下面三个选项（Add a README / .gitignore / License），因为我们的项目已经有这些文件了
4. 点 **`Create repository`**
5. ✅ 仓库创建完成，你会看到一个空仓库页面

> 创建后页面会显示一个 URL，类似：
> `https://github.com/你的用户名/cpp20-coroutine-io_uring-rpc`
> 
> **记住你的用户名**，后面要用。

---

## 第三步：生成 SSH 密钥（让服务器能推代码到 GitHub）

这一步在你的 **VM 服务器上**操作（通过 Tailscale SSH 连上去）。

### 3.1 检查是否已有密钥

```bash
ls -la ~/.ssh/id_ed25519.pub
```

- 如果显示文件存在 → 跳到 3.3
- 如果显示 No such file → 继续下一步

### 3.2 生成新密钥

```bash
ssh-keygen -t ed25519 -C "你的GitHub邮箱@example.com"
```

会问你三个问题，**全部直接按回车**（用默认值就行）：

```
Enter file in which to save the key (/home/adminx/.ssh/id_ed25519):  ← 按回车
Enter passphrase (empty for no passphrase):                           ← 按回车
Enter same passphrase again:                                          ← 按回车
```

### 3.3 查看并复制公钥

```bash
cat ~/.ssh/id_ed25519.pub
```

会输出一行类似这样的内容：

```
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI... 你的邮箱@example.com
```

**把这整行复制下来**（从 `ssh-ed25519` 开始到邮箱结束，一整行）。

---

## 第四步：把公钥添加到 GitHub

1. 打开 https://github.com/settings/keys
2. 点 **`New SSH key`**
3. 填写：

| 字段 | 填什么 |
|------|--------|
| Title | `VM-Server` （随便起，标记是哪台机器） |
| Key type | `Authentication Key` |
| Key | 粘贴刚才复制的整行公钥 |

4. 点 **`Add SSH key`**
5. 可能要输 GitHub 密码确认
6. ✅ 添加完成

### 验证连接

回到服务器上执行：

```bash
ssh -T git@github.com
```

第一次连接会问：

```
Are you sure you want to continue connecting (yes/no)?  ← 输 yes 回车
```

成功会显示：

```
Hi 你的用户名! You've successfully authenticated, but GitHub does not provide shell access.
```

看到这行就说明 ✅ 服务器已经能和 GitHub 通信了。

---

## 第五步：把代码推上去

### 5.1 进入项目目录，添加远程仓库

```bash
cd ~/rpc

# 把 "你的用户名" 替换成你的 GitHub 用户名
git remote add origin git@github.com:你的用户名/cpp20-coroutine-io_uring-rpc.git
```

### 5.2 推送代码

```bash
# 推送所有提交到 GitHub
git push -u origin master
```

会看到类似输出：

```
Enumerating objects: 85, done.
Counting objects: 100% (85/85), done.
Delta compression using up to 4 threads
Compressing objects: 100% (72/72), done.
Writing objects: 100% (85/85), 156.23 KiB | 3.12 MiB/s, done.
To github.com:你的用户名/cpp20-coroutine-io_uring-rpc.git
 * [new branch]      master -> master
Branch 'master' set up to track remote branch 'master' from 'origin'.
```

### 5.3 验证

打开浏览器访问：

```
https://github.com/你的用户名/cpp20-coroutine-io_uring-rpc
```

能看到你的项目代码就 ✅ 大功告成！

---

## 第六步：美化仓库（加分项）

### 6.1 添加 Topics（标签，让别人能搜到）

1. 在仓库页面，点右侧 **⚙️ About** 旁边的齿轮图标
2. **Topics** 填入：`cpp20` `coroutine` `io-uring` `rpc` `high-performance` `linux` `network-programming`
3. 勾选 **Releases** / **Packages**（可选）
4. 点 **Save**

### 6.2 确认 README 正常显示

仓库首页应该自动显示 README.md 的内容，包括：
- 6 个 badge（C++20 / io_uring / ASan / UBSan / MIT / Tests）
- 特性列表
- 快速开始
- Benchmark 表格

如果没显示，检查 README.md 是否在仓库根目录。

### 6.3 设置主分支为默认（如果需要）

1. 仓库页面 → **Settings** → **General** → **Default branch**
2. 确认是 `master`（或 `main`）
3. 如果想改成 `main`：

```bash
git branch -m master main
git push -u origin main
```

然后在 GitHub Settings 里把默认分支改成 `main`。

---

## 常见问题排查

### ❌ `Permission denied (publickey)`

**原因**：SSH 密钥没配对

**解决**：
```bash
# 1. 确认密钥存在
ls -la ~/.ssh/id_ed25519.pub

# 2. 确认公钥已添加到 GitHub（对比内容）
cat ~/.ssh/id_ed25519.pub
# 去 https://github.com/settings/keys 检查是否一致

# 3. 测试连接
ssh -T git@github.com
```

### ❌ `remote origin already exists`

**原因**：已经添加过 remote

**解决**：
```bash
# 更新 remote URL
git remote set-url origin git@github.com:你的用户名/cpp20-coroutine-io_uring-rpc.git
```

### ❌ `failed to push some refs`

**原因**：远程仓库有本地没有的文件（比如创建时勾选了 README）

**解决**：
```bash
# 先拉取远程内容再推
git pull origin master --rebase
git push -u origin master
```

### ❌ 推送很慢或超时

**原因**：网络问题（VM 网络出口慢）

**解决**：
```bash
# 增大超时
git config --global http.postBuffer 524288000
git config --global core.compression 0

# 或者用 SSH 方式（比 HTTPS 快且不需要输密码）
git remote set-url origin git@github.com:你的用户名/cpp20-coroutine-io_uring-rpc.git
```

---

## 完整操作速查（复制粘贴版）

把下面的 `YOUR_USERNAME` 替换成你的 GitHub 用户名，`YOUR_EMAIL` 替换成你的邮箱：

```bash
# === 1. 生成 SSH 密钥 ===
ssh-keygen -t ed25519 -C "YOUR_EMAIL"
# 连按 3 次回车

# === 2. 查看公钥（复制输出，添加到 GitHub） ===
cat ~/.ssh/id_ed25519.pub

# === 3. 测试 GitHub 连接 ===
ssh -T git@github.com
# 输入 yes

# === 4. 添加远程仓库并推送 ===
cd ~/rpc
git remote add origin git@github.com:YOUR_USERNAME/cpp20-coroutine-io_uring-rpc.git
git push -u origin master

# === 5. 打开浏览器验证 ===
# https://github.com/YOUR_USERNAME/cpp20-coroutine-io_uring-rpc
```

---

## 以后更新代码怎么推？

```bash
cd ~/rpc

# 1. 查看改了什么
git status

# 2. 添加修改的文件
git add .

# 3. 写提交说明
git commit -m "fix: 修复了xxx问题"

# 4. 推到 GitHub
git push
```

就这 4 步，以后每次改完代码都这样推。

---

## 面试怎么放链接

简历上写：

```
项目：基于 C++20 协程与 io_uring 的高性能 RPC 框架
GitHub：github.com/YOUR_USERNAME/cpp20-coroutine-io_uring-rpc
技术栈：C++20 / io_uring / 协程 / 零拷贝序列化 / 连接池 / 负载均衡
亮点：协程调度 QPS 4000万+ / 序列化 QPS 5.25亿 / 38 测试全绿 / ASan 零泄漏
```

面试官点进 GitHub 就能看到：badge 全绿、代码结构清晰、文档齐全、有 CI 有 Docker。
