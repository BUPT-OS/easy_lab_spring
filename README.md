# OS101 锁与同步实验

本实验项目实现了多种锁和同步原语，用于学习和理解操作系统中的并发控制机制。

- **截止时间**：2026.5.21 23:59
- **助教邮箱**：`cyxrust@163.com`

## 目录结构

```
os101-spring-lock/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 本文件
├── include/
│   └── my/
│       ├── atomic.hpp          # 使用内联汇编实现的原子类型
│       ├── spin_lock.hpp       # 自旋锁
│       ├── ticket_spin_lock.hpp # Ticket 自旋锁（公平锁）
│       ├── mcs_lock.hpp        # MCS 锁（基于链表的自旋锁）
│       └── semaphore.hpp       # 使用 futex 实现的信号量
├── tests/
│   ├── test_atomic.cpp         # atomic 测试
│   ├── test_spin_lock.cpp      # 自旋锁测试
│   ├── test_semaphore.cpp      # 信号量测试
│   ├── test_ticket_spin_lock.cpp # Ticket 锁测试
│   ├── test_mcs_lock.cpp       # MCS 锁测试
│   └── benchmark_locks.cpp     # 三种锁的性能对比测试
└── examples/
    ├── producer_consumer.cpp   # 生产者-消费者模型
    └── dining_philosophers.cpp # 哲学家进餐问题
```

## 编译与运行

### 环境要求

- Linux 操作系统（使用了 futex 系统调用）
- GCC 或 Clang（支持 C++17）
- CMake 3.14 或更高版本
- x86_64 或 ARM 架构

### 编译步骤

```bash
# 创建构建目录
mkdir build && cd build

# 生成构建文件
cmake ..

# 编译所有目标
make -j$(nproc)
```

### 运行测试

```bash
# 运行单个测试
./test_atomic
./test_spin_lock
./test_semaphore
./test_ticket_spin_lock
./test_mcs_lock

# 运行所有测试
make run_all_tests
```

### 运行示例

```bash
# 生产者-消费者模型
./producer_consumer

# 哲学家进餐问题
./dining_philosophers
```

### 运行性能对比测试

```bash
# 三种锁的性能对比
./benchmark_locks
```

## 实验内容

### 基础版

#### 1. my::atomic

使用内联汇编实现 4 字节的原子类型，支持以下操作：
- `load()`: 原子读取
- `store()`: 原子写入
- `exchange()`: 原子交换
- `compare_exchange_strong()`: CAS 操作
- `fetch_add()` / `fetch_sub()`: 原子加减

**关键指令（x86）**:
- `xchg`: 原子交换（隐含 lock 前缀）
- `lock cmpxchg`: 原子比较并交换
- `lock xadd`: 原子加法并返回旧值

#### 2. my::spin_lock

使用 `my::atomic` 实现的自旋锁：
- 使用 TAS (Test-And-Set) 策略
- 包含 `pause` 指令优化（x86）或 `yield` 指令（ARM）
- 实现了 `lock()`, `unlock()`, `try_lock()`

**pause 指令的作用**:
1. 提示 CPU 当前处于自旋等待状态
2. 避免内存序违规导致的流水线清空
3. 降低功耗
4. 让出流水线资源给其他超线程

#### 3. my::semaphore

使用 Linux `sys_futex` 实现的信号量：
- `wait()` / `P 操作`: 如果值 > 0 则减 1，否则阻塞
- `signal()` / `V 操作`: 值加 1，并唤醒等待线程

**futex 的优势**:
- 无竞争时完全在用户空间操作，无系统调用开销
- 有竞争时才陷入内核进行等待/唤醒

#### 4. 生产者-消费者模型

使用三个信号量解决：
- `empty`: 空闲槽位数量
- `full`: 已填充槽位数量
- `mutex`: 保护缓冲区的互斥访问

#### 5. 哲学家进餐问题

使用资源有序分配避免死锁：
- 总是先拿编号较小的筷子
- 打破循环等待条件

### 进阶版

#### 1. Ticket Spin Lock（公平自旋锁）

解决简单自旋锁的不公平问题：
- 类似银行取号排队
- `next_ticket`: 下一个要发放的号码
- `now_serving`: 当前正在服务的号码
- 保证 FIFO 顺序

