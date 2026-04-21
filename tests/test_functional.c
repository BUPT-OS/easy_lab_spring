/**
 * @file test_functional.c
 * @brief 自定义内存分配器 - 功能性测试套件
 * 
 * 本文件包含了对 my_malloc/my_free 实现的功能性测试。
 * 测试覆盖基础功能、边界条件、复杂场景和压力测试。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#include "my_malloc.h"

/*============================================================================
 * 测试框架
 *============================================================================*/

/* 测试结果统计 */
static int tests_passed = 0;
static int tests_failed = 0;

#define TOTAL_SCORE 60.0

/* 颜色输出 */
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_RESET   "\x1b[0m"

#define TEST_PASS(name) do { \
    printf(COLOR_GREEN "[PASS]" COLOR_RESET " %s\n", name); \
    tests_passed++; \
} while(0)

#define TEST_FAIL(name, reason) do { \
    printf(COLOR_RED "[FAIL]" COLOR_RESET " %s: %s\n", name, reason); \
    tests_failed++; \
} while(0)

#define TEST_INFO(msg) printf(COLOR_YELLOW "[INFO]" COLOR_RESET " %s\n", msg)

#define ASSERT(cond, name, reason) do { \
    if (cond) { \
        TEST_PASS(name); \
    } else { \
        TEST_FAIL(name, reason); \
    } \
} while(0)

/*============================================================================
 * 基础测试
 *============================================================================*/

/**
 * @brief 测试1: 简单分配释放
 */
void test_simple_alloc_free(void)
{
    TEST_INFO("Testing simple alloc and free...");
    
    void *ptr = my_malloc(100);
    if (ptr == NULL) {
        TEST_FAIL("simple_alloc", "malloc returned NULL");
        return;
    }
    
    /* 写入数据 */
    memset(ptr, 0xAB, 100);
    
    /* 验证数据 */
    unsigned char *p = (unsigned char *)ptr;
    int valid = 1;
    for (int i = 0; i < 100; i++) {
        if (p[i] != 0xAB) {
            valid = 0;
            break;
        }
    }
    
    ASSERT(valid, "simple_alloc_write", "data verification failed");
    
    my_free(ptr);
    TEST_PASS("simple_free");
}

/**
 * @brief 测试2: 多次分配测试
 */
void test_multiple_alloc(void)
{
    TEST_INFO("Testing multiple allocations...");
    
    #define NUM_ALLOCS 10
    void *ptrs[NUM_ALLOCS];
    
    /* 分配多块内存 */
    for (int i = 0; i < NUM_ALLOCS; i++) {
        ptrs[i] = my_malloc(64);
        if (ptrs[i] == NULL) {
            TEST_FAIL("multiple_alloc", "malloc returned NULL");
            /* 清理已分配的 */
            for (int j = 0; j < i; j++) {
                my_free(ptrs[j]);
            }
            return;
        }
    }
    
    /* 验证地址不重叠 */
    int overlap = 0;
    for (int i = 0; i < NUM_ALLOCS; i++) {
        for (int j = i + 1; j < NUM_ALLOCS; j++) {
            uintptr_t addr_i = (uintptr_t)ptrs[i];
            uintptr_t addr_j = (uintptr_t)ptrs[j];
            /* 检查是否有重叠（假设每块至少64字节） */
            if ((addr_i < addr_j && addr_i + 64 > addr_j) ||
                (addr_j < addr_i && addr_j + 64 > addr_i)) {
                overlap = 1;
                break;
            }
        }
        if (overlap) break;
    }
    
    ASSERT(!overlap, "multiple_alloc_no_overlap", "memory blocks overlap");
    
    /* 释放所有内存 */
    for (int i = 0; i < NUM_ALLOCS; i++) {
        my_free(ptrs[i]);
    }
    TEST_PASS("multiple_free");
    
    #undef NUM_ALLOCS
}

/**
 * @brief 测试3: 释放后重用测试
 */
