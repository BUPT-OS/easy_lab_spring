# Lab 4: 虚拟内存模拟器 (VM Simulator)

## 实验简介

本实验通过**纯用户态程序**来模拟 Linux 内核中虚拟内存管理子系统的核心机制。你将在不依赖真实硬件 MMU 的情况下，实现一个完整的虚拟内存系统，深入理解操作系统如何管理进程的地址空间、如何处理缺页异常、以及如何优化内存使用效率。

## 一、实验目的

通过完成本实验，你将：

1. **理解虚拟地址空间抽象**：掌握 `mm_struct` 和 `vm_area_struct` 的设计原理
2. **掌握多级页表机制**：实现 4 级页表的地址翻译过程（9+9+9+9+12 位）
3. **理解缺页异常处理**：实现完整的 page fault 处理路径
4. **区分匿名页与文件页**：理解两种内存映射的差异
5. **实现写时复制（COW）**：掌握 fork 时的内存优化技术
6. **理解页面回收机制**：（进阶）实现基于 LRU 的页面回收算法

## 二、背景知识

### 2.1 Linux 虚拟内存概述

在 Linux 中，每个进程都有独立的虚拟地址空间。内核通过以下核心数据结构来管理这些地址空间：

```
+------------------------------------------------------------------+
|                     Process Virtual Address Space                 |
|  ┌─────────────────────────────────────────────────────────────┐ |
|  │  Stack (grows downward)                                      │ |
|  │  ...                                                         │ |
|  │  Memory Mapped Files / Shared Libraries                      │ |
|  │  ...                                                         │ |
|  │  Heap (grows upward)                                         │ |
|  │  BSS (Uninitialized Data)                                    │ |
|  │  Data (Initialized Data)                                     │ |
|  │  Text (Code)                                                 │ |
|  └─────────────────────────────────────────────────────────────┘ |
+------------------------------------------------------------------+
```

### 2.2 关键数据结构

| 数据结构 | 说明 | 对应本实验 |
|---------|------|-----------|
| `mm_struct` | 进程内存描述符，管理整个地址空间 | `struct mm_struct` |
| `vm_area_struct` | 虚拟内存区域（VMA），描述一段连续的虚拟地址范围 | `struct vm_area_struct` |
| 页表 | 多级页表结构，用于虚拟地址到物理地址的映射 | 4 级页表（PGD/PUD/PMD/PTE） |
| `struct page` | 物理页框描述符 | `struct page` |

### 2.3 缺页异常处理流程

当 CPU 访问一个虚拟地址时，如果页表中没有对应的映射（或权限不足），会触发缺页异常（Page Fault）：

```
访问虚拟地址
     │
     ▼
┌─────────────┐
│  MMU 翻译   │───── 成功 ────▶ 访问物理内存
└─────────────┘
     │ 失败
     ▼
┌─────────────┐
│  Page Fault │
└─────────────┘
     │
     ▼
┌─────────────┐     无 VMA
│  查找 VMA   │──────────────▶ SIGSEGV（段错误）
└─────────────┘
     │ 找到
     ▼
┌─────────────┐     权限不符
│  检查权限   │──────────────▶ SIGSEGV（段错误）
└─────────────┘
     │ 通过
     ▼
┌─────────────┐
│  检查 PTE   │
└─────────────┘
     │
     ├─── 不存在 ────▶ Demand Paging（按需分页）
     │                 ├─ 匿名页：分配零页
     │                 └─ 文件页：从文件读取
     │
     └─── 存在但只读 ─▶ 检查是否 COW
                       ├─ 是：复制页面，更新 PTE
                       └─ 否：SIGSEGV
```

### 2.4 推荐参考资料

- **书籍**
  - 《深入理解 Linux 内核》（Understanding the Linux Kernel）- 第 8、9 章
  - 《Linux 内核设计与实现》（Linux Kernel Development）- 第 15 章
  - 《Operating Systems: Three Easy Pieces》- 虚拟内存章节（免费在线阅读）

- **Linux 内核源码**
  - `mm/memory.c` - 缺页异常处理
  - `mm/mmap.c` - VMA 管理
  - `include/linux/mm_types.h` - 核心数据结构定义

