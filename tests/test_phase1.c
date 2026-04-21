/**
 * Phase 1 测试：最小可运行 VM
 * 
 * 测试目标：
 * - 物理页分配与释放
 * - 基本页表操作
 * - Demand Paging（按需分页）
 * - 简单的读写操作
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "vm_defs.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_PASS(name) do { \
    printf("[PASS] %s\n", name); \
    tests_passed++; \
} while(0)

#define TEST_FAIL(name, reason) do { \
    printf("[FAIL] %s: %s\n", name, reason); \
    tests_failed++; \
} while(0)

/**
 * 测试 1.1: 物理页分配
 */
void test_phys_alloc(void) {
    printf("\n--- Test 1.1: Physical Page Allocation ---\n");
    
    struct page *p1 = alloc_page();
    if (!p1) {
        TEST_FAIL("phys_alloc_basic", "Failed to allocate first page");
        return;
    }
    
    if (p1->refcount != 1) {
        TEST_FAIL("phys_alloc_refcount", "New page refcount should be 1");
        return;
    }
    
    struct page *p2 = alloc_page();
    if (!p2) {
        TEST_FAIL("phys_alloc_second", "Failed to allocate second page");
        return;
    }
    
    if (p1 == p2) {
        TEST_FAIL("phys_alloc_unique", "Allocated same page twice");
        return;
    }
    
    // 验证页面内容被清零
    void *data = page_to_virt(p1);
    uint8_t *bytes = (uint8_t *)data;
    int is_zero = 1;
    for (int i = 0; i < PAGE_SIZE; i++) {
        if (bytes[i] != 0) {
            is_zero = 0;
            break;
        }
    }
    
    if (!is_zero) {
        TEST_FAIL("phys_alloc_zeroed", "New page should be zeroed");
        return;
    }
    
    TEST_PASS("test_phys_alloc");
}

/**
 * 测试 1.2: 物理页释放
 */
void test_phys_free(void) {
    printf("\n--- Test 1.2: Physical Page Free ---\n");
    
    struct page *p = alloc_page();
    if (!p) {
        TEST_FAIL("phys_free_alloc", "Failed to allocate page");
        return;
    }
    
    uint64_t pfn = page_to_pfn(p);
    
    // 增加引用计数
    p->refcount = 2;
    free_page(p);
    
    if (p->refcount != 1) {
        TEST_FAIL("phys_free_refcount", "Refcount should decrement to 1");
        return;
    }
    
    free_page(p);
    
    if (p->refcount != 0) {
        TEST_FAIL("phys_free_zero", "Refcount should be 0 after second free");
        return;
    }
    
    // 页面应该回到 free list，可以再次分配
    struct page *p2 = alloc_page();
    if (!p2) {
        TEST_FAIL("phys_free_realloc", "Failed to reallocate freed page");
        return;
    }
    
    TEST_PASS("test_phys_free");
}

/**
 * 测试 1.3: 页表遍历
 */
void test_pt_walk(void) {
    printf("\n--- Test 1.3: Page Table Walk ---\n");
    
    struct mm_struct mm = {0};
    uint64_t addr = 0x100000;
    
    // 不分配模式下应该返回 NULL
    pte_t *pte = pt_walk(&mm, addr, false);
    if (pte != NULL) {
        TEST_FAIL("pt_walk_no_alloc", "Should return NULL when not allocating");
        return;
    }
    
    // 分配模式下应该创建页表
    pte = pt_walk(&mm, addr, true);
    if (pte == NULL) {
        TEST_FAIL("pt_walk_alloc", "Should return valid PTE when allocating");
        return;
    }
    
    // 同一地址再次访问应该返回相同的 PTE
    pte_t *pte2 = pt_walk(&mm, addr, false);
    if (pte != pte2) {
        TEST_FAIL("pt_walk_consistent", "Should return same PTE for same address");
        return;
    }
    
    // 不同地址应该返回不同的 PTE
    pte_t *pte3 = pt_walk(&mm, addr + PAGE_SIZE, true);
    if (pte == pte3) {
        TEST_FAIL("pt_walk_different", "Different addresses should have different PTEs");
        return;
    }
    
    TEST_PASS("test_pt_walk");
}

/**
 * 测试 1.4: 基本读写（Demand Paging）
 */
void test_basic_rw(void) {
    printf("\n--- Test 1.4: Basic Read/Write (Demand Paging) ---\n");
    
    struct mm_struct mm = {0};
    uint64_t addr = 0x200000;
    
    // 创建匿名映射
    int ret = do_mmap(&mm, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    if (ret != 0) {
        TEST_FAIL("basic_rw_mmap", "Failed to create anonymous mapping");
        return;
    }
    
    // 写入数据（应该触发 page fault）
    char *test_data = "Hello, VM Simulator!";
    ret = vm_write(&mm, addr, test_data, strlen(test_data) + 1);
    if (ret != 0) {
        TEST_FAIL("basic_rw_write", "Failed to write data");
        return;
    }
    
    // 读取数据
    char buf[64] = {0};
    ret = vm_read(&mm, addr, buf, strlen(test_data) + 1);
    if (ret != 0) {
        TEST_FAIL("basic_rw_read", "Failed to read data");
        return;
    }
    
    // 验证数据正确性
    if (strcmp(buf, test_data) != 0) {
        TEST_FAIL("basic_rw_verify", "Read data doesn't match written data");
        printf("  Expected: '%s'\n", test_data);
        printf("  Got:      '%s'\n", buf);
        return;
    }
    
    TEST_PASS("test_basic_rw");
}

/**
 * 测试 1.5: 多次读写
 */
void test_multiple_rw(void) {
    printf("\n--- Test 1.5: Multiple Read/Write ---\n");
    
    struct mm_struct mm = {0};
    uint64_t addr = 0x300000;
    
    do_mmap(&mm, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    // 写入多个位置
    uint32_t val1 = 0xDEADBEEF;
    uint32_t val2 = 0xCAFEBABE;
    
    vm_write(&mm, addr, &val1, sizeof(val1));
    vm_write(&mm, addr + 100, &val2, sizeof(val2));
    
    // 读取并验证
    uint32_t read1 = 0, read2 = 0;
    vm_read(&mm, addr, &read1, sizeof(read1));
    vm_read(&mm, addr + 100, &read2, sizeof(read2));
    
    if (read1 != val1 || read2 != val2) {
        TEST_FAIL("multiple_rw_verify", "Data mismatch");
        printf("  val1: expected 0x%x, got 0x%x\n", val1, read1);
        printf("  val2: expected 0x%x, got 0x%x\n", val2, read2);
        return;
    }
    
    TEST_PASS("test_multiple_rw");
}

int main(void) {
    printf("========================================\n");
    printf("  Phase 1: Minimum Viable VM Tests\n");
    printf("========================================\n");
    
    phys_mem_init();
    
    test_phys_alloc();
    test_phys_free();
    test_pt_walk();
    test_basic_rw();
    test_multiple_rw();
    
    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");
    
    if (tests_failed == 0) {
        printf("\n*** PHASE 1 COMPLETE! ***\n");
        return 0;
    }
    return 1;
}