void test_reuse_after_free(void)
{
    TEST_INFO("Testing memory reuse after free...");
    
    /* 第一次分配 */
    void *ptr1 = my_malloc(128);
    if (ptr1 == NULL) {
        TEST_FAIL("reuse_alloc1", "first malloc returned NULL");
        return;
    }
    
    uintptr_t addr1 = (uintptr_t)ptr1;
    
    /* 释放 */
    my_free(ptr1);
    
    /* 再次分配相同大小 */
    void *ptr2 = my_malloc(128);
    if (ptr2 == NULL) {
        TEST_FAIL("reuse_alloc2", "second malloc returned NULL");
        return;
    }
    
    uintptr_t addr2 = (uintptr_t)ptr2;
    
    /* 检查是否重用了相同的内存（地址可能相同或接近） */
    /* 注意：这个测试不是强制的，取决于实现 */
    if (addr1 == addr2) {
        TEST_PASS("reuse_same_address");
    } else {
        TEST_INFO("Memory was not reused at same address (this is OK)");
        TEST_PASS("reuse_different_address");
    }
    
    my_free(ptr2);
}

/*============================================================================
 * 边界条件测试
 *============================================================================*/

/**
 * @brief 测试4: 零大小分配
 */
void test_zero_size(void)
{
    TEST_INFO("Testing zero size allocation...");
    
    void *ptr = my_malloc(0);
    
    /* 零大小可以返回 NULL 或有效指针，都是合理的 */
    if (ptr == NULL) {
        TEST_PASS("zero_size_returns_null");
    } else {
        TEST_PASS("zero_size_returns_valid_ptr");
        my_free(ptr);
    }
}

/**
 * @brief 测试5: 大块内存分配
 */
void test_large_alloc(void)
{
    TEST_INFO("Testing large allocation (1MB)...");
    
    size_t large_size = 1024 * 1024;  /* 1MB */
    void *ptr = my_malloc(large_size);
    
    if (ptr == NULL) {
        TEST_FAIL("large_alloc", "failed to allocate 1MB");
        return;
    }
    
    /* 写入一些数据验证可用 */
    memset(ptr, 0, large_size);
    ((char *)ptr)[0] = 'A';
    ((char *)ptr)[large_size - 1] = 'Z';
    
    int valid = (((char *)ptr)[0] == 'A' && ((char *)ptr)[large_size - 1] == 'Z');
    ASSERT(valid, "large_alloc_access", "failed to access large block");
    
    my_free(ptr);
    TEST_PASS("large_free");
}

/**
 * @brief 测试6: 对齐测试
 */
void test_alignment(void)
{
    TEST_INFO("Testing memory alignment...");
    
    int aligned = 1;
    
    for (int i = 0; i < 100; i++) {
        void *ptr = my_malloc(i * 7 + 1);  /* 各种奇怪的大小 */
        if (ptr == NULL) {
            TEST_FAIL("alignment_alloc", "malloc returned NULL");
            return;
        }
        
        /* 检查8字节对齐 */
        if ((uintptr_t)ptr % 8 != 0) {
            aligned = 0;
            printf("  Unaligned pointer: %p for size %d\n", ptr, i * 7 + 1);
        }
        
        my_free(ptr);
    }
    
    ASSERT(aligned, "alignment_8byte", "some allocations are not 8-byte aligned");
}

/*============================================================================
 * 复杂场景测试
 *============================================================================*/

/**
 * @brief 测试7: 碎片化测试
 */
void test_fragmentation(void)
{
    TEST_INFO("Testing fragmentation handling...");
    
    #define FRAG_COUNT 20
    void *ptrs[FRAG_COUNT];
    
    /* 分配多块不同大小的内存 */
    for (int i = 0; i < FRAG_COUNT; i++) {
        ptrs[i] = my_malloc(32 + i * 16);
        if (ptrs[i] == NULL) {
            TEST_FAIL("fragmentation_alloc", "malloc returned NULL");
            for (int j = 0; j < i; j++) my_free(ptrs[j]);
            return;
        }
    }
    
    /* 释放偶数位置的块，造成碎片 */
    for (int i = 0; i < FRAG_COUNT; i += 2) {
        my_free(ptrs[i]);
        ptrs[i] = NULL;
    }
    
    /* 尝试分配一些小块，应该能复用碎片 */
    void *small_ptrs[FRAG_COUNT / 2];
    int alloc_success = 1;
    
    for (int i = 0; i < FRAG_COUNT / 2; i++) {
        small_ptrs[i] = my_malloc(24);
        if (small_ptrs[i] == NULL) {
            alloc_success = 0;
            break;
        }
    }
    
    ASSERT(alloc_success, "fragmentation_reuse", "failed to reuse fragmented memory");
    
    /* 清理 */
    for (int i = 0; i < FRAG_COUNT; i++) {
        if (ptrs[i] != NULL) my_free(ptrs[i]);
    }
    for (int i = 0; i < FRAG_COUNT / 2; i++) {
        if (small_ptrs[i] != NULL) my_free(small_ptrs[i]);
    }
    
    #undef FRAG_COUNT
}

