# 内存分配器原理详解

## 1. 概述

`malloc` 和 `free` 是 C 语言中最常用的内存管理函数，它们由 C 标准库（如 glibc）提供。本文将详细介绍这些函数的底层工作原理。

## 2. 进程内存布局

一个典型的 Linux 进程内存布局如下：

```
高地址
┌─────────────────────┐
│       Stack         │  ← 栈（向下增长）
│         ↓           │
├─────────────────────┤
│                     │
│    (空闲区域)        │
│                     │
├─────────────────────┤
│         ↑           │
│       Heap          │  ← 堆（向上增长）
├─────────────────────┤
│        BSS          │  ← 未初始化的全局变量
├─────────────────────┤
│        Data         │  ← 初始化的全局变量
├─────────────────────┤
│        Text         │  ← 程序代码
└─────────────────────┘
低地址
```

**堆（Heap）** 是动态内存分配的区域，`malloc` 从这里分配内存。

## 3. 系统调用：sbrk() 和 mmap()

### 3.1 sbrk()

`sbrk()` 是最传统的堆扩展方式：

```c
#include <unistd.h>

void *sbrk(intptr_t increment);
```

- `sbrk(0)` - 返回当前 program break（堆顶）的地址
- `sbrk(n)` - 将 program break 增加 n 字节，返回原来的位置
- `sbrk(-n)` - 将 program break 减少 n 字节

**示例：**

```c
void *old_brk = sbrk(0);     // 获取当前堆顶
void *new_mem = sbrk(4096);  // 扩展 4KB
if (new_mem == (void *)-1) {
    // 扩展失败
}
// new_mem 指向新分配的 4KB 内存的起始位置
```

### 3.2 mmap()

对于大块内存，glibc 的 malloc 使用 `mmap()` 直接从操作系统获取内存：

```c
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
```

通常，当请求的内存大于 128KB 时，会使用 `mmap()` 而非 `sbrk()`。

## 4. 内存块结构

### 4.1 块头部（Block Header）

每个分配的内存块前面都有一个头部，存储元数据：

```
┌────────────────────────────────────┐
│           Block Header             │
│  ┌──────────┬──────────┬────────┐  │
│  │   size   │ is_free  │ magic  │  │
│  ├──────────┴──────────┴────────┤  │
│  │      prev      │     next    │  │
│  └──────────────────────────────┘  │
├────────────────────────────────────┤
│                                    │
│            Payload                 │
│     (用户可用的内存区域)             │
│                                    │
└────────────────────────────────────┘
```

### 4.2 头部字段说明

| 字段 | 大小 | 说明 |
|------|------|------|
| size | 8 bytes | 整个块的大小（包含头部） |
| is_free | 4 bytes | 标志：1 = 空闲，0 = 已分配 |
| magic | 4 bytes | 魔数，用于验证块的有效性 |
| prev | 8 bytes | 指向前一个块（用于合并） |
| next | 8 bytes | 指向下一个块（空闲链表） |

### 4.3 内存对齐

返回给用户的指针必须满足对齐要求：

- 32 位系统：8 字节对齐
- 64 位系统：16 字节对齐

对齐的原因：
1. 某些 CPU 指令要求数据对齐
2. 未对齐的访问可能导致性能下降或硬件异常

**对齐宏：**

```c
#define ALIGNMENT 16
#define ALIGN_UP(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

// 示例：
// ALIGN_UP(1)  = 16
// ALIGN_UP(16) = 16
// ALIGN_UP(17) = 32
```

## 5. 空闲链表管理

### 5.1 隐式空闲链表

最简单的方法是将所有块（无论空闲与否）按物理顺序链接：

```
heap_start                                              heap_end
    │                                                       │
    ▼                                                       ▼
┌────────┐   ┌────────┐   ┌────────┐   ┌────────┐   ┌────────┐
│ Block1 │──▶│ Block2 │──▶│ Block3 │──▶│ Block4 │──▶│ Block5 │
│ (used) │   │ (free) │   │ (used) │   │ (free) │   │ (used) │
└────────┘   └────────┘   └────────┘   └────────┘   └────────┘
```

