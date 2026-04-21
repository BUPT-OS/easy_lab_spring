/**
 * @file my_malloc.h
 * @brief 自定义内存分配器 - 公共接口
 * 
 * 本文件定义了学生需要实现的内存分配器接口。
 * 通过实现这些函数，学生将深入理解 glibc malloc 的工作原理。
 * 
 * 编译选项：
 *   - 默认：函数名为 my_malloc, my_free, my_calloc, my_realloc
 *   - 定义 USE_STANDARD_NAMES：函数名为 malloc, free, calloc, realloc
 *     用于编译成动态库替代 glibc malloc
 */

#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 内部实现函数声明
 * 
 * 这些是实际的实现函数，使用 _impl 后缀避免与标准库冲突。
 * 无论是否定义 USE_STANDARD_NAMES，内部实现函数名保持不变。
 *============================================================================*/

void *my_malloc_impl(size_t size);
void my_free_impl(void *ptr);
void *my_calloc_impl(size_t nmemb, size_t size);
void *my_realloc_impl(void *ptr, size_t size);

/*============================================================================
 * 公共接口
 * 
 * 当定义 USE_STANDARD_NAMES 时，提供标准 C 库兼容的函数名，
 * 这样编译出的动态库可以通过 LD_PRELOAD 替代 glibc 的 malloc。
 *============================================================================*/

#ifdef USE_STANDARD_NAMES

/* 使用标准名称时，直接定义标准函数 */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

/* 为了兼容性，也提供 my_* 别名指向内部实现 */
#define my_malloc  my_malloc_impl
#define my_free    my_free_impl
#define my_calloc  my_calloc_impl
#define my_realloc my_realloc_impl

#else

/* 不使用标准名称时，my_* 函数指向内部实现 */
#define my_malloc  my_malloc_impl
#define my_free    my_free_impl
#define my_calloc  my_calloc_impl
#define my_realloc my_realloc_impl

#endif /* USE_STANDARD_NAMES */

/*============================================================================
 * 统计和调试接口
 *============================================================================*/

/**
 * @brief 获取当前堆的统计信息
 * 
 * 用于调试和性能测试。
 */
typedef struct {
    size_t total_heap_size;      // 总堆大小（字节）
    size_t total_allocated;      // 已分配的总字节数
    size_t total_free;           // 空闲的总字节数
    size_t num_blocks;           // 总块数
    size_t num_free_blocks;      // 空闲块数
    size_t largest_free_block;   // 最大空闲块大小
} heap_stats_t;

/**
 * @brief 获取堆统计信息
 * 
 * @param stats 用于存储统计信息的结构体指针
 */
void my_malloc_stats(heap_stats_t *stats);

/**
 * @brief 打印堆的当前状态（用于调试）
 */
void my_malloc_debug_print(void);

#ifdef __cplusplus
}
#endif

#endif /* MY_MALLOC_H */