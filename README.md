# Lab 4: 自定义内存分配器 (Custom Memory Allocator)

## 实验简介

本实验旨在帮助学生深入理解用户态内存管理机制，特别是 glibc 中 `malloc`/`free` 的工作原理。通过实现一个简化版的内存分配器，学生将掌握：

- 堆内存管理的基本概念
- 内存块的元数据管理
- 空闲块链表的维护
- 内存分配策略（First-Fit、Best-Fit、Worst-Fit）
- 内存碎片问题及解决方法（块分割与合并）

## 学习目标

完成本实验后，你将能够：

1. 解释 `malloc` 和 `free` 的底层工作原理
2. 理解内存分配器如何管理堆内存
3. 分析不同分配策略的优缺点
4. 实现基本的内存分配和释放功能
5. 处理内存碎片问题

## 目录结构

```
lab4_custom_malloc/
├── benchmark.sh    # 用于性能测试
├── docs
│   └── malloc_intro.md     # 介绍一下malloc
├── include     # 定义了有用的数据结构、宏和函数签名
│   ├── malloc_internal.h
│   └── my_malloc.h
├── Makefile
├── README.md
├── scripts     
│   ├── __pycache__
│   │   └── score_benchmark.cpython-312.pyc
│   └── score_benchmark.py  # 用于性能测试
├── src
│   └── my_malloc.c # 你需要修改的文件，实现malloc
└── tests
    └── test_functional.c # 功能测试文件
```

## 快速开始

### 1. 编译项目

我们提供了`.devcontainer`文件夹，在vscode中可以使用`dev container`插件将这个文件夹在容器中打开。按下`Ctrl + Shift + p`之后输入`reopen in container`然后选择`Dev Container: Reopen in Container`，等待片刻将会拉取镜像然后创建容器，在容器中打开这个文件夹。容器中具有实验所需的环境，不需要额外配置就可以开始实验。

> **注意：** 遇到镜像拉取问题请先自行尝试解决或者和同学讨论，比如通过Docker镜像站拉取或者配置VPN。如果实在无法解决再找助教。

```bash
# 编译功能测试test_funcitonal
make
# 编译成动态链接库
make lib
```

### 2. 运行测试

运行功能测试：

```bash
make test
```

运行性能测试：

1. 编译成动态链接库，得到`libmymalloc.so`

    ```bash
    make lib
    ```

2. 使用benchmark.sh下载glibc并编译，使用glibc自带的benchtests测试malloc，需要一段时间

    ```bash
    ./benchmark.sh --init --preload $PWD/libmymalloc.so
    ```

### 3. 清理构建产物

```bash
make clean
```

## 实验任务

### Level 1：基础实现（必做）

完成 `src/my_malloc.c` 中的以下函数：

| 函数 | 描述 | 难度 |
|------|------|------|
| `heap_init()` | 初始化堆 | ⭐ |
| `heap_extend()` | 扩展堆空间 | ⭐⭐ |
| `find_first_fit()` | First-Fit 查找算法 | ⭐⭐ |
| `add_to_free_list()` | 添加块到空闲链表 | ⭐⭐ |
| `remove_from_free_list()` | 从空闲链表移除块 | ⭐⭐ |
| `my_malloc()` | 分配内存 | ⭐⭐⭐ |
| `my_free()` | 释放内存 | ⭐⭐⭐ |

### Level 2：进阶实现（选做）

| 函数 | 描述 | 难度 |
|------|------|------|
| `split_block()` | 块分割 | ⭐⭐⭐ |
| `coalesce()` | 相邻空闲块合并 | ⭐⭐⭐ |

### Level 3：高级实现（挑战）

| 函数 | 描述 | 难度 |
|------|------|------|
| `find_best_fit()` | Best-Fit 查找算法 | ⭐⭐⭐ |
| `find_worst_fit()` | Worst-Fit 查找算法 | ⭐⭐⭐ |
| `my_calloc()` | 分配并清零内存 | ⭐⭐⭐ |
| `my_realloc()` | 重新调整内存大小 | ⭐⭐⭐⭐ |

## 实现提示

### 内存块结构

每个分配的内存块包含一个头部（header）和有效载荷（payload）：

```
+------------------+
|   block_header   |  <- 元数据（magic, size, is_free, prev, next）
+------------------+
|                  |
|     payload      |  <- 返回给用户的内存区域
|                  |
+------------------+
```

### 关键宏定义

```c
// 内存对齐
#define ALIGN_UP(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

// 从用户指针获取块头部
#define GET_HEADER(ptr) ((block_header_t *)((char *)(ptr) - sizeof(block_header_t)))

// 从块头部获取用户指针
#define GET_PAYLOAD(header) ((void *)((char *)(header) + sizeof(block_header_t)))
```

### 空闲链表管理

建议按地址顺序维护空闲链表，这样更容易实现相邻块的合并：

```
free_list -> [block A] -> [block C] -> [block E] -> NULL
               0x1000       0x2000       0x3000
```

## 评分标准

| 评分项 | 分值 | 说明 |
|--------|------|------|
| 功能正确性 | 60分 | 通过功能测试套件 |
| 吞吐量 | 20分 | 与系统 malloc 的性能比较 |
| 内存利用率 | 15分 | 有效载荷 / 总堆大小 |
| 代码质量 | 5分 | 代码规范、注释清晰 |

### 功能性评分细则

- Level 1 完成：可获得最高 40 分
- Level 2 完成：可获得额外 15 分
- Level 3 完成：可获得额外 5 分

### 性能评分细则

- 吞吐量达到系统 malloc 的 50% 以上：满分
- 吞吐量达到系统 malloc 的 30% 以上：15分
- 吞吐量达到系统 malloc 的 10% 以上：10分
- 吞吐量达到系统 malloc 的 5% 以上：5分

## 参考资料

1. [The GNU C Library - Memory Allocation](https://www.gnu.org/software/libc/manual/html_node/Memory-Allocation.html)
2. [A Memory Allocator by Doug Lea](http://gee.cs.oswego.edu/dl/html/malloc.html)
3. [CMU 15-213: Malloc Lab](http://csapp.cs.cmu.edu/3e/malloclab.pdf)

## 提交要求

1. 提交完整的压缩包文件，压缩包内有一个文件夹，名字叫lab4_custom_malloc，然后文件夹下面是实验的所有代码。
2. 提交的代码需要能运行`make test`来执行功能测试以及`make lib && ./benchmark --init --preload $PWD/libmymalloc.so`来执行性能测试，评分依赖此，严禁修改测试框架。
3. 附上实验报告，说明：（包括但不限于）
   - 你实现了哪些功能
   - 遇到的主要困难及解决方法
   - 对不同分配策略的理解