/**
 * @brief 测试8: NULL 指针释放
 */
void test_free_null(void)
{
    TEST_INFO("Testing free(NULL)...");
    
    /* 这应该不会崩溃 */
    my_free(NULL);
    my_free(NULL);
    my_free(NULL);
    
    TEST_PASS("free_null_no_crash");
}

/**
 * @brief 测试9: 连续分配释放循环
 */
void test_alloc_free_cycle(void)
{
    TEST_INFO("Testing alloc/free cycles...");
    
    int success = 1;
    
    for (int cycle = 0; cycle < 100; cycle++) {
        void *ptr = my_malloc(256);
        if (ptr == NULL) {
            success = 0;
            break;
        }
        memset(ptr, cycle & 0xFF, 256);
        my_free(ptr);
    }
    
    ASSERT(success, "alloc_free_cycles", "failed during alloc/free cycles");
}

/**
 * @brief 测试10: 内存合并测试
 */
void test_coalesce(void)
{
    TEST_INFO("Testing block coalescing...");
    
    /* 分配三个相邻的块 */
    void *ptr1 = my_malloc(100);
    void *ptr2 = my_malloc(100);
    void *ptr3 = my_malloc(100);
    
    if (ptr1 == NULL || ptr2 == NULL || ptr3 == NULL) {
        TEST_FAIL("coalesce_alloc", "initial allocation failed");
        if (ptr1) my_free(ptr1);
        if (ptr2) my_free(ptr2);
        if (ptr3) my_free(ptr3);
        return;
    }
    
    /* 释放中间的块 */
    my_free(ptr2);
    
    /* 释放第一个块，应该能与第二个合并 */
    my_free(ptr1);
    
    /* 释放第三个块，应该能与前面合并成一个大块 */
    my_free(ptr3);
    
    /* 现在尝试分配一个大块，应该能成功 */
    void *big_ptr = my_malloc(280);  /* 接近 3 * 100 */
    
    if (big_ptr != NULL) {
        TEST_PASS("coalesce_success");
        my_free(big_ptr);
    } else {
        /* 即使不能分配大块，也不一定是错误（取决于实现） */
        TEST_INFO("Could not allocate coalesced block (may be OK depending on implementation)");
        TEST_PASS("coalesce_attempted");
    }
}

/*============================================================================
 * 压力测试
 *============================================================================*/

/**
 * @brief 测试11: 大量小块分配
 */
void test_many_small_allocs(void)
{
    TEST_INFO("Testing many small allocations (1000 x 16 bytes)...");
    
    #define SMALL_COUNT 1000
    void *ptrs[SMALL_COUNT];
    int alloc_success = 1;
    
    for (int i = 0; i < SMALL_COUNT; i++) {
        ptrs[i] = my_malloc(16);
        if (ptrs[i] == NULL) {
            alloc_success = 0;
            /* 记录失败位置 */
            printf("  Failed at allocation %d\n", i);
            /* 释放已分配的 */
            for (int j = 0; j < i; j++) {
                my_free(ptrs[j]);
            }
            break;
        }
        /* 写入一些数据 */
        memset(ptrs[i], i & 0xFF, 16);
    }
    
    if (alloc_success) {
        TEST_PASS("many_small_allocs");
        
        /* 释放所有 */
        for (int i = 0; i < SMALL_COUNT; i++) {
            my_free(ptrs[i]);
        }
        TEST_PASS("many_small_frees");
    } else {
        TEST_FAIL("many_small_allocs", "allocation failed");
    }
    
    #undef SMALL_COUNT
}

/**
 * @brief 测试12: 随机分配释放
 */
