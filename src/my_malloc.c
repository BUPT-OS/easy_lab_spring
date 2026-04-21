/**
 * @file my_malloc.c
 * @brief 自定义内存分配器 - 学生实现文件
 * 
 * 【实验说明】
 * 本文件包含了需要学生实现的内存分配器函数。
 * 请根据注释提示，完成各个函数的实现。
 * 
 * 【实现提示】
 * 1. 使用 sbrk() 系统调用来扩展堆
 * 2. 每个分配的内存块需要有头部来记录元数据
 * 3. 使用链表来管理空闲块
 * 4. 注意内存对齐要求
 * 
 * 【难度等级】
 * - Level 1（必做）：实现基本的 my_malloc 和 my_free
 * - Level 2（选做）：实现块分割和合并
 * - Level 3（挑战）：实现 my_calloc 和 my_realloc
 */

#include <unistd.h>     /* sbrk() */
#include <string.h>     /* memset(), memcpy() */
#include <stdio.h>      /* printf() for debug */
#include <stdint.h>     /* uintptr_t */
#include <sys/mman.h>   /* mmap() - optional */

#include "my_malloc.h"
#include "malloc_internal.h"

/*============================================================================
 * 全局变量
 *============================================================================*/

/* 堆状态 */
static heap_state_t heap_state = {
    .heap_start = NULL,
    .heap_end = NULL,
    .free_list = NULL,
    .total_heap_size = 0,
    .allocated_size = 0,
    .initialized = 0
};

/* 当前分配策略，默认使用首次适应 */
static alloc_strategy_t current_strategy = STRATEGY_FIRST_FIT;

/*============================================================================
 * 辅助函数实现
 *============================================================================*/

/**
 * @brief 初始化堆
 * 
 * TODO: 学生实现
 * 
 * 实现提示：
 * 1. 使用 sbrk(0) 获取当前堆的位置
 * 2. 设置 heap_state 的初始状态
 * 3. 设置 initialized 标志
 * 
 * @return 成功返回 0，失败返回 -1
 */
int heap_init(void)
{
    if (heap_state.initialized) {
        return 0;  /* 已经初始化 */
    }
    
    /* ===== 在此处添加你的代码 ===== */
    
    /* 提示：获取当前 program break */
    void *current_brk = sbrk(0);
    if (current_brk == (void *)-1) {
        return -1;
    }
    
    /* TODO: 初始化 heap_state 的各个字段 */
    
    
    /* ===== 代码结束 ===== */
    
    heap_state.initialized = 1;
    return 0;
}

/**
 * @brief 扩展堆
 * 
 * TODO: 学生实现
 * 
 * 实现提示：
 * 1. 使用 sbrk(size) 来扩展堆
 * 2. 创建一个新的块头部
 * 3. 更新 heap_state
 * 
 * 注意：这个函数只扩展堆并返回新块，
 * 不会将新块添加到空闲链表中（由调用者决定）。
 * 
 * @param size 需要扩展的字节数
 * @return 成功返回新内存的起始地址，失败返回 NULL
 */
void *heap_extend(size_t size)
{
    /* 确保扩展大小至少为 HEAP_EXTEND_MIN */
    if (size < HEAP_EXTEND_MIN) {
        size = HEAP_EXTEND_MIN;
    }
    
    /* 对齐大小 */
    size = ALIGN_UP(size);
    
    /* ===== 在此处添加你的代码 ===== */
    
    /* TODO: 
     */
    
    return NULL;  /* 替换为你的实现 */
    
    /* ===== 代码结束 ===== */
}

/**
 * @brief 在空闲链表中查找合适的块（First-Fit 策略）
 * 
 * TODO: 学生实现
 * 
 * 实现提示：
 * 1. 遍历空闲链表
 * 2. 找到第一个大小足够的块
 * 
 * @param size 需要的块大小（包含头部）
 * @return 找到返回块指针，未找到返回 NULL
 */
static block_header_t *find_first_fit(size_t size)
{
    /* ===== 在此处添加你的代码 ===== */
    
    /* TODO:
     * 1. 从 heap_state.free_list 开始遍历
     * 2. 检查每个块的大小是否 >= size
     * 3. 找到则返回该块
     * 4. 遍历完未找到则返回 NULL
     */
    
    return NULL;  /* 替换为你的实现 */
    
    /* ===== 代码结束 ===== */
}