- **在线资源**
  - [Linux Memory Management Documentation](https://docs.kernel.org/mm/index.html)
  - `/proc/[pid]/maps` - 查看进程的 VMA 布局
  - `/proc/[pid]/pagemap` - 查看页表映射关系

## 三、实验框架

### 3.1 项目结构

```
lab4_vm_simulator/
├── Makefile                 # 编译脚本
├── README.md                # 本文档
├── include/
│   └── vm_defs.h            # 核心数据结构和 API 声明
├── src/
│   ├── phys.c               # 物理内存模拟（页帧分配器）
│   ├── pt.c                 # 页表操作（4 级页表）
│   ├── mm.c                 # VMA 管理（mmap/munmap）
│   ├── core.c               # 核心逻辑（MMU 翻译、缺页处理）
│   └── main.c               # 测试入口
├── tests/                   # 测试用例
└── test.py                  # 测试用的脚本, python3 test.py运行测试
```

项目提供了`.devcontainers`文件夹，vscode可以使用插件`Dev container`在容器中打开这个文件夹。容器中具有了实验需要的环境，不需要额外配置。容器内使用`clangd`作为语言服务器高亮代码和提供跳转，使用`bear -- make tests`来劫持编译过程生成`compile_commands.json`文件。

### 3.2 核心数据结构

#### 页表项（PTE）

```c
// 64 位页表项
// [63:N] 保留 | [M:12] PFN（页帧号）| [11:0] 标志位
typedef uint64_t pte_t;

// 标志位定义
#define PTE_P   0x001   // Present（存在位）
#define PTE_W   0x002   // Writable（可写位）
#define PTE_U   0x004   // User（用户态可访问）
#define PTE_A   0x020   // Accessed（已访问）
#define PTE_D   0x040   // Dirty（已修改）
```

#### 物理页描述符

```c
struct page {
    uint32_t refcount;       // 引用计数
    uint32_t flags;          // 页面标志
    struct page *lru_next;   // LRU 链表
    struct page *lru_prev;
};
```

#### 虚拟内存区域（VMA）

```c
struct vm_area_struct {
    uint64_t vm_start;       // 起始地址
    uint64_t vm_end;         // 结束地址
    uint32_t vm_prot;        // 权限（R/W/X）
    uint32_t vm_flags;       // 标志（匿名/文件/共享）
    char *vm_file_path;      // 文件映射路径
    uint64_t vm_pgoff;       // 文件偏移
    // 双向链表
    struct vm_area_struct *vm_next;
    struct vm_area_struct *vm_prev;
};
```

#### 进程内存描述符

```c
struct mm_struct {
    pte_t *pgd;                      // 页全局目录（顶级页表）
    struct vm_area_struct *mmap;     // VMA 链表
    struct vm_area_struct *mmap_cache; // VMA 缓存（优化查找）
};
```

### 3.3 核心 API

| 函数 | 文件 | 功能 |
|-----|------|------|
| `phys_mem_init()` | phys.c | 初始化物理内存模拟器 |
| `alloc_page()` | phys.c | 分配一个物理页 |
| `free_page(page)` | phys.c | 释放一个物理页 |
| `pt_walk(mm, addr, alloc)` | pt.c | 遍历页表，获取 PTE 指针 |
| `pt_map_page(mm, addr, pfn, flags)` | pt.c | 建立虚拟地址到物理页的映射 |
| `find_vma(mm, addr)` | mm.c | 查找包含指定地址的 VMA |
| `do_mmap(...)` | mm.c | 创建新的虚拟内存映射 |
| `handle_page_fault(mm, addr, type)` | core.c | 处理缺页异常 |
| `mmu_translate(mm, addr, type, paddr)` | core.c | 模拟 MMU 地址翻译 |
| `vm_read(mm, addr, buf, len)` | core.c | 从虚拟地址读取数据 |
| `vm_write(mm, addr, buf, len)` | core.c | 向虚拟地址写入数据 |

### 3.4 编译与运行

```bash
# 编译所有测试（不运行）
make tests

# 运行特定阶段的测试
make test1    # Phase 1: 最小可运行 VM
make test2    # Phase 2: 多 VMA + 权限
make test3    # Phase 3: mmap 语义
make test4    # Phase 4: fork + COW
make test5    # Phase 5: Page Cache（进阶）
make test6    # Phase 6: 内存压力与回收（进阶）

# 运行所有测试
make test

# 清理
make clean
```

## 四、实验任务

本实验分为 **6 个阶段**，前 4 个阶段为**基础任务**（80 分），后 2 个阶段为**进阶任务**（20 分）。

---

### Phase 1：最小可运行 VM（15 分）

#### 目标
实现一个最简单的虚拟内存系统：单个 VMA、单级页表、单个匿名页。

---

### Phase 2：多 VMA + 权限检查（15 分）

#### 目标
支持多个 VMA，实现权限检查机制。

---

### Phase 3：mmap 语义实现（20 分）

#### 目标
完整实现 `mmap` 的语义，包括匿名映射和文件映射。

---

### Phase 4：fork + 写时复制（COW）（30 分）

#### 目标
实现 fork 系统调用的内存语义，包括写时复制机制。

#### COW 处理流程
```
写 fault 触发
     │
     ▼
┌───────────────────┐
│ 检查 VMA 权限     │──── 不可写 ────▶ SIGSEGV
└───────────────────┘
     │ 可写
     ▼
┌───────────────────┐
│ 检查 PTE          │──── 已可写 ────▶ 正常写入
└───────────────────┘
     │ 只读（COW）
     ▼
┌───────────────────┐
│ 检查 refcount     │──── == 1 ─────▶ 直接改为可写
└───────────────────┘
     │ > 1
     ▼
┌───────────────────┐
│ 分配新页 + 复制   │
│ 更新 PTE 为可写   │
│ 原页 refcount--   │
└───────────────────┘
```

---

### Phase 5：Page Cache（10 分）🚀 进阶

#### 目标
实现 Page Cache，使多个进程映射同一文件时共享物理页。

---

### Phase 6：内存压力与页面回收（10 分）🚀 进阶

#### 目标
实现基于 LRU 的页面回收机制，处理内存不足的情况。

#### LRU 简化模型
```
┌──────────────────────────────────────────┐
│              Active List                  │
│  (最近访问过的页面)                        │
└──────────────────────────────────────────┘
                    │ 老化
                    ▼
┌──────────────────────────────────────────┐
│             Inactive List                 │
│  (候选回收页面)                           │
└──────────────────────────────────────────┘
                    │ 回收
                    ▼
              释放物理页
```

## 六、评分规则

| 阶段 | 分值 | 评分标准 |
|------|------|---------|
| Phase 1 | 15 分 | 通过 `test_phase1` 测试 |
| Phase 2 | 15 分 | 通过 `test_phase2` 测试 |
| Phase 3 | 20 分 | 通过 `test_phase3` 测试 |
| Phase 4 | 30 分 | 通过 `test_phase4` 测试 |
| Phase 5 | 10 分 | 通过 `test_phase5` 测试（进阶） |
| Phase 6 | 10 分 | 通过 `test_phase6` 测试（进阶） |
| **总计** | **100 分** | |

### 评分说明

- **基础任务（Phase 1-4）**：80 分，要求所有同学完成
- **进阶任务（Phase 5-6）**：20 分，供学有余力的同学挑战
- 每个阶段通过对应的功能测试即可得分
- 代码需要有适当的注释，逻辑清晰

## 七、提交要求

- 完整的源代码，用一个压缩包存储整个项目，压缩包中具有一个文件夹，文件夹名字叫`lab4_vm_simulator`，文件夹下面是整个项目。
- 不要提交编译生成的文件（`.o`、可执行文件等），压缩之前运行一下`make clean`
- 保持原有文件结构不变
- 提交的项目需要能够运行`python3 test.py`或者`make test`进行测试，测试系统依赖此。

## 八、常见问题

### Q1: 为什么 mmap 后立即访问会触发 Page Fault？

这就是**懒分配（Lazy Allocation）** 或 **Demand Paging** 的核心思想。`mmap` 只是建立了虚拟地址到后备存储（文件或匿名）的映射关系，并没有实际分配物理内存。只有当程序真正访问这个地址时，才会触发 Page Fault，内核才会分配物理页并建立页表映射。

### Q2: COW 的 refcount 什么时候减少？

当发生 COW fault 时：
1. 分配新的物理页
2. 复制原页面内容到新页
3. 更新 PTE 指向新页
4. 原页面的 `refcount--`

如果原页面 `refcount` 变为 0，则可以回收该页面。

### Q3: 如何区分匿名页和文件页的 fault 处理？

在 `handle_page_fault()` 中：
- 如果 VMA 带有 `VM_ANON` 标志 → 分配零页
- 如果 VMA 有 `vm_file_path` → 从文件读取内容填充页面

### Q4: 4 级页表的索引如何计算？

对于 64 位虚拟地址（实际使用 48 位）：
```
63    48 47    39 38    30 29    21 20    12 11     0
+-------+--------+--------+--------+--------+--------+
| Sign  |  PGD   |  PUD   |  PMD   |  PTE   | Offset |
|  Ext  | Index  | Index  | Index  | Index  |        |
+-------+--------+--------+--------+--------+--------+
   16       9        9        9        9       12
```

每级页表有 512 (2^9) 个条目，页大小为 4KB (2^12)。

---

**祝你实验顺利！如有问题，请及时联系助教或在讨论区提问。**