void test_random_alloc_free(void)
{
    TEST_INFO("Testing random alloc/free pattern...");
    
    srand((unsigned int)time(NULL));
    
    #define RAND_SLOTS 100
    void *ptrs[RAND_SLOTS] = {NULL};
    size_t sizes[RAND_SLOTS] = {0};
    
    int operations = 500;
    int success = 1;
    
    for (int op = 0; op < operations && success; op++) {
        int slot = rand() % RAND_SLOTS;
        
        if (ptrs[slot] == NULL) {
            /* 分配 */
            size_t size = (rand() % 256) + 1;
            ptrs[slot] = my_malloc(size);
            if (ptrs[slot] == NULL) {
                printf("  Allocation failed at op %d, size %zu\n", op, size);
                success = 0;
            } else {
                sizes[slot] = size;
                memset(ptrs[slot], slot & 0xFF, size);
            }
        } else {
            /* 先验证数据完整性 */
            unsigned char *p = (unsigned char *)ptrs[slot];
            unsigned char expected = slot & 0xFF;
            for (size_t i = 0; i < sizes[slot]; i++) {
                if (p[i] != expected) {
                    printf("  Data corruption at slot %d, offset %zu\n", slot, i);
                    success = 0;
                    break;
                }
            }
            
            /* 释放 */
            my_free(ptrs[slot]);
            ptrs[slot] = NULL;
            sizes[slot] = 0;
        }
    }
    
    /* 清理剩余 */
    for (int i = 0; i < RAND_SLOTS; i++) {
        if (ptrs[i] != NULL) {
            my_free(ptrs[i]);
        }
    }
    
    ASSERT(success, "random_alloc_free", "random pattern test failed");
    
    #undef RAND_SLOTS
}

/*============================================================================
 * calloc 和 realloc 测试（Level 3）
 *============================================================================*/

/**
 * @brief 测试13: calloc 基本功能
 */
void test_calloc_basic(void)
{
    TEST_INFO("Testing calloc basic functionality...");
    
    int *arr = (int *)my_calloc(100, sizeof(int));
    
    if (arr == NULL) {
        TEST_FAIL("calloc_basic", "calloc returned NULL");
        return;
    }
    
    /* 验证内存被清零 */
    int all_zero = 1;
    for (int i = 0; i < 100; i++) {
        if (arr[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    
    ASSERT(all_zero, "calloc_zeroed", "calloc did not zero memory");
    
    my_free(arr);
}

/**
 * @brief 测试14: realloc 扩大
 */
void test_realloc_grow(void)
{
    TEST_INFO("Testing realloc grow...");
    
    char *ptr = (char *)my_malloc(50);
    if (ptr == NULL) {
        TEST_FAIL("realloc_grow_init", "initial malloc failed");
        return;
    }
    
    /* 写入数据 */
    for (int i = 0; i < 50; i++) {
        ptr[i] = 'A' + (i % 26);
    }
    
    /* 扩大 */
    char *new_ptr = (char *)my_realloc(ptr, 100);
    if (new_ptr == NULL) {
        TEST_FAIL("realloc_grow", "realloc returned NULL");
        my_free(ptr);
        return;
    }
    
    /* 验证原数据保留 */
    int data_preserved = 1;
    for (int i = 0; i < 50; i++) {
        if (new_ptr[i] != 'A' + (i % 26)) {
            data_preserved = 0;
            break;
        }
    }
    
    ASSERT(data_preserved, "realloc_grow_data", "data not preserved after realloc");
    
    my_free(new_ptr);
}

/**
 * @brief 测试15: realloc 缩小
 */
void test_realloc_shrink(void)
{
    TEST_INFO("Testing realloc shrink...");
    
    char *ptr = (char *)my_malloc(100);
    if (ptr == NULL) {
        TEST_FAIL("realloc_shrink_init", "initial malloc failed");
        return;
    }
    
    /* 写入数据 */
    for (int i = 0; i < 100; i++) {
        ptr[i] = 'Z' - (i % 26);
    }
    
    /* 缩小 */
    char *new_ptr = (char *)my_realloc(ptr, 50);
    if (new_ptr == NULL) {
        TEST_FAIL("realloc_shrink", "realloc returned NULL");
        my_free(ptr);
        return;
    }
    
    /* 验证保留的数据 */
    int data_preserved = 1;
    for (int i = 0; i < 50; i++) {
        if (new_ptr[i] != 'Z' - (i % 26)) {
            data_preserved = 0;
            break;
        }
    }
    
    ASSERT(data_preserved, "realloc_shrink_data", "data not preserved after shrink");
    
    my_free(new_ptr);
}

/**
 * @brief 测试16: realloc 特殊情况
 */
void test_realloc_special(void)
{
    TEST_INFO("Testing realloc special cases...");
    
    /* realloc(NULL, size) 应该等同于 malloc(size) */
    void *ptr1 = my_realloc(NULL, 64);
    ASSERT(ptr1 != NULL, "realloc_null_ptr", "realloc(NULL, 64) should work like malloc");
    
    if (ptr1 != NULL) {
        /* realloc(ptr, 0) 应该等同于 free(ptr) */
        void *ptr2 = my_realloc(ptr1, 0);
        /* ptr2 可以是 NULL 或有效指针 */
        if (ptr2 != NULL) {
            my_free(ptr2);
        }
        TEST_PASS("realloc_zero_size");
    }
}

/*============================================================================
 * 线程安全测试
 *============================================================================*/

#define THREAD_COUNT 4
#define ALLOCS_PER_THREAD 1000
#define ALLOC_SIZE 64

/* 线程测试结果 */
typedef struct {
    int thread_id;
    int success_count;
    int failure_count;
    void **ptrs;
} thread_test_result_t;

/**
 * @brief 线程测试函数 - 每个线程分配和释放多个块
 */
void *thread_alloc_free(void *arg)
{
    thread_test_result_t *result = (thread_test_result_t *)arg;
    result->success_count = 0;
    result->failure_count = 0;
    
    /* 分配阶段 */
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        result->ptrs[i] = my_malloc(ALLOC_SIZE);
        if (result->ptrs[i] != NULL) {
            /* 写入线程ID和索引以验证数据完整性 */
            memset(result->ptrs[i], (unsigned char)(result->thread_id ^ i), ALLOC_SIZE);
            result->success_count++;
        } else {
            result->failure_count++;
        }
    }
    
    /* 验证数据完整性 */
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        if (result->ptrs[i] != NULL) {
            unsigned char expected = (unsigned char)(result->thread_id ^ i);
            unsigned char *p = (unsigned char *)result->ptrs[i];
            for (int j = 0; j < ALLOC_SIZE; j++) {
                if (p[j] != expected) {
                    result->failure_count++;
                    break;
                }
            }
        }
    }
    
    /* 释放阶段 */
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        if (result->ptrs[i] != NULL) {
            my_free(result->ptrs[i]);
            result->ptrs[i] = NULL;
        }
    }
    
    return NULL;
}