/**
 * @brief 在空闲链表中查找合适的块（Best-Fit 策略）
 * 
 * TODO: 学生实现（Level 3 进阶）
 * 
 * @param size 需要的块大小（包含头部）
 * @return 找到返回块指针，未找到返回 NULL
 */
static block_header_t *find_best_fit(size_t size)
{
    /* ===== 在此处添加你的代码 ===== */
    
    /* TODO:
     */
    
    return NULL;  /* 替换为你的实现 */
    
    /* ===== 代码结束 ===== */
}

/**
 * @brief 在空闲链表中查找合适的块（Worst-Fit 策略）
 * 
 * TODO: 学生实现（Level 3 进阶）
 * 
 * @param size 需要的块大小（包含头部）
 * @return 找到返回块指针，未找到返回 NULL
 */
static block_header_t *find_worst_fit(size_t size)
{
    /* ===== 在此处添加你的代码 ===== */
    
    /* TODO:
     */
    
    return NULL;  /* 替换为你的实现 */
    
    /* ===== 代码结束 ===== */
}

/**
 * @brief 根据当前策略查找空闲块
 */
block_header_t *find_free_block(size_t size)
{
    switch (current_strategy) {
        case STRATEGY_FIRST_FIT:
            return find_first_fit(size);
        case STRATEGY_BEST_FIT:
            return find_best_fit(size);
        case STRATEGY_WORST_FIT:
            return find_worst_fit(size);
        default:
            return find_first_fit(size);
    }
}

/**
 * @brief 分割块
 * 
 * TODO: 学生实现（Level 2）
 * 
 * @param block 要分割的块
 * @param size 需要的大小
 */
void split_block(block_header_t *block, size_t size)
{
    /* 检查剩余空间是否足够创建新块 */
    if (block->size < size + MIN_BLOCK_SIZE) {
        return;  /* 空间不足，不分割 */
    }
    
    /* ===== 在此处添加你的代码 ===== */
    
    /* TODO:
     */
    
    /* ===== 代码结束 ===== */
}

/**
 * @brief 合并相邻的空闲块
 * 
 * TODO: 学生实现（Level 2）
 * 
 * @param block 当前块
 * @return 合并后的块指针
 */
block_header_t *coalesce(block_header_t *block)
{
    /* ===== 在此处添加你的代码 ===== */
    
    /* TODO:
     */
    
    return block;  /* 替换为你的实现 */
    
    /* ===== 代码结束 ===== */
}

/**
 * @brief 将块添加到空闲链表
 * 
 * TODO: 学生实现
 * 
 * @param block 要添加的块
 */
void add_to_free_list(block_header_t *block)
{
    /* ===== 在此处添加你的代码 ===== */
    
    /* TODO:
     */
    
    /* ===== 代码结束 ===== */
}

/**
 * @brief 从空闲链表中移除块
 * 
 * TODO: 学生实现
 * 
 * @param block 要移除的块
 */
void remove_from_free_list(block_header_t *block)
{
    /* ===== 在此处添加你的代码 ===== */
    
    /* TODO:
     */
    
    /* ===== 代码结束 ===== */
}

/*============================================================================
 * 主要接口实现
 *============================================================================*/

/**
 * @brief 分配内存
 * 
 * TODO: 学生实现
 * 
 * 实现步骤：
 * 1. 处理特殊情况（size == 0）
 * 2. 初始化堆（如果需要）
 * 3. 计算实际需要的块大小（对齐 + 头部）
 * 4. 在空闲链表中查找合适的块
 * 5. 如果找到，从空闲链表移除，可能需要分割
 * 6. 如果没找到，扩展堆
 * 7. 返回有效载荷的指针
 * 
 * @param size 请求的字节数
 * @return 成功返回内存指针，失败返回 NULL
 */
void *my_malloc_impl(size_t size)
{
    /* 处理 size 为 0 的情况 */
    if (size == 0) {
        return NULL;
    }
    
    /* 初始化堆（如果尚未初始化） */
    if (!heap_state.initialized) {
        if (heap_init() != 0) {
            return NULL;
        }
    }
    
    /* ===== 在此处添加你的代码 ===== */
    
    /* TODO:
     */
    
    return NULL;  /* 替换为你的实现 */
    
    /* ===== 代码结束 ===== */
}

/**
 * @brief 释放内存
 * 
 * TODO: 学生实现
 * 
 * 实现步骤：
 * 1. 处理 NULL 指针
 * 2. 获取块头部
 * 3. 验证块的有效性
 * 4. 将块添加到空闲链表
 * 5. 尝试合并相邻空闲块
 * 6. 更新统计信息
 * 
 * @param ptr 要释放的内存指针
 */
