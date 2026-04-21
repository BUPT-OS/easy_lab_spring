/**
 * @file malloc_internal.h
 * @brief 自定义内存分配器 - 内部数据结构和辅助函数
 * 
 * 本文件定义了内存分配器的内部实现细节。
 * 学生可以参考这些数据结构，也可以根据需要修改。
 */

#ifndef MALLOC_INTERNAL_H
#define MALLOC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 配置常量
 *============================================================================*/

/* 内存对齐要求（必须是 2 的幂） */
#define ALIGNMENT 16

/* 最小块大小（必须能容纳头部 + 最小有效载荷） */
#define MIN_BLOCK_SIZE (sizeof(block_header_t) + ALIGNMENT)

/* 堆扩展的最小单位（4KB，一页） */
#define HEAP_EXTEND_MIN 4096

/* 使用 mmap 的阈值（大于此值使用 mmap 而非 sbrk） */
#define MMAP_THRESHOLD (128 * 1024)

/* 魔数，用于检测内存损坏 */
#define BLOCK_MAGIC 0xDEADBEEF

/*============================================================================
 * 数据结构定义
 *============================================================================*/

/**
 * @brief 内存块头部结构
 * 
 * 每个分配的内存块前面都有这个头部。
 * 头部存储了块的元数据，用于管理和释放内存。
 * 
 *  +------------------+
 *  |   block_header   |  <- 元数据
 *  +------------------+
 *  |                  |
 *  |     payload      |  <- 用户使用的内存区域
 *  |                  |
 *  +------------------+
 */
typedef struct block_header {
    uint32_t magic;              /* 魔数，用于检测损坏和验证指针 */
    uint32_t is_free;            /* 是否空闲：1 = 空闲，0 = 已分配 */
    size_t size;                 /* 整个块的大小（包含头部） */
    struct block_header *next;   /* 下一个块（物理顺序或空闲链表） */
    struct block_header *prev;   /* 上一个块（用于合并相邻空闲块） */
} block_header_t;

/**
 * @brief 堆管理器状态
 * 
 * 全局状态，跟踪堆的整体信息。
 */
typedef struct {
    block_header_t *heap_start;    /* 堆的起始地址 */
    block_header_t *heap_end;      /* 堆的结束地址 */
    block_header_t *free_list;     /* 空闲块链表头 */
    size_t total_heap_size;        /* 总堆大小 */
    size_t allocated_size;         /* 已分配的大小 */
    int initialized;               /* 是否已初始化 */
} heap_state_t;

/*============================================================================
 * 分配策略枚举
 *============================================================================*/

typedef enum {
    STRATEGY_FIRST_FIT,    /* 首次适应：找到第一个足够大的块 */
    STRATEGY_BEST_FIT,     /* 最佳适应：找到大小最接近的块 */
    STRATEGY_WORST_FIT     /* 最差适应：找到最大的块 */
} alloc_strategy_t;

/*============================================================================
 * 辅助宏定义
 *============================================================================*/

/* 将 size 向上对齐到 ALIGNMENT 的倍数 */
#define ALIGN_UP(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

/* 将 size 向下对齐到 ALIGNMENT 的倍数 */
#define ALIGN_DOWN(size) ((size) & ~(ALIGNMENT - 1))

/* 检查地址是否对齐 */
#define IS_ALIGNED(ptr) (((uintptr_t)(ptr) & (ALIGNMENT - 1)) == 0)

/* 从用户指针获取块头部 */
#define GET_HEADER(ptr) ((block_header_t *)((char *)(ptr) - sizeof(block_header_t)))

/* 从块头部获取用户指针 */
#define GET_PAYLOAD(header) ((void *)((char *)(header) + sizeof(block_header_t)))

/* 获取块的有效载荷大小 */
#define GET_PAYLOAD_SIZE(header) ((header)->size - sizeof(block_header_t))

/* 获取下一个物理相邻的块 */
#define NEXT_PHYSICAL_BLOCK(header) \
    ((block_header_t *)((char *)(header) + (header)->size))

/* 检查块是否有效 */
#define IS_VALID_BLOCK(header) ((header)->magic == BLOCK_MAGIC)

/*============================================================================
 * 内部函数声明（学生可以选择实现或修改）
 *============================================================================*/

/**
 * @brief 初始化堆
 * 
 * 在第一次调用 my_malloc 时自动调用。
 * 
 * @return 成功返回 0，失败返回 -1
 */
int heap_init(void);

/**
 * @brief 扩展堆
 * 
 * 使用 sbrk() 或 mmap() 增加堆的大小。
 * 
 * @param size 需要增加的字节数
 * @return 成功返回新内存的起始地址，失败返回 NULL
 */
void *heap_extend(size_t size);

/**
 * @brief 在空闲链表中查找合适的块
 * 
 * 根据当前的分配策略查找一个足够大的空闲块。
 * 
 * @param size 需要的块大小（包含头部）
 * @return 找到返回块指针，未找到返回 NULL
 */
block_header_t *find_free_block(size_t size);

/**
 * @brief 分割块
 * 
 * 如果块比需要的大得多，将其分割成两个块。
 * 
 * @param block 要分割的块
 * @param size 需要的大小
 */
void split_block(block_header_t *block, size_t size);

/**
 * @brief 合并相邻的空闲块
 * 
 * 将当前块与其物理相邻的空闲块合并。
 * 
 * @param block 当前块
 * @return 合并后的块指针
 */
block_header_t *coalesce(block_header_t *block);

/**
 * @brief 将块添加到空闲链表
 * 
 * @param block 要添加的块
 */
void add_to_free_list(block_header_t *block);

/**
 * @brief 从空闲链表中移除块
 * 
 * @param block 要移除的块
 */
void remove_from_free_list(block_header_t *block);

/**
 * @brief 设置分配策略
 * 
 * @param strategy 分配策略
 */
void set_alloc_strategy(alloc_strategy_t strategy);

/**
 * @brief 获取当前分配策略
 * 
 * @return 当前分配策略
 */
alloc_strategy_t get_alloc_strategy(void);

#ifdef __cplusplus
}
#endif

#endif /* MALLOC_INTERNAL_H */