/**
 * @brief 线程测试函数 - 交替分配释放
 */
void *thread_alternating(void *arg)
{
    thread_test_result_t *result = (thread_test_result_t *)arg;
    result->success_count = 0;
    result->failure_count = 0;
    
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        /* 分配 */
        void *ptr = my_malloc(ALLOC_SIZE + (i % 128));
        if (ptr != NULL) {
            memset(ptr, 0xAB, ALLOC_SIZE);
            result->success_count++;
            /* 立即释放 */
            my_free(ptr);
        } else {
            result->failure_count++;
        }
    }
    
    return NULL;
}

/**
 * @brief 测试17: 多线程并发分配释放
 */
void test_thread_safety_basic(void)
{
    TEST_INFO("Testing thread safety (concurrent alloc/free)...");
    
    pthread_t threads[THREAD_COUNT];
    thread_test_result_t results[THREAD_COUNT];
    
    /* 为每个线程分配指针数组 */
    for (int i = 0; i < THREAD_COUNT; i++) {
        results[i].thread_id = i;
        results[i].ptrs = (void **)malloc(ALLOCS_PER_THREAD * sizeof(void *));
        if (results[i].ptrs == NULL) {
            TEST_FAIL("Thread safety setup", "Failed to allocate pointer array");
            return;
        }
        memset(results[i].ptrs, 0, ALLOCS_PER_THREAD * sizeof(void *));
    }
    
    /* 创建线程 */
    for (int i = 0; i < THREAD_COUNT; i++) {
        if (pthread_create(&threads[i], NULL, thread_alloc_free, &results[i]) != 0) {
            TEST_FAIL("Thread safety", "Failed to create thread");
            /* 清理已分配的资源 */
            for (int j = 0; j <= i; j++) {
                free(results[j].ptrs);
            }
            return;
        }
    }
    
    /* 等待所有线程完成 */
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* 检查结果 */
    int total_success = 0;
    int total_failure = 0;
    for (int i = 0; i < THREAD_COUNT; i++) {
        total_success += results[i].success_count;
        total_failure += results[i].failure_count;
        free(results[i].ptrs);
    }
    
    int expected_success = THREAD_COUNT * ALLOCS_PER_THREAD;
    ASSERT(total_success == expected_success && total_failure == 0,
           "Concurrent alloc/free",
           "Some allocations failed or data corrupted");
}

