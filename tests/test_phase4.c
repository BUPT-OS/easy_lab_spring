/**
 * Phase 4 测试：fork + 写时复制 (COW)
 * 
 * 测试目标：
 * - fork 时的页表复制
 * - 页面共享与引用计数
 * - 写时复制触发
 * - 父子进程数据隔离
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
 * 辅助函数：模拟 fork
 * 复制 VMA 和页表，设置 COW 标记
 */
int fork_mm(struct mm_struct *parent, struct mm_struct *child) {
    memset(child, 0, sizeof(*child));
    
    // 复制 VMA 链表
    struct vm_area_struct *parent_vma = parent->mmap;
    struct vm_area_struct **child_vma_ptr = &child->mmap;
    
    while (parent_vma) {
        struct vm_area_struct *new_vma = malloc(sizeof(struct vm_area_struct));
        if (!new_vma) return -1;
        
        memcpy(new_vma, parent_vma, sizeof(*new_vma));
        new_vma->vm_next = NULL;
        new_vma->vm_prev = NULL;
        
        if (parent_vma->vm_file_path) {
            new_vma->vm_file_path = strdup(parent_vma->vm_file_path);
        }
        
        *child_vma_ptr = new_vma;
        child_vma_ptr = &new_vma->vm_next;
        parent_vma = parent_vma->vm_next;
    }
    
    // 复制页表，设置 COW
    // 遍历父进程的所有 VMA，对每个已映射的页进行处理
    parent_vma = parent->mmap;
    while (parent_vma) {
        for (uint64_t addr = parent_vma->vm_start; 
             addr < parent_vma->vm_end; 
             addr += PAGE_SIZE) {
            
            pte_t *parent_pte = pt_walk(parent, addr, false);
            if (parent_pte && (*parent_pte & PTE_P)) {
                // 1. 获取物理页
                uint64_t pfn = (*parent_pte & PFN_MASK) >> PAGE_SHIFT;
                struct page *page = pfn_to_page(pfn);
                
                // 2. 标记父进程 PTE 为只读（如果原来可写）
                if (*parent_pte & PTE_W) {
                    *parent_pte &= ~PTE_W;
                }
                
                // 3. 增加引用计数
                page->refcount++;
                
                // 4. 在子进程中建立相同的映射（只读）
                uint32_t flags = PTE_P | PTE_U;  // 无 PTE_W
                pt_map_page(child, addr, pfn, flags);
                
                printf("[Fork] Shared PFN %lu at addr %lx, refcount now: %d\n", 
                       pfn, addr, page->refcount);
            }
        }
        parent_vma = parent_vma->vm_next;
    }
    
    return 0;
}

/**
 * 测试 4.1: 基本 fork 操作
 */
