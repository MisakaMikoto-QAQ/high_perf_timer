# High-Performance Timer

> 从零手写的 C++17 高性能定时器 —— 包含最小堆、时间轮两种数据结构，以及基于 epoll + eventfd 的定时器管理器。

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](https://www.kernel.org/)
[![Build](https://img.shields.io/badge/build-cmake%20%7C%20make-brightgreen.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/license-MIT-yellow.svg)](./LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0-orange.svg)](#更新日志--路线图)

---

## 项目简介

`high_perf_timer` 是一个**教学 + 可生产**双重用途的 C++17 定时器项目，从零实现三种主流定时器方案并统一到一个事件循环里：

- **MinHeapTimer（最小堆定时器）** —— 通过「数组实现二叉堆 + id→下标哈希表」实现 O(log n) 的添加、删除与触发；这是 libevent / nginx 的经典做法。
- **TimeWheelTimer（时间轮定时器）** —— 通过「环形数组 + 每槽链表 + 圈数（rotations）」实现 O(1) 插入，适合海量短定时场景；这是 Linux 内核 / Netty 的经典做法。
- **TimerManager（事件驱动管理器）** —— 基于 `epoll_wait` + `eventfd` 把定时器挂到事件循环上，超时时间由「下一个最近到期时间」决定，`eventfd` 用于「运行中新增/取消定时器后立即唤醒 epoll」，实现零忙等待。

### 亮点

- **零第三方依赖**：只用 libc / libstdc++ 与 Linux 系统调用。
- **两种数据结构**：既讲清算法，也讲清工程取舍。
- **线程安全**：内部 `mutex` 保护，回调在锁外执行，避免死锁。
- **可组合**：`TimerManager` 可以与任意 fd（socket、pipe、eventfd）共用同一个 epoll。
- **带完整测试**：包含功能测试、取消测试、周期测试、时间轮回绕测试。

---

## 功能特性

- ✅ **最小堆定时器**
  - O(log n) 插入 / 删除 / 触发
  - 任意位置 O(log n) 取消（通过 id → 堆下标映射）
  - 支持一次性 / 周期定时器
- ✅ **时间轮定时器**
  - O(1) 插入
  - 支持多圈延迟（`rotations`）
  - 支持一次性 / 周期定时器
- ✅ **定时器管理器**
  - 基于 `epoll_wait`，超时自动计算，无忙等待
  - `eventfd` 唤醒机制，运行中可动态增删定时器
  - 可与任意 fd 共用同一个 epoll 实例
- ✅ **线程安全**
  - 内部互斥锁保护数据结构
  - 回调在锁外执行，允许在回调中 `add` / `cancel`
- ✅ **优雅退出**
  - 通过 `stop()` 唤醒 `epoll_wait` 并结束事件循环
  - 支持 `SIGINT` / `SIGTERM` 信号

---

## 环境依赖

| 项目 | 版本要求 | 备注 |
| --- | --- | --- |
| 操作系统 | Linux（推荐 Ubuntu 20.04+ / WSL2 Ubuntu-24.04） | 依赖 `epoll`、`eventfd` |
| 编译器 | GCC 9+ / Clang 10+ | 需支持 C++17 |
| 构建工具 | CMake ≥ 3.16、Make | 或 Ninja |
| 第三方库 | 无 | 仅使用 libc / libstdc++ |
| 工具链 | `build-essential`, `cmake` | Ubuntu 下 `apt install` 即可 |

> macOS / Windows 原生**不支持**（本项目使用 Linux 专有系统调用 `epoll` 与 `eventfd`）。
> Windows 用户请使用 WSL2 或完整 Linux 虚拟机。

---

## 安装 / 编译步骤

### 1. 安装依赖（Ubuntu / WSL2 Ubuntu-24.04）

```bash
sudo apt update
sudo apt install -y build-essential cmake git
```

### 2. 获取源码

```bash
git clone <your-repo-url> high_perf_timer
cd high_perf_timer
```

或直接进入你已有的项目目录：

```bash
cd ~/projects/high_perf_timer
```

### 3. 编译

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

编译成功后，`build/` 目录下会生成：

- `libtimer.a` —— 静态库
- `test_timer` —— 单元测试可执行文件
- `epoll_example` —— 示例程序

### 4. 清理

```bash
cd build
make clean      # 只清理中间产物
rm -rf *        # 完全清理（含 CMake 缓存）
```

---

## 快速开始 / 使用示例

最小可运行示例：注册一个周期定时器（每 500ms 打印一次）和一个一次性定时器（3 秒后退出）。

```cpp
// examples/epoll_server.cpp
#include "timer_manager.h"
#include <csignal>
#include <iostream>

static TimerManager* g_mgr = nullptr;
static void on_signal(int) { if (g_mgr) g_mgr->stop(); }

int main() {
    TimerManager mgr;
    if (!mgr.init()) {
        std::cerr << "TimerManager::init failed\n";
        return 1;
    }
    g_mgr = &mgr;
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    // 周期任务：每 500ms 打印一次
    mgr.add_timer(500, []{
        std::cout << "[timer] 500ms tick\n";
    }, 500);

    // 一次性任务：3 秒后停止
    mgr.add_timer(3000, [&mgr]{
        std::cout << "[timer] 3s reached, stop\n";
        mgr.stop();
    });

    std::cout << "TimerManager running... (Ctrl+C to exit)\n";
    mgr.run();
    std::cout << "TimerManager exited\n";
    return 0;
}
```

运行：

```bash
cd build
./epoll_example
```

预期输出：

```
TimerManager running... (Ctrl+C to exit)
[timer] 500ms tick
[timer] 500ms tick
[timer] 500ms tick
[timer] 500ms tick
[timer] 500ms tick
[timer] 3s reached, stop
TimerManager exited
```

按 **Ctrl+C** 也能随时优雅退出。

---

## 项目结构

```
high_perf_timer/
├── CMakeLists.txt              # 顶层构建脚本
├── README.md
├── LICENSE
├── include/                    # 公开头文件
│   ├── timer.h                 # 基础类型：Timer / TimerId / TimerCallback
│   ├── min_heap_timer.h        # 最小堆定时器声明
│   ├── time_wheel_timer.h      # 时间轮定时器声明
│   └── timer_manager.h         # epoll 事件驱动管理器声明
├── src/                        # 实现文件
│   ├── min_heap_timer.cpp
│   ├── time_wheel_timer.cpp
│   └── timer_manager.cpp
├── tests/
│   └── test_timer.cpp          # 单元测试（自带断言）
├── examples/
│   └── epoll_server.cpp        # 快速开始示例
└── build/                      # 构建目录（cmake 生成）
```

关键文件说明：

| 文件 | 作用 |
| --- | --- |
| `include/timer.h` | 定义 `Clock`（`steady_clock`）、`TimePoint`、`TimerCallback`、`Timer` 对象 |
| `include/min_heap_timer.h` | 最小堆数据结构与接口 |
| `include/time_wheel_timer.h` | 时间轮数据结构与接口 |
| `include/timer_manager.h` | epoll + eventfd 事件循环封装 |
| `src/min_heap_timer.cpp` | 上浮 / 下沉 / 取消 / tick 全部实现 |
| `src/time_wheel_timer.cpp` | 槽位计算、圈数递减、`advance()` 实现 |
| `src/timer_manager.cpp` | `epoll_wait` 超时计算、`eventfd` 唤醒、信号安全退出 |

---

## 核心设计 / 原理说明

### 1. 最小堆定时器（MinHeapTimer）

数据结构：

```
heap_ : vector<shared_ptr<Timer>>      // 按 expire 排序的二叉堆
index_: unordered_map<TimerId, size_t> // id -> 堆下标
```

核心操作：

| 操作 | 复杂度 | 说明 |
| --- | --- | --- |
| `add` / `add_at` | O(log n) | 尾部插入 + 上浮 |
| `cancel` | O(log n) | 与尾元素交换 → pop → 上浮或下沉 |
| `tick` | O(k log n) | k = 本次到期数量 |
| `next_timeout_ms` | O(1) | 取堆顶 |

关键点：

- 使用 `steady_clock` 而非 `system_clock`，避免系统时间跳变影响定时。
- 周期定时器重排时使用 `now + interval`，**不是**旧 `expire + interval`，避免长期挂起后「一次追赶触发 N 次」。
- `tick()` **先取出到期定时器 → 释放锁 → 执行回调**，因此回调中可安全调用 `add` / `cancel`，不会死锁。

### 2. 时间轮定时器（TimeWheelTimer）

数据结构：

```
wheel_ : vector<list<WheelTimer>>   // 每个槽是一条链表
current_: size_t                     // 当前指针位置
tick_ms_: uint64_t                   // 每格代表多少毫秒
slots_  : size_t                     // 槽位数
```

槽位与圈数计算（delay_ms → slot）：

```
ticks     = ceil(delay_ms / tick_ms_)
rotations = ticks / slots_
slot      = (current_ + ticks % slots_) % slots_
```

核心操作：

| 操作 | 复杂度 | 说明 |
| --- | --- | --- |
| `add` | O(1) | 哈希 + 链表尾插 |
| `cancel` | O(k) | k = 该槽链表长度（可用双向链表优化到 O(1)） |
| `tick`（走一格） | O(k) | k = 当前槽触发数 |
| `advance`（按真实时间走多格） | O(m·k) | m = 需要前进的格数 |

关键点：

- 向上取整（`(delay + tick - 1) / tick`）保证**不早触发**。
- `rotations > 0` 时递减而不触发。
- 覆盖范围 = `slots_ × tick_ms_`，超出范围由多圈处理。

### 3. 事件驱动管理器（TimerManager）

事件循环伪代码：

```
while (running_) {
    timeout = timer_.next_timeout_ms();           // 距离最近到期还有多少 ms
    n = epoll_wait(epfd_, events, MAX, timeout);  // 无定时器时阻塞 -1
    for each event:
        if fd == wakeup_fd_: drain; continue     // 唤醒事件
        else: fd_cb_(fd, events)
    timer_.tick();                                // 触发到期定时器
}
```

`eventfd` 的作用：

- 运行中调用 `add_timer` 后，如果新定时器比当前 `epoll_wait` 的超时更早，直接写入 `eventfd` 唤醒 `epoll_wait`，让它重新计算超时，无需等到旧超时。

`stop()` 的实现：

- `running_ = false` + 写入 `eventfd` → `epoll_wait` 立即返回 → 循环退出。

---

## API 说明

### 公共类型（`timer.h`）

```cpp
using Clock         = std::chrono::steady_clock;
using TimePoint     = Clock::time_point;
using TimerCallback = std::function<void()>;
using TimerId       = uint64_t;
```

### `class MinHeapTimer`

```cpp
explicit MinHeapTimer(size_t capacity = 64);

// 相对时间添加；interval_ms > 0 表示周期定时器
TimerId add(uint64_t delay_ms, TimerCallback cb, uint64_t interval_ms = 0);

// 绝对时间添加
TimerId add_at(TimePoint expire, TimerCallback cb, uint64_t interval_ms = 0);

// 取消；成功返回 true，未找到返回 false
bool cancel(TimerId id);

// 执行所有已到期的定时器；返回执行数量
size_t tick();

// 距离下一个到期的毫秒数；空堆返回 -1；已过期返回 0
int64_t next_timeout_ms() const;

size_t size() const;
```

### `class TimeWheelTimer`

```cpp
// slots 个槽，每槽 tick_ms 毫秒
explicit TimeWheelTimer(size_t slots = 60, uint64_t tick_ms = 1000);

TimerId add(uint64_t delay_ms, TimerCallback cb, uint64_t interval_ms = 0);
bool    cancel(TimerId id);

// 指针前进一格
size_t  tick();

// 按真实流逝时间推进若干格
size_t  advance();

size_t  size() const;
```

### `class TimerManager`

```cpp
// 初始化 epoll 与 eventfd；失败返回 false
bool init();

// 添加定时器；内部会唤醒 epoll
TimerId add_timer(uint64_t delay_ms, TimerCallback cb, uint64_t interval_ms = 0);

// 取消定时器
bool cancel_timer(TimerId id);

// 把普通 fd 也注册进同一个 epoll
bool add_fd(int fd, uint32_t events);
bool del_fd(int fd);

// 设置非定时器 fd 的回调
using FdCallback = std::function<void(int fd, uint32_t events)>;
void set_fd_callback(FdCallback cb);

// 阻塞运行事件循环
void run();

// 从其他线程或信号处理里唤醒并退出
void stop();
```

### 异常与错误处理

- 所有构造函数不抛异常；`TimerManager::init()` 通过返回值表示失败。
- `add_timer` / `cancel` 等**不抛异常**；`cancel` 找不到 id 时返回 `false`。
- 系统调用失败（`epoll_create1`、`eventfd`、`epoll_ctl`）通过返回值告知，不使用 `errno` 包装异常。

---

## 运行测试

```bash
cd build
./test_timer
```

预期输出：

```
[TEST] min_heap basic ... OK
[TEST] min_heap cancel ... OK
[TEST] min_heap repeat ... OK (count=5)
[TEST] time wheel ... OK
[TEST] time wheel wrap ... OK

All tests passed.
```

测试覆盖点：

| 测试 | 覆盖内容 |
| --- | --- |
| `min_heap basic` | 一次性定时器按时间顺序触发 |
| `min_heap cancel` | 取消后不再触发，且堆结构正确 |
| `min_heap repeat` | 周期定时器多次触发 |
| `time wheel` | 同一格范围内的多个定时器 |
| `time wheel wrap` | 需要多圈（rotations）的定时器 |

---

## 常见问题 FAQ

**Q1：`cmake ..` 报 `Could NOT find Threads`？**
A：安装 pthread 开发包：

```bash
sudo apt install -y libpthread-stubs0-dev
```

**Q2：编译报 `undefined reference to pthread_create`？**
A：CMake 里必须链接 `Threads::Threads`，本项目 `CMakeLists.txt` 已包含：

```cmake
find_package(Threads REQUIRED)
target_link_libraries(timer PUBLIC Threads::Threads)
```

**Q3：定时器不触发？**
A：检查是否调用了 `tick()`（或使用 `TimerManager::run()`）。

**Q4：周期定时器一次触发了很多次？**
A：如果你自己改过重排逻辑，确保用 `now + interval` 而不是 `expire + interval`，避免「追赶堆积」。

**Q5：`cancel` 之后堆结构乱了？**
A：检查 `swap_node` 是否同步更新了 `index_` 映射。本项目已在 `swap_node` 中同步。

**Q6：`eventfd` 读阻塞 / 事件循环卡死？**
A：`eventfd` 必须带 `EFD_NONBLOCK`。本项目已设置。

**Q7：`epoll_wait` 超时传了很大的数？**
A：`epoll_wait` 的 `timeout` 是 `int`，必须做 `INT_MAX` 截断。本项目已处理。

**Q8：Windows 能编译吗？**
A：不行，本项目依赖 Linux 专有接口。请用 WSL2。

**Q9：为什么不用 `system_clock`？**
A：`system_clock` 会被 `ntpdate` / 手动改时间影响，导致定时器早触发或晚触发。`steady_clock` 是单调递增的。

**Q10：时间轮为什么不用 `ceil` 用整除？**
A：`delay / tick` 会向下取整导致**早触发**。因此必须 `(delay + tick - 1) / tick`。

---

## 贡献指南

### 提交 Issue

请在 Issue 中提供：

1. 操作系统与编译器版本（`uname -a`、`g++ --version`）。
2. 完整复现步骤。
3. 期望行为 vs 实际行为。
4. 相关日志（`dmesg`、gdb backtrace、CMake 完整输出）。

### 提交 Pull Request

1. Fork 本仓库 → 新建分支：`git checkout -b feature/xxx`。
2. 保证 `cmake .. && make -j && ./test_timer` 全部通过。
3. 新功能需包含对应的单元测试。
4. 代码风格：
   - C++17，不使用 GNU 扩展。
   - 4 空格缩进，头文件 `#pragma once`。
   - 命名：类 `CamelCase`，函数/变量 `snake_case`，成员变量带尾下划线 `_`。
   - 不引入新的第三方依赖。
   - 所有系统调用必须检查返回值并处理 `EINTR`。
5. 提交信息遵循 [Conventional Commits](https://www.conventionalcommits.org/)：`feat: ...`、`fix: ...`、`docs: ...`。
6. PR 描述中说明改动动机、影响范围、测试方式。

### 目录约定

- `include/` 只放公开头文件。
- `src/` 只放实现，不暴露非必要符号。
- 新增数据结构请提供 `tests/test_<name>.cpp` 并在 CMake 里注册。

---

## 许可证 License

本项目基于 **MIT License** 开源，可自由用于商业与非商业场景，只需保留版权声明。

```
MIT License

Copyright (c) 2026 MisakaMikoto-QAQ

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 作者与联系方式

| 项目 | 信息 |
| --- | --- |
| 作者 | MisakaMikoto-QAQ |
| 邮箱 | 3128204593@qq.com |
| GitHub | [MisakaMikoto-QAQ](https://github.com/MisakaMikoto-QAQ) |


欢迎 Star / Fork / Issue / PR。

---

## 致谢 / 参考

- [Linux Programmer's Manual — epoll(7)](https://man7.org/linux/man-pages/man7/epoll.7.html)
- [Linux Programmer's Manual — eventfd(2)](https://man7.org/linux/man-pages/man2/eventfd.2.html)
- [Linux Programmer's Manual — timerfd_create(2)](https://man7.org/linux/man-pages/man2/timerfd_create.2.html)
- libevent 源码 `event.c` / `minheap-internal.h`
- nginx 源码 `src/event/ngx_event_timer.c`
- Netty `io.netty.util.HashedWheelTimer`
- 陈皓《Linux 多线程服务端编程：使用 muduo C++ 网络库》相关章节
- C++ 标准库参考：[cppreference.com](https://en.cppreference.com/)

---

## 更新日志 / 路线图

### v1.0.0（当前）

- ✅ 实现 `MinHeapTimer`（最小堆，O(log n) 增删）
- ✅ 实现 `TimeWheelTimer`（时间轮，O(1) 插入 + 多圈）
- ✅ 实现 `TimerManager`（epoll + eventfd 驱动）
- ✅ 提供单元测试与示例
- ✅ 提供 CMake 构建

### 路线图（Roadmap）

- [ ] **v1.1**：`TimerManager` 支持 `timerfd` 作为统一超时源，减少超时计算误差
- [ ] **v1.2**：提供**分层时间轮**（Hierarchical Timing Wheel），进一步降低多圈场景下的误差
- [ ] **v1.3**：时间轮的 `cancel` 优化为 O(1)（双向链表 + 哈希迭代器）
- [ ] **v1.4**：支持**多种时间轮精度**（毫秒轮 + 秒轮 + 分轮）
- [ ] **v1.5**：提供基准测试（Benchmark），对比最小堆 / 时间轮在不同负载下的表现
- [ ] **v2.0**：多线程版本，`TimerManager` 支持多 Reactor

---

<p align="center">
  <sub>Built with ❤️ for learning and production.</sub>
</p>