/**
 * @brief 测试18: 多线程交替分配释放
 */
void test_thread_safety_alternating(void)
{
    TEST_INFO("Testing thread safety (alternating pattern)...");
    
    pthread_t threads[THREAD_COUNT];
    thread_test_result_t results[THREAD_COUNT];
    
    /* 初始化结果结构 */
    for (int i = 0; i < THREAD_COUNT; i++) {
        results[i].thread_id = i;
        results[i].ptrs = NULL;  /* 交替模式不需要保存指针 */
    }
    
    /* 创建线程 */
    for (int i = 0; i < THREAD_COUNT; i++) {
        if (pthread_create(&threads[i], NULL, thread_alternating, &results[i]) != 0) {
            TEST_FAIL("Thread safety alternating", "Failed to create thread");
            return;
        }
    }
    
    /* 等待所有线程完成 */
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* 检查结果 */
    int total_success = 0;
    int total_failure = 0;
    for (int i = 0; i < THREAD_COUNT; i++) {
        total_success += results[i].success_count;
        total_failure += results[i].failure_count;
    }
    
    int expected_success = THREAD_COUNT * ALLOCS_PER_THREAD;
    ASSERT(total_success == expected_success && total_failure == 0,
           "Concurrent alternating alloc/free",
           "Some allocations failed in alternating pattern");
}

/*============================================================================
 * 主函数
 *============================================================================*/

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     Custom Memory Allocator - Functional Test Suite          ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* 基础测试 */
    printf("=== Basic Tests ===\n");
    test_simple_alloc_free();
    test_multiple_alloc();
    test_reuse_after_free();
    printf("\n");
    
    /* 边界条件测试 */
    printf("=== Boundary Tests ===\n");
    test_zero_size();
    test_large_alloc();
    test_alignment();
    printf("\n");
    
    /* 复杂场景测试 */
    printf("=== Complex Scenario Tests ===\n");
    test_fragmentation();
    test_free_null();
    test_alloc_free_cycle();
    test_coalesce();
    printf("\n");
    
    /* 压力测试 */
    printf("=== Stress Tests ===\n");
    test_many_small_allocs();
    test_random_alloc_free();
    printf("\n");
    
    /* calloc/realloc 测试 (Level 3) */
    printf("=== Level 3 Tests (calloc/realloc) ===\n");
    test_calloc_basic();
    test_realloc_grow();
    test_realloc_shrink();
    test_realloc_special();
    printf("\n");
    
    /* 线程安全测试 (Level 4) */
    printf("=== Level 4 Tests (Thread Safety) ===\n");
    test_thread_safety_basic();
    test_thread_safety_alternating();
    printf("\n");
    
    /* 打印堆统计信息 */
    printf("=== Heap Statistics ===\n");
    heap_stats_t stats;
    my_malloc_stats(&stats);
    printf("Total heap size: %zu bytes\n", stats.total_heap_size);
    printf("Total allocated: %zu bytes\n", stats.total_allocated);
    printf("Total free: %zu bytes\n", stats.total_free);
    printf("Free blocks: %zu\n", stats.num_free_blocks);
    printf("Largest free block: %zu bytes\n", stats.largest_free_block);
    printf("\n");
    
    /* 总结 */
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                       TEST SUMMARY                           ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  " COLOR_GREEN "Passed: %3d" COLOR_RESET "                                                 ║\n", tests_passed);
    printf("║  " COLOR_RED "Failed: %3d" COLOR_RESET "                                                 ║\n", tests_failed);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    if (tests_failed == 0) {
        printf(COLOR_GREEN "All tests passed! Congratulations!" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "Some tests failed. Please check your implementation." COLOR_RESET "\n");
    }
    printf("\n");
    
    /* 机器可读的输出格式，供脚本解析 */
    printf("##RESULT## FUNCTIONAL passed=%d failed=%d total=%d\n", 
           tests_passed, tests_failed, tests_passed + tests_failed);
    
    double score = tests_passed * TOTAL_SCORE / (tests_passed + tests_failed);
    printf("functional_score: %.2f\n", score);
    return tests_failed > 0 ? 1 : 0;
}