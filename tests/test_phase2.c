/**
 * Phase 2 测试：多 VMA + 权限检查
 * 
 * 测试目标：
 * - 多个 VMA 的创建与管理
 * - VMA 查找功能
 * - 权限检查（读/写/执行）
 * - 访问无效地址触发 SIGSEGV
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
 * 测试 2.1: 多个 VMA 创建
 */
void test_multiple_vmas(void) {
    printf("\n--- Test 2.1: Multiple VMAs ---\n");
    
    struct mm_struct mm = {0};
    
    // 创建三个不重叠的 VMA
    int ret1 = do_mmap(&mm, 0x100000, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    int ret2 = do_mmap(&mm, 0x200000, PAGE_SIZE * 2, VM_READ, VM_ANON, NULL, 0);
    int ret3 = do_mmap(&mm, 0x300000, PAGE_SIZE, VM_READ | VM_WRITE | VM_EXEC, VM_ANON, NULL, 0);
    
    if (ret1 != 0 || ret2 != 0 || ret3 != 0) {
        TEST_FAIL("multiple_vmas_create", "Failed to create VMAs");
        return;
    }
    
    // 验证 VMA 链表
    int vma_count = 0;
    struct vm_area_struct *vma = mm.mmap;
    while (vma) {
        vma_count++;
        vma = vma->vm_next;
    }
    
    if (vma_count != 3) {
        TEST_FAIL("multiple_vmas_count", "Expected 3 VMAs");
        printf("  Got: %d VMAs\n", vma_count);
        return;
    }
    
    print_vmas(&mm);
    TEST_PASS("test_multiple_vmas");
}

/**
 * 测试 2.2: VMA 查找
 */
void test_find_vma(void) {
    printf("\n--- Test 2.2: VMA Lookup ---\n");
    
    struct mm_struct mm = {0};
    
    do_mmap(&mm, 0x100000, PAGE_SIZE * 4, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    do_mmap(&mm, 0x200000, PAGE_SIZE * 2, VM_READ, VM_ANON, NULL, 0);
    
    // 测试在 VMA 内的地址
    struct vm_area_struct *vma = find_vma(&mm, 0x100000);
    if (!vma || vma->vm_start != 0x100000) {
        TEST_FAIL("find_vma_start", "Should find VMA at start address");
        return;
    }
    
    // 测试 VMA 中间的地址
    vma = find_vma(&mm, 0x101000);
    if (!vma || vma->vm_start != 0x100000) {
        TEST_FAIL("find_vma_middle", "Should find VMA for middle address");
        return;
    }
    
    // 测试第二个 VMA
    vma = find_vma(&mm, 0x200500);
    if (!vma || vma->vm_start != 0x200000) {
        TEST_FAIL("find_vma_second", "Should find second VMA");
        return;
    }
    
    // 测试不在任何 VMA 中的地址
    vma = find_vma(&mm, 0x500000);
    if (vma != NULL) {
        TEST_FAIL("find_vma_none", "Should return NULL for unmapped address");
        return;
    }
    
    // 测试 VMA 之间的间隙
    vma = find_vma(&mm, 0x180000);
    if (vma != NULL) {
        TEST_FAIL("find_vma_gap", "Should return NULL for address in gap");
        return;
    }
    
    TEST_PASS("test_find_vma");
}

/**
 * 测试 2.3: 写入只读 VMA
 */
void test_write_readonly(void) {
    printf("\n--- Test 2.3: Write to Read-Only VMA ---\n");
    
    struct mm_struct mm = {0};
    uint64_t addr = 0x400000;
    
    // 创建只读 VMA
    do_mmap(&mm, addr, PAGE_SIZE, VM_READ, VM_ANON, NULL, 0);
    
    // 先读取（应该成功，触发 demand paging）
    char buf[16] = {0};
    int ret = vm_read(&mm, addr, buf, 16);
    if (ret != 0) {
        TEST_FAIL("write_readonly_read", "Read from RO VMA should succeed");
        return;
    }
    
    // 尝试写入（应该失败）
    char *data = "test";
    ret = vm_write(&mm, addr, data, 5);
    if (ret == 0) {
        TEST_FAIL("write_readonly_write", "Write to RO VMA should fail (SIGSEGV)");
        return;
    }
    
    printf("  Write to RO VMA correctly rejected (simulated SIGSEGV)\n");
    TEST_PASS("test_write_readonly");
}

/**
 * 测试 2.4: 访问无效地址
 */
void test_invalid_access(void) {
    printf("\n--- Test 2.4: Access Invalid Address ---\n");
    
    struct mm_struct mm = {0};
    
    // 创建一个 VMA
    do_mmap(&mm, 0x100000, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    // 访问不在任何 VMA 中的地址
    char buf[16];
    int ret = vm_read(&mm, 0x999000, buf, 16);
    if (ret == 0) {
        TEST_FAIL("invalid_access_read", "Read from invalid address should fail");
        return;
    }
    
    printf("  Access to invalid address correctly rejected (simulated SIGSEGV)\n");
    
    // 写入无效地址
    ret = vm_write(&mm, 0x888000, "test", 4);
    if (ret == 0) {
        TEST_FAIL("invalid_access_write", "Write to invalid address should fail");
        return;
    }
    
    TEST_PASS("test_invalid_access");
}

/**
 * 测试 2.5: VMA 边界检查
 */
void test_vma_boundary(void) {
    printf("\n--- Test 2.5: VMA Boundary Check ---\n");
    
    struct mm_struct mm = {0};
    uint64_t addr = 0x500000;
    
    // 创建一个 2 页的 VMA
    do_mmap(&mm, addr, PAGE_SIZE * 2, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    // 在 VMA 内访问
    char data[] = "boundary test";
    int ret = vm_write(&mm, addr + PAGE_SIZE - 5, data, 10);  // 跨页边界
    if (ret != 0) {
        TEST_FAIL("vma_boundary_cross", "Cross-page write within VMA should succeed");
        return;
    }
    
    // 在 VMA 末尾访问（应该成功）
    ret = vm_write(&mm, addr + PAGE_SIZE * 2 - 10, data, 10);
    if (ret != 0) {
        TEST_FAIL("vma_boundary_end", "Write at VMA end should succeed");
        return;
    }
    
    // 超出 VMA 访问（应该失败）
    ret = vm_write(&mm, addr + PAGE_SIZE * 2, data, 10);
    if (ret == 0) {
        TEST_FAIL("vma_boundary_over", "Write beyond VMA should fail");
        return;
    }
    
    TEST_PASS("test_vma_boundary");
}

/**
 * 测试 2.6: VMA 排序
 */
void test_vma_ordering(void) {
    printf("\n--- Test 2.6: VMA Ordering ---\n");
    
    struct mm_struct mm = {0};
    
    // 乱序创建 VMA
    do_mmap(&mm, 0x300000, PAGE_SIZE, VM_READ, VM_ANON, NULL, 0);
    do_mmap(&mm, 0x100000, PAGE_SIZE, VM_READ, VM_ANON, NULL, 0);
    do_mmap(&mm, 0x200000, PAGE_SIZE, VM_READ, VM_ANON, NULL, 0);
    
    // 验证按地址排序
    struct vm_area_struct *vma = mm.mmap;
    uint64_t prev_addr = 0;
    int ordered = 1;
    
    while (vma) {
        if (vma->vm_start < prev_addr) {
            ordered = 0;
            break;
        }
        prev_addr = vma->vm_start;
        vma = vma->vm_next;
    }
    
    if (!ordered) {
        TEST_FAIL("vma_ordering", "VMAs should be sorted by address");
        print_vmas(&mm);
        return;
    }
    
    print_vmas(&mm);
    TEST_PASS("test_vma_ordering");
}

int main(void) {
    printf("========================================\n");
    printf("  Phase 2: Multi-VMA + Permissions Tests\n");
    printf("========================================\n");
    
    phys_mem_init();
    
    test_multiple_vmas();
    test_find_vma();
    test_write_readonly();
    test_invalid_access();
    test_vma_boundary();
    test_vma_ordering();
    
    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");
    
    if (tests_failed == 0) {
        printf("\n*** PHASE 2 COMPLETE! ***\n");
        return 0;
    }
    return 1;
}