**缺点**: Thundering Herd 问题 - 所有等待线程都在同一个变量上自旋

#### 2. MCS Lock（链表自旋锁）

解决 Ticket Lock 的 Cache Line Bouncing 问题：
- 每个线程在自己的本地变量上自旋
- 维护隐式的 FIFO 队列（链表）
- 释放锁时只影响后继节点

**优势**:
1. 避免 Thundering Herd
2. 减少 Cache 争用
3. 保持公平性（FIFO）

## 学生填写区域

代码中使用 `// ==================== 学生填写区域 开始 ====================` 和 `// ==================== 学生填写区域 结束 ====================` 标记了需要学生完成的部分。

主要需要填写的内容：
1. `my::atomic`: 内联汇编实现原子操作
2. `my::spin_lock`: 使用 atomic 实现锁的获取和释放
3. `my::semaphore`: 使用 futex 实现 wait 和 signal
4. `my::ticket_spin_lock`: 实现公平的自旋锁
5. `my::mcs_lock`: 实现基于链表的自旋锁
6. 生产者-消费者: 使用信号量协调
7. 哲学家进餐: 实现筷子的获取和释放

## 性能对比测试

`benchmark_locks` 程序对比了三种锁的性能，包括以下测试：

### 测试项目

1. **吞吐量测试**: 不同线程数下的锁操作耗时
2. **公平性测试**: 各线程获取锁次数的变异系数
3. **高竞争场景**: 8 线程，临界区内有额外工作
4. **低竞争场景**: 4 线程，临界区外有较长工作时间

### 典型测试结果

#### 吞吐量测试 (每线程 100000 次操作)

| 线程数 | spin_lock | ticket_lock | mcs_lock | 最快 |
|--------|-----------|-------------|----------|------|
| 1 | ~1 ms | ~1.5 ms | ~1.7 ms | spin_lock |
| 4 | ~40 ms | ~70 ms | ~64 ms | spin_lock |
| 8 | ~200 ms | ~170 ms | ~220 ms | ticket_lock |
| 16 | ~540 ms | ~1070 ms | ~300 ms | **mcs_lock** |

#### 公平性测试 (变异系数 CV，越小越公平)

| 锁类型 | 变异系数 (CV) |
|--------|---------------|
| spin_lock | ~0.06 |
| ticket_spin_lock | ~0.04 |
| mcs_lock | ~0.03 |

#### 高竞争场景测试 (8 线程)

| 锁类型 | 耗时 |
|--------|------|
| spin_lock | ~150 ms |
| ticket_spin_lock | ~260 ms |
| **mcs_lock** | **~80 ms** |

### 结论

| 锁类型 | 优点 | 缺点 | 适用场景 |
|--------|------|------|----------|
| spin_lock | 实现简单，低竞争性能好 | 不公平，可能饥饿 | 低竞争，短临界区 |
| ticket_spin_lock | FIFO 公平，无饥饿 | Cache Line Bouncing | 需要公平性保证 |
| mcs_lock | 公平，高竞争性能最优 | 实现复杂，需额外存储 | 高竞争，多核系统 |

## 知识点总结

### 原子操作

| 操作 | x86 指令 | 作用 |
|------|----------|------|
| load | mov | 原子读取 |
| store | mov | 原子写入 |
| exchange | xchg | 原子交换 |
| CAS | lock cmpxchg | 比较并交换 |
| fetch_add | lock xadd | 原子加法 |

### 锁的演进

```
简单自旋锁 (TAS)
    ↓ 解决公平性问题
Ticket Spin Lock
    ↓ 解决 Cache Line Bouncing
MCS Lock
```

### futex 系统调用

```c
// 如果 *addr == val，则睡眠
futex(addr, FUTEX_WAIT, val);

// 唤醒最多 n 个等待线程
futex(addr, FUTEX_WAKE, n);
```

## 参考资料

1. Intel x86 指令参考手册
2. Linux futex 手册页: `man futex`
3. MCS Lock 论文: "Algorithms for Scalable Synchronization on Shared-Memory Multiprocessors" (1991)
4. 操作系统概念（第10版）- 进程同步章节

## 许可证

本项目仅用于教学目的。