void test_basic_fork(void) {
    printf("\n--- Test 4.1: Basic Fork ---\n");
    
    struct mm_struct parent = {0};
    struct mm_struct child = {0};
    uint64_t addr = 0x100000;
    
    // 父进程创建映射并写入数据
    do_mmap(&parent, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    char *data = "Parent Data";
    vm_write(&parent, addr, data, strlen(data) + 1);
    
    // 执行 fork
    int ret = fork_mm(&parent, &child);
    if (ret != 0) {
        TEST_FAIL("basic_fork", "fork_mm failed");
        return;
    }
    
    // 验证子进程 VMA 存在
    struct vm_area_struct *vma = find_vma(&child, addr);
    if (!vma) {
        TEST_FAIL("basic_fork_vma", "Child should have VMA");
        return;
    }
    
    // 验证子进程可以读取父进程的数据
    char buf[32] = {0};
    ret = vm_read(&child, addr, buf, 32);
    if (ret != 0) {
        TEST_FAIL("basic_fork_read", "Child read failed");
        return;
    }
    
    if (strcmp(buf, data) != 0) {
        TEST_FAIL("basic_fork_data", "Child should see parent's data");
        printf("  Expected: '%s'\n", data);
        printf("  Got:      '%s'\n", buf);
        return;
    }
    
    printf("  Child reads parent's data: '%s'\n", buf);
    TEST_PASS("test_basic_fork");
}

/**
 * 测试 4.2: 引用计数验证
 */
void test_refcount(void) {
    printf("\n--- Test 4.2: Reference Counting ---\n");
    
    struct mm_struct parent = {0};
    struct mm_struct child = {0};
    uint64_t addr = 0x200000;
    
    do_mmap(&parent, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    // 写入数据，分配物理页
    uint32_t val = 0xDEADBEEF;
    vm_write(&parent, addr, &val, sizeof(val));
    
    // 获取物理页
    pte_t *parent_pte = pt_walk(&parent, addr, false);
    if (!parent_pte || !(*parent_pte & PTE_P)) {
        TEST_FAIL("refcount_parent_pte", "Parent should have mapped page");
        return;
    }
    
    uint64_t pfn = (*parent_pte & PFN_MASK) >> PAGE_SHIFT;
    struct page *page = pfn_to_page(pfn);
    
    int initial_refcount = page->refcount;
    printf("  Initial refcount: %d\n", initial_refcount);
    
    // Fork
    fork_mm(&parent, &child);
    
    int after_fork_refcount = page->refcount;
    printf("  After fork refcount: %d\n", after_fork_refcount);
    
    if (after_fork_refcount != initial_refcount + 1) {
        TEST_FAIL("refcount_increment", "Refcount should increment after fork");
        return;
    }
    
    TEST_PASS("test_refcount");
}

/**
 * 测试 4.3: COW 触发
 */
void test_cow_trigger(void) {
    printf("\n--- Test 4.3: COW Trigger ---\n");
    
    struct mm_struct parent = {0};
    struct mm_struct child = {0};
    uint64_t addr = 0x300000;
    
    do_mmap(&parent, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    char *parent_data = "Parent Original";
    vm_write(&parent, addr, parent_data, strlen(parent_data) + 1);
    
    // 获取原始 PFN
    pte_t *pte = pt_walk(&parent, addr, false);
    uint64_t original_pfn = (*pte & PFN_MASK) >> PAGE_SHIFT;
    printf("  Original PFN: %lu\n", original_pfn);
    
    // Fork
    fork_mm(&parent, &child);
    
    // 子进程写入 -> 触发 COW
    printf("  Child writing (should trigger COW)...\n");
    char *child_data = "Child Modified";
    int ret = vm_write(&child, addr, child_data, strlen(child_data) + 1);
    if (ret != 0) {
        TEST_FAIL("cow_trigger_write", "Child write should succeed via COW");
        return;
    }
    
    // 验证子进程获得了新的 PFN
    pte_t *child_pte = pt_walk(&child, addr, false);
    uint64_t child_pfn = (*child_pte & PFN_MASK) >> PAGE_SHIFT;
    printf("  Child PFN after COW: %lu\n", child_pfn);
    
    if (child_pfn == original_pfn) {
        TEST_FAIL("cow_trigger_pfn", "Child should have different PFN after COW");
        return;
    }
    
    TEST_PASS("test_cow_trigger");
}

/**
 * 测试 4.4: 数据隔离验证
 */
void test_data_isolation(void) {
    printf("\n--- Test 4.4: Data Isolation ---\n");
    
    struct mm_struct parent = {0};
    struct mm_struct child = {0};
    uint64_t addr = 0x400000;
    
    do_mmap(&parent, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    char *original = "Original Data";
    vm_write(&parent, addr, original, strlen(original) + 1);
    
    // Fork
    fork_mm(&parent, &child);
    
    // 子进程修改数据
    char *modified = "Modified By Child";
    vm_write(&child, addr, modified, strlen(modified) + 1);
    
    // 验证父进程数据不变
    char parent_buf[32] = {0};
    vm_read(&parent, addr, parent_buf, 32);
    
    if (strcmp(parent_buf, original) != 0) {
        TEST_FAIL("data_isolation_parent", "Parent data should not change");
        printf("  Expected: '%s'\n", original);
        printf("  Got:      '%s'\n", parent_buf);
        return;
    }
    
    // 验证子进程数据已修改
    char child_buf[32] = {0};
    vm_read(&child, addr, child_buf, 32);
    
    if (strcmp(child_buf, modified) != 0) {
        TEST_FAIL("data_isolation_child", "Child data should be modified");
        printf("  Expected: '%s'\n", modified);
        printf("  Got:      '%s'\n", child_buf);
        return;
    }
    
    printf("  Parent reads: '%s'\n", parent_buf);
    printf("  Child reads:  '%s'\n", child_buf);
    TEST_PASS("test_data_isolation");
}

/**
 * 测试 4.5: 多页 COW
 */
void test_multipage_cow(void) {
    printf("\n--- Test 4.5: Multi-page COW ---\n");
    
    struct mm_struct parent = {0};
    struct mm_struct child = {0};
    uint64_t addr = 0x500000;
    int num_pages = 4;
    
    do_mmap(&parent, addr, PAGE_SIZE * num_pages, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    // 父进程写入每个页
    for (int i = 0; i < num_pages; i++) {
        uint32_t val = 0x1000 + i;
        vm_write(&parent, addr + i * PAGE_SIZE, &val, sizeof(val));
    }
    
    // Fork
    fork_mm(&parent, &child);
    
    // 只修改其中一个页（第2页）
    uint32_t new_val = 0xAAAA;
    vm_write(&child, addr + PAGE_SIZE, &new_val, sizeof(new_val));
    
    // 验证其他页仍然共享
    pte_t *parent_pte0 = pt_walk(&parent, addr, false);
    pte_t *child_pte0 = pt_walk(&child, addr, false);
    
    uint64_t parent_pfn0 = (*parent_pte0 & PFN_MASK) >> PAGE_SHIFT;
    uint64_t child_pfn0 = (*child_pte0 & PFN_MASK) >> PAGE_SHIFT;
    
    // 第0页应该仍然共享（除非实现了 eager COW）
    printf("  Page 0 - Parent PFN: %lu, Child PFN: %lu\n", parent_pfn0, child_pfn0);
    
    // 第1页应该分离
    pte_t *parent_pte1 = pt_walk(&parent, addr + PAGE_SIZE, false);
    pte_t *child_pte1 = pt_walk(&child, addr + PAGE_SIZE, false);
    
    uint64_t parent_pfn1 = (*parent_pte1 & PFN_MASK) >> PAGE_SHIFT;
    uint64_t child_pfn1 = (*child_pte1 & PFN_MASK) >> PAGE_SHIFT;
    
    printf("  Page 1 - Parent PFN: %lu, Child PFN: %lu\n", parent_pfn1, child_pfn1);
    
    if (parent_pfn1 == child_pfn1) {
        TEST_FAIL("multipage_cow_split", "Written page should be split");
        return;
    }
    
    // 验证数据正确性
    uint32_t parent_val, child_val;
    vm_read(&parent, addr + PAGE_SIZE, &parent_val, sizeof(parent_val));
    vm_read(&child, addr + PAGE_SIZE, &child_val, sizeof(child_val));
    
    if (parent_val == child_val) {
        TEST_FAIL("multipage_cow_data", "Data should differ after COW");
        return;
    }
    
    printf("  Page 1 - Parent value: 0x%x, Child value: 0x%x\n", parent_val, child_val);
    TEST_PASS("test_multipage_cow");
}

/**
 * 测试 4.6: COW 引用计数递减
 */
void test_cow_refcount_decrement(void) {
    printf("\n--- Test 4.6: COW Refcount Decrement ---\n");
    
    struct mm_struct parent = {0};
    struct mm_struct child = {0};
    uint64_t addr = 0x600000;
    
    do_mmap(&parent, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    uint32_t val = 0xBEEF;
    vm_write(&parent, addr, &val, sizeof(val));
    
    // 获取原始页
    pte_t *pte = pt_walk(&parent, addr, false);
    uint64_t original_pfn = (*pte & PFN_MASK) >> PAGE_SHIFT;
    struct page *original_page = pfn_to_page(original_pfn);
    
    printf("  Before fork - refcount: %d\n", original_page->refcount);
    
    // Fork
    fork_mm(&parent, &child);
    int refcount_after_fork = original_page->refcount;
    printf("  After fork - refcount: %d\n", refcount_after_fork);
    
    // 子进程写入触发 COW
    uint32_t new_val = 0xCAFE;
    vm_write(&child, addr, &new_val, sizeof(new_val));
    
    int refcount_after_cow = original_page->refcount;
    printf("  After COW - refcount: %d\n", refcount_after_cow);
    
    if (refcount_after_cow != refcount_after_fork - 1) {
        TEST_FAIL("cow_refcount_dec", "Refcount should decrement after COW");
        return;
    }
    
    TEST_PASS("test_cow_refcount_decrement");
}

/**
 * 测试 4.7: 父进程写入触发 COW
 */
void test_parent_cow(void) {
    printf("\n--- Test 4.7: Parent Write Triggers COW ---\n");
    
    struct mm_struct parent = {0};
    struct mm_struct child = {0};
    uint64_t addr = 0x700000;
    
    do_mmap(&parent, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    char *original = "Original";
    vm_write(&parent, addr, original, strlen(original) + 1);
    
    // Fork
    fork_mm(&parent, &child);
    
    // 父进程写入（不是子进程）
    printf("  Parent writing (should trigger COW)...\n");
    char *modified = "Parent Modified";
    int ret = vm_write(&parent, addr, modified, strlen(modified) + 1);
    if (ret != 0) {
        TEST_FAIL("parent_cow_write", "Parent write should succeed via COW");
        return;
    }
    
    // 验证数据隔离
    char parent_buf[32] = {0};
    char child_buf[32] = {0};
    
    vm_read(&parent, addr, parent_buf, 32);
    vm_read(&child, addr, child_buf, 32);
    
    printf("  Parent reads: '%s'\n", parent_buf);
    printf("  Child reads:  '%s'\n", child_buf);
    
    if (strcmp(parent_buf, modified) != 0) {
        TEST_FAIL("parent_cow_parent_data", "Parent should see modified data");
        return;
    }
    
    if (strcmp(child_buf, original) != 0) {
        TEST_FAIL("parent_cow_child_data", "Child should see original data");
        return;
    }
    
    TEST_PASS("test_parent_cow");
}

int main(void) {
    printf("========================================\n");
    printf("  Phase 4: Fork + COW Tests\n");
    printf("========================================\n");
    
    phys_mem_init();
    
    test_basic_fork();
    test_refcount();
    test_cow_trigger();
    test_data_isolation();
    test_multipage_cow();
    test_cow_refcount_decrement();
    test_parent_cow();
    
    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");
    
    if (tests_failed == 0) {
        printf("\n*** PHASE 4 COMPLETE! ***\n");
        return 0;
    }
    return 1;
}