**优点：** 实现简单
**缺点：** 查找空闲块需要遍历所有块，效率低

### 5.2 显式空闲链表

只将空闲块链接起来：

```
free_list
    │
    ▼
┌────────┐       ┌────────┐       ┌────────┐
│ Block2 │──────▶│ Block4 │──────▶│ Block7 │──▶ NULL
│ (free) │       │ (free) │       │ (free) │
└────────┘       └────────┘       └────────┘
```

**优点：** 查找只需遍历空闲块
**缺点：** 需要额外的指针空间

### 5.3 分离空闲链表（Segregated Free Lists）

按大小范围维护多个空闲链表：

```
Size Class    Free List
──────────    ─────────
  1-16    ──▶ [Block] ──▶ [Block] ──▶ NULL
 17-32    ──▶ [Block] ──▶ NULL
 33-64    ──▶ [Block] ──▶ [Block] ──▶ [Block] ──▶ NULL
 65-128   ──▶ NULL
129-256   ──▶ [Block] ──▶ NULL
  ...
```

**优点：** 快速找到合适大小的块
**缺点：** 实现复杂

## 6. 分配策略

### 6.1 First-Fit（首次适应）

从链表头开始，找到**第一个**足够大的空闲块。

伪代码如下：
```text
First-Fit(free_list, size):
    current ← free_list
    WHILE current ≠ NULL DO
        IF current.size ≥ size THEN
            RETURN current    // 找到足够大的块
        END IF
        current ← current.next
    END WHILE
    RETURN NULL    // 没有找到合适的块
```

**优点：** 简单快速
**缺点：** 可能在链表前端产生很多小碎片

### 6.2 Best-Fit（最佳适应）

遍历整个链表，找到**大小最接近**的空闲块。

伪代码如下：
```text
Best-Fit(free_list, size):
    current ← free_list
    best ← NULL
    best_size ← ∞
    WHILE current ≠ NULL DO
        IF current.size ≥ size AND current.size < best_size THEN
            best ← current
            best_size ← current.size
        END IF
        current ← current.next
    END WHILE
    RETURN best    // 返回最接近 size 的块，或 NULL
```

**优点：** 减少内存浪费
**缺点：** 需要遍历整个链表，较慢；可能产生很多小碎片

### 6.3 Worst-Fit（最差适应）

遍历整个链表，找到**最大的**空闲块。

伪代码如下：
```text
Worst-Fit(free_list, size):
    current ← free_list
    worst ← NULL
    worst_size ← 0
    WHILE current ≠ NULL DO
        IF current.size ≥ size AND current.size > worst_size THEN
            worst ← current
            worst_size ← current.size
        END IF
        current ← current.next
    END WHILE
    RETURN worst    // 返回最大的块，或 NULL
```

**优点：** 剩余空间较大，可能更有用
**缺点：** 大块很快被消耗完

### 6.4 策略比较

| 策略 | 时间复杂度 | 空间利用率 | 碎片化 |
|------|-----------|-----------|--------|
| First-Fit | O(n) 平均 | 中等 | 链表前端碎片多 |
| Best-Fit | O(n) | 较高 | 产生很多小碎片 |
| Worst-Fit | O(n) | 较低 | 大块消耗快 |

## 7. 块分割（Splitting）

当找到的空闲块比需要的大很多时，应该将其分割：

```
分割前：
┌─────────────────────────────────────┐
│              Free Block             │
│              size = 256             │
└─────────────────────────────────────┘

请求 64 字节后分割：
┌─────────────────┬───────────────────┐
│  Allocated      │    New Free       │
│  size = 80      │    size = 176     │
│  (64 + header)  │                   │
└─────────────────┴───────────────────┘
```

## 8. 块合并（Coalescing）

当释放一个块时，检查相邻块是否也空闲，如果是则合并：