void my_free_impl(void *ptr)
{
    /* 处理 NULL 指针 */
    if (ptr == NULL) {
        return;
    }
    
    /* ===== 在此处添加你的代码 ===== */
    
    /* TODO:
     */
    
    /* ===== 代码结束 ===== */
}

/**
 * @brief 分配并清零内存
 * 
 * TODO: 学生实现（Level 3）
 * 
 * @param nmemb 元素个数
 * @param size 每个元素的大小
 * @return 成功返回内存指针，失败返回 NULL
 */
void *my_calloc_impl(size_t nmemb, size_t size)
{
    /* ===== 在此处添加你的代码 ===== */
    
    /* TODO:
     */
    
    return NULL;  /* 替换为你的实现 */
    
    /* ===== 代码结束 ===== */
}

/**
 * @brief 重新分配内存
 * 
 * TODO: 学生实现（Level 3）
 * 
 * @param ptr 原内存指针
 * @param size 新大小
 * @return 成功返回新内存指针，失败返回 NULL
 */
void *my_realloc_impl(void *ptr, size_t size)
{
    /* 处理特殊情况 */
    if (ptr == NULL) {
        return my_malloc_impl(size);
    }
    
    if (size == 0) {
        my_free_impl(ptr);
        return NULL;
    }
    
    /* ===== 在此处添加你的代码 ===== */
    
    /* TODO:
     */
    
    return NULL;  /* 替换为你的实现 */
    
    /* ===== 代码结束 ===== */
}

/*============================================================================
 * 标准接口包装函数（当 USE_STANDARD_NAMES 定义时）
 *============================================================================*/

#ifdef USE_STANDARD_NAMES
void *malloc(size_t size)
{
    return my_malloc_impl(size);
}

void free(void *ptr)
{
    my_free_impl(ptr);
}

void *calloc(size_t nmemb, size_t size)
{
    return my_calloc_impl(nmemb, size);
}

void *realloc(void *ptr, size_t size)
{
    return my_realloc_impl(ptr, size);
}
#endif /* USE_STANDARD_NAMES */

/*============================================================================
 * 调试和统计函数
 *============================================================================*/

/**
 * @brief 获取堆统计信息
 */
void my_malloc_stats(heap_stats_t *stats)
{
    if (stats == NULL) {
        return;
    }
    
    stats->total_heap_size = heap_state.total_heap_size;
    stats->total_allocated = heap_state.allocated_size;
    stats->total_free = heap_state.total_heap_size - heap_state.allocated_size;
    
    /* 统计块数 */
    stats->num_blocks = 0;
    stats->num_free_blocks = 0;
    stats->largest_free_block = 0;
    
    /* 遍历空闲链表统计，添加循环保护 */
    block_header_t *current = heap_state.free_list;
    size_t max_iterations = 1000000;  /* 防止无限循环 */
    size_t iterations = 0;
    
    while (current != NULL && iterations < max_iterations) {
        stats->num_free_blocks++;
        if (GET_PAYLOAD_SIZE(current) > stats->largest_free_block) {
            stats->largest_free_block = GET_PAYLOAD_SIZE(current);
        }
        current = current->next;
        iterations++;
    }
}

/**
 * @brief 打印堆状态（调试用）
 */
void my_malloc_debug_print(void)
{
    printf("=== Heap Debug Info ===\n");
    printf("Initialized: %s\n", heap_state.initialized ? "yes" : "no");
    printf("Heap start: %p\n", (void *)heap_state.heap_start);
    printf("Heap end: %p\n", (void *)heap_state.heap_end);
    printf("Total heap size: %zu bytes\n", heap_state.total_heap_size);
    printf("Allocated size: %zu bytes\n", heap_state.allocated_size);
    
    printf("\n--- Free List ---\n");
    block_header_t *current = heap_state.free_list;
    int count = 0;
    while (current != NULL) {
        printf("Block %d: addr=%p, size=%zu, is_free=%d\n",
               count++, (void *)current, current->size, current->is_free);
        current = current->next;
    }
    printf("Total free blocks: %d\n", count);
    printf("=======================\n");
}

/**
 * @brief 设置分配策略
 */
void set_alloc_strategy(alloc_strategy_t strategy)
{
    current_strategy = strategy;
}

/**
 * @brief 获取当前分配策略
 */
alloc_strategy_t get_alloc_strategy(void)
{
    return current_strategy;
}