```
合并前：
┌─────────┐ ┌─────────┐ ┌─────────┐
│  Free   │ │  Free   │ │  Free   │
│ Block A │ │ Block B │ │ Block C │
│ (freed) │ │         │ │         │
└─────────┘ └─────────┘ └─────────┘

合并后：
┌─────────────────────────────────────┐
│              Free Block             │
│         size = A + B + C            │
└─────────────────────────────────────┘
```

**合并的四种情况：**

1. 前后都不空闲 → 不合并
2. 只有后块空闲 → 与后块合并
3. 只有前块空闲 → 与前块合并
4. 前后都空闲 → 三块合并

## 9. glibc malloc 实现概述

glibc 的 malloc（ptmalloc2）使用了更复杂的机制：

### 9.1 Bins（箱子）

glibc 使用多种 bins 管理空闲块：

- **Fast Bins**: 小块（≤80字节），单链表，LIFO
- **Small Bins**: 中等块，双向链表，FIFO
- **Large Bins**: 大块，按大小排序
- **Unsorted Bin**: 新释放的块的临时存放处

### 9.2 Chunks

glibc 中的内存块称为 chunk：

```
┌──────────────────────────────────────┐
│ prev_size (如果前一个块空闲)            │
├──────────────────────────────────────┤
│ size | A | M | P                     │
│        │   │   └── PREV_INUSE bit    │
│        │   └────── IS_MMAPPED bit    │
│        └────────── NON_MAIN_ARENA bit│
├──────────────────────────────────────┤
│                                      │
│              Payload                 │
│                                      │
└──────────────────────────────────────┘
```

### 9.3 Arena

多线程支持：每个线程可以有自己的 arena（内存区域），减少锁竞争。

## 10. 常见问题和陷阱

### 10.1 内存泄漏（Memory Leak）

分配的内存没有被释放：

```c
void leak_example() {
    char *ptr = malloc(100);
    // 没有调用 free(ptr)
}  // ptr 丢失，100 字节泄漏
```

### 10.2 双重释放（Double Free）

同一块内存释放两次：

```c
char *ptr = malloc(100);
free(ptr);
free(ptr);  // 危险！未定义行为
```

### 10.3 使用已释放的内存（Use After Free）

```c
char *ptr = malloc(100);
free(ptr);
ptr[0] = 'A';  // 危险！内存已释放
```

### 10.4 缓冲区溢出（Buffer Overflow）

写入超出分配大小的数据：

```c
char *ptr = malloc(10);
strcpy(ptr, "This is a very long string");  // 溢出！
```

## 11. 调试工具

### 11.1 Valgrind

valgrind homepage: https://valgrind.org/

valgrind release: https://valgrind.org/downloads/current.html

在release的README文件中有valgrind的安装过程

检测内存错误：

```bash
valgrind --leak-check=full ./your_program
```

### 11.2 AddressSanitizer

编译时启用：

```bash
gcc -fsanitize=address -g your_program.c -o your_program
./your_program
```

启用`-fsanitize=address`需要`libasan.so`运行时库，这里asan是Address sanitize的简称。在fedora下运行`dnf install libasan`来安装`libasan.so`运行时库。其他发行版使用各自的包管理器安装该运行时库。

## 12. 总结

实现一个内存分配器需要考虑：

1. **正确性**：正确管理内存块，避免损坏
2. **效率**：快速分配和释放
3. **内存利用率**：减少内部和外部碎片
4. **可扩展性**：支持多线程

参考文献[4]对内存分配器的目标有详细描述。

通过本实验，你将亲手实现这些核心功能，深入理解用户态内存管理的本质。

## 参考文献

1. Computer Systems: A Programmer's Perspective, Chapter 9
2. The C Programming Language, Kernighan & Ritchie
3. glibc malloc source code: https://sourceware.org/git/?p=glibc.git
4. Doug Lea's malloc: http://gee.cs.oswego.edu/dl/html/malloc.html
