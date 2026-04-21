/**
 * Phase 6 测试：内存压力与页面回收（进阶）
 * 
 * 测试目标：
 * - 内存压力处理
 * - LRU 页面回收
 * - 页面老化机制
 * - 脏页处理
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "vm_defs.h"

static int tests_passed = 0;
static int tests_failed = 0;
static FILE *detail_log = NULL;
static FILE *saved_stdout = NULL;

#define LOG_DETAIL(fmt, ...) do { \
    if (detail_log) { \
        fprintf(detail_log, fmt, ##__VA_ARGS__); \
        fflush(detail_log); \
    } \
} while(0)

// 重定向stdout到文件，只保留测试核心输出到终端
static void redirect_verbose_output(void) {
    fflush(stdout);
    saved_stdout = stdout;
    stdout = detail_log;
}

static void restore_output(void) {
    if (saved_stdout && detail_log) {
        fflush(detail_log);
        stdout = saved_stdout;
    }
}

#define TEST_PASS(name) do { \
    restore_output(); \
    printf("[PASS] %s\n", name); \
    tests_passed++; \
} while(0)

#define TEST_FAIL(name, reason) do { \
    restore_output(); \
    printf("[FAIL] %s: %s\n", name, reason); \
    tests_failed++; \
} while(0)

/**
 * 辅助函数：统计已分配的物理页数量
 */
int count_allocated_pages(void) {
    int count = 0;
    for (int i = 0; i < NUM_PHYS_PAGES; i++) {
        if (g_pages[i].refcount > 0) {
            count++;
        }
    }
    return count;
}

/**
 * 测试 6.1: 基本内存分配追踪
 */
void test_memory_tracking(void) {
    printf("\n--- Test 6.1: Memory Tracking ---\n");
    
    redirect_verbose_output();
    int initial_pages = count_allocated_pages();
    LOG_DETAIL("  Initially allocated pages: %d\n", initial_pages);
    restore_output();
    
    struct mm_struct mm = {0};
    uint64_t addr = 0x100000;
    
    // 分配多个页面
    int num_pages = 10;
    redirect_verbose_output();
    do_mmap(&mm, addr, PAGE_SIZE * num_pages, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    // 访问所有页面（触发分配）
    for (int i = 0; i < num_pages; i++) {
        uint32_t val = i;
        vm_write(&mm, addr + i * PAGE_SIZE, &val, sizeof(val));
    }
    restore_output();
    
    int after_alloc = count_allocated_pages();
    printf("  After allocating %d pages: %d total allocated\n", num_pages, after_alloc);
    
    // 应该增加了大约 num_pages 个页面（可能还有页表页）
    int diff = after_alloc - initial_pages;
    printf("  New pages allocated: %d (expected >= %d)\n", diff, num_pages);
    
    if (diff < num_pages) {
        TEST_FAIL("memory_tracking", "Should allocate at least num_pages");
        return;
    }
    
    TEST_PASS("test_memory_tracking");
}

/**
 * 测试 6.2: 内存压力 - 大量分配
 */
void test_memory_pressure(void) {
    printf("\n--- Test 6.2: Memory Pressure ---\n");
    
    struct mm_struct mm = {0};
    uint64_t base_addr = 0x1000000;  // 16MB 起始
    
    int successful_allocs = 0;
    int max_attempts = 100;
    
    redirect_verbose_output();
    // 尝试分配大量页面
    for (int i = 0; i < max_attempts; i++) {
        uint64_t addr = base_addr + i * PAGE_SIZE * 10;
        
        if (do_mmap(&mm, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0) != 0) {
            LOG_DETAIL("  mmap failed at iteration %d\n", i);
            break;
        }
        
        // 写入触发实际分配
        uint32_t val = i;
        int ret = vm_write(&mm, addr, &val, sizeof(val));
        if (ret != 0) {
            LOG_DETAIL("  Memory exhausted at iteration %d\n", i);
            break;
        }
        
        successful_allocs++;
    }
    restore_output();
    
    printf("  Successfully allocated %d pages before exhaustion/limit\n", successful_allocs);
    
    int total_allocated = count_allocated_pages();
    printf("  Total physical pages in use: %d / %d\n", total_allocated, NUM_PHYS_PAGES);
    
    // 如果实现了页面回收，即使在压力下也应该能继续分配
    // 如果没有实现，最终会 OOM
    
    TEST_PASS("test_memory_pressure");
}

/**
 * 测试 6.3: LRU 基本行为
 */
void test_lru_basic(void) {
    printf("\n--- Test 6.3: LRU Basic Behavior ---\n");
    
    redirect_verbose_output();
    struct mm_struct mm = {0};
    uint64_t addr1 = 0x200000;
    uint64_t addr2 = 0x201000;
    uint64_t addr3 = 0x202000;
    
    // 分配三个页面
    do_mmap(&mm, addr1, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    do_mmap(&mm, addr2, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    do_mmap(&mm, addr3, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    // 按顺序访问
    uint32_t val = 1;
    vm_write(&mm, addr1, &val, sizeof(val));
    val = 2;
    vm_write(&mm, addr2, &val, sizeof(val));
    val = 3;
    vm_write(&mm, addr3, &val, sizeof(val));
    
    // 获取物理页面
    pte_t *pte1 = pt_walk(&mm, addr1, false);
    pte_t *pte2 = pt_walk(&mm, addr2, false);
    pte_t *pte3 = pt_walk(&mm, addr3, false);
    
    if (!pte1 || !pte2 || !pte3) {
        TEST_FAIL("lru_basic_pte", "Failed to get PTEs for all addresses");
        return;
    }
    
    uint64_t pfn1 = (*pte1 & PFN_MASK) >> PAGE_SHIFT;
    uint64_t pfn2 = (*pte2 & PFN_MASK) >> PAGE_SHIFT;
    uint64_t pfn3 = (*pte3 & PFN_MASK) >> PAGE_SHIFT;
    
    struct page *page1 = pfn_to_page(pfn1);
    struct page *page2 = pfn_to_page(pfn2);
    struct page *page3 = pfn_to_page(pfn3);
    
    // 验证页面在LRU链表中（lru_next/lru_prev不应该都是NULL，除非在free list中）
    // 如果页面已分配(refcount > 0)，它应该在某个LRU链表中
    int pages_in_lru = 0;
    if (page1->lru_next != NULL || page1->lru_prev != NULL) pages_in_lru++;
    if (page2->lru_next != NULL || page2->lru_prev != NULL) pages_in_lru++;
    if (page3->lru_next != NULL || page3->lru_prev != NULL) pages_in_lru++;
    
    printf("  Pages in LRU chain: %d/3\n", pages_in_lru);
    
    if (pages_in_lru == 0) {
        TEST_FAIL("lru_chain_missing", "Pages should be linked in LRU chains (lru_next/lru_prev)");
        printf("  Expected: Pages should be in active/inactive LRU lists\n");
        printf("  Actual: All lru_next and lru_prev are NULL\n");
        return;
    }
    
    // 重新访问 addr1（应该 promote 到 active）
    vm_read(&mm, addr1, &val, sizeof(val));
    printf("  Accessed addr1 again (should promote in LRU)\n");
    
    // 检查 Accessed 位
    pte1 = pt_walk(&mm, addr1, false);
    pte2 = pt_walk(&mm, addr2, false);
    pte3 = pt_walk(&mm, addr3, false);
    
    printf("  addr1 PTE Accessed: %s\n", (*pte1 & PTE_A) ? "yes" : "no");
    printf("  addr2 PTE Accessed: %s\n", (*pte2 & PTE_A) ? "yes" : "no");
    printf("  addr3 PTE Accessed: %s\n", (*pte3 & PTE_A) ? "yes" : "no");
    
    // 重新访问的addr1应该设置了Accessed位
    if (!(*pte1 & PTE_A)) {
        TEST_FAIL("lru_accessed_bit", "addr1 should have Accessed bit set after recent access");
        return;
    }
    
    TEST_PASS("test_lru_basic");
}

/**
 * 测试 6.4: 脏页标记
 */
void test_dirty_pages(void) {
    printf("\n--- Test 6.4: Dirty Page Tracking ---\n");
    
    redirect_verbose_output();
    struct mm_struct mm = {0};
    uint64_t addr = 0x300000;
    
    do_mmap(&mm, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    // 只读访问
    char buf[16] = {0};
    vm_read(&mm, addr, buf, 16);
    
    pte_t *pte = pt_walk(&mm, addr, false);
    if (!pte) {
        TEST_FAIL("dirty_pages_pte", "PTE not found");
        return;
    }
    restore_output();
    
    int dirty_after_read = (*pte & PTE_D) ? 1 : 0;
    printf("  After read: Dirty=%d\n", dirty_after_read);
    
    // 写入访问
    char *data = "test";
    vm_write(&mm, addr, data, 5);
    
    pte = pt_walk(&mm, addr, false);
    int dirty_after_write = (*pte & PTE_D) ? 1 : 0;
    printf("  After write: Dirty=%d\n", dirty_after_write);
    
    if (!dirty_after_write) {
        TEST_FAIL("dirty_bit_not_set", "Dirty bit not set after write");
        return;
    }
    
    TEST_PASS("test_dirty_pages");
}

/**
 * 测试 6.5: 页面回收触发（需要实现）
 */
void test_page_reclaim(void) {
    printf("\n--- Test 6.5: Page Reclaim ---\n");
    
    struct mm_struct mm = {0};
    int initial_free = 0;
    
    // 计算初始空闲页
    for (int i = 0; i < NUM_PHYS_PAGES; i++) {
        if (g_pages[i].refcount == 0) {
            initial_free++;
        }
    }
    printf("  Initial free pages: %d\n", initial_free);
    
    redirect_verbose_output();
    // 分配几乎所有空闲页面
    uint64_t base = 0x10000000;
    int allocated = 0;
    int target = initial_free - 10;  // 留10个空闲页
    
    for (int i = 0; i < target && i < 500; i++) {
        uint64_t addr = base + i * PAGE_SIZE * 2;
        if (do_mmap(&mm, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0) == 0) {
            uint32_t val = i;
            if (vm_write(&mm, addr, &val, sizeof(val)) == 0) {
                allocated++;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    restore_output();
    
    printf("  Allocated %d pages\n", allocated);
    
    int before_pressure = 0;
    for (int i = 0; i < NUM_PHYS_PAGES; i++) {
        if (g_pages[i].refcount == 0) {
            before_pressure++;
        }
    }
    printf("  Free pages before pressure: %d\n", before_pressure);
    
    redirect_verbose_output();
    // 继续分配，触发页面回收
    int reclaim_allocs = 0;
    int max_reclaim_test = 50;
    
    for (int i = 0; i < max_reclaim_test; i++) {
        uint64_t addr = 0x50000000 + i * PAGE_SIZE * 2;
        if (do_mmap(&mm, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0) == 0) {
            uint32_t val = 0xDEAD;
            if (vm_write(&mm, addr, &val, sizeof(val)) == 0) {
                reclaim_allocs++;
            } else {
                LOG_DETAIL("  Failed to write at iteration %d\n", i);
                break;
            }
        } else {
            LOG_DETAIL("  mmap failed at iteration %d\n", i);
            break;
        }
    }
    restore_output();
    
    printf("  Allocations during pressure: %d\n", reclaim_allocs);
    
    int after_pressure = 0;
    for (int i = 0; i < NUM_PHYS_PAGES; i++) {
        if (g_pages[i].refcount == 0) {
            after_pressure++;
        }
    }
    printf("  Free pages after pressure: %d\n", after_pressure);
    
    // 如果实现了页面回收，应该能在压力下继续分配
    // 如果只是OOM，分配会很快失败
    if (reclaim_allocs == 0) {
        TEST_FAIL("page_reclaim_not_working", "Page reclaim not implemented - immediate OOM");
        printf("  Expected: Should reclaim pages and allow continued allocation\n");
        printf("  Actual: Failed to allocate any pages under pressure\n");
        return;
    }
    
    // 验证空闲页数量在合理范围（如果有回收，应该维持一定数量的空闲页）
    if (reclaim_allocs >= 20 && after_pressure < 5) {
        TEST_FAIL("page_reclaim_insufficient", "Page reclaim may not be maintaining free pages");
        printf("  Allocated %d under pressure but only %d free pages remain\n", 
               reclaim_allocs, after_pressure);
        return;
    }
    
    TEST_PASS("test_page_reclaim");
}

/**
 * 测试 6.6: 访问模式与 LRU
 */
void test_access_pattern_lru(void) {
    printf("\n--- Test 6.6: Access Pattern and LRU ---\n");
    
    redirect_verbose_output();
    struct mm_struct mm = {0};
    int num_pages = 8;
    uint64_t base = 0x400000;
    
    // 分配页面
    for (int i = 0; i < num_pages; i++) {
        uint64_t addr = base + i * PAGE_SIZE;
        do_mmap(&mm, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
        uint32_t val = i;
        vm_write(&mm, addr, &val, sizeof(val));
    }
    restore_output();
    
    printf("  Allocated %d pages\n", num_pages);
    
    // 获取这些页面的PFN（用于后续验证）
    uint64_t hot_pfns[3];
    for (int i = 0; i < 3; i++) {
        pte_t *pte = pt_walk(&mm, base + i * PAGE_SIZE, false);
        if (pte) {
            hot_pfns[i] = (*pte & PFN_MASK) >> PAGE_SHIFT;
        }
    }
    
    // 模拟工作集访问模式：频繁访问前几个页面
    printf("  Simulating working set access pattern...\n");
    redirect_verbose_output();
    for (int round = 0; round < 10; round++) {
        // 只访问前 3 个页面（热页面）
        for (int i = 0; i < 3; i++) {
            uint64_t addr = base + i * PAGE_SIZE;
            uint32_t val;
            vm_read(&mm, addr, &val, sizeof(val));
        }
    }
    
    // 偶尔访问其他页面（冷页面）
    uint32_t val;
    vm_read(&mm, base + 7 * PAGE_SIZE, &val, sizeof(val));
    restore_output();
    
    // 检查 Accessed 位
    printf("  Page access status:\n");
    int hot_pages_accessed = 0;
    for (int i = 0; i < num_pages; i++) {
        uint64_t addr = base + i * PAGE_SIZE;
        pte_t *pte = pt_walk(&mm, addr, false);
        if (pte) {
            int accessed = (*pte & PTE_A) ? 1 : 0;
            printf("    Page %d: Accessed=%d\n", i, accessed);
            if (i < 3 && accessed) {
                hot_pages_accessed++;
            }
        }
    }
    
    // 验证热页面都设置了Accessed位
    if (hot_pages_accessed < 3) {
        TEST_FAIL("hot_pages_not_tracked", "Hot pages should have Accessed bit set");
        printf("  Expected: All 3 hot pages have Accessed=1\n");
        printf("  Actual: Only %d/3 hot pages have Accessed bit\n", hot_pages_accessed);
        return;
    }
    
    // 检查这些页面是否在LRU链表中
    int hot_pages_in_lru = 0;
    for (int i = 0; i < 3; i++) {
        struct page *p = pfn_to_page(hot_pfns[i]);
        if (p && (p->lru_next != NULL || p->lru_prev != NULL)) {
            hot_pages_in_lru++;
        }
    }
    
    printf("  Hot pages in LRU chains: %d/3\n", hot_pages_in_lru);
    
    if (hot_pages_in_lru == 0) {
        TEST_FAIL("lru_chain_not_maintained", "Pages should be in LRU chains");
        printf("  Expected: Pages should be linked in active/inactive lists\n");
        printf("  Actual: No pages have lru_next/lru_prev set\n");
        return;
    }
    
    TEST_PASS("test_access_pattern_lru");
}

/**
 * 测试 6.7: 内存统计
 */
void test_memory_statistics(void) {
    printf("\n--- Test 6.7: Memory Statistics ---\n");
    
    int total_pages = NUM_PHYS_PAGES;
    int used_pages = 0;
    int free_pages = 0;
    int dirty_pages = 0;
    
    for (int i = 0; i < NUM_PHYS_PAGES; i++) {
        if (g_pages[i].refcount > 0) {
            used_pages++;
            // 检查是否有脏标记（需要实现 page flags）
        } else {
            free_pages++;
        }
    }
    
    printf("  Memory Statistics:\n");
    printf("    Total physical pages: %d (%d MB)\n", 
           total_pages, (total_pages * PAGE_SIZE) / (1024 * 1024));
    printf("    Used pages: %d (%d KB)\n", 
           used_pages, (used_pages * PAGE_SIZE) / 1024);
    printf("    Free pages: %d (%d KB)\n", 
           free_pages, (free_pages * PAGE_SIZE) / 1024);
    printf("    Memory usage: %.1f%%\n", 
           (float)used_pages / total_pages * 100);
    
    TEST_PASS("test_memory_statistics");
}

/**
 * 测试 6.8: OOM 处理
 */
void test_oom_handling(void) {
    printf("\n--- Test 6.8: OOM Handling ---\n");
    
    struct mm_struct mm = {0};
    uint64_t base = 0x80000000;
    int oom_triggered = 0;
    int alloc_count = 0;
    
    printf("  Allocating until OOM...\n");
    redirect_verbose_output();
    for (int i = 0; i < NUM_PHYS_PAGES + 100; i++) {
        uint64_t addr = base + i * PAGE_SIZE * 2;
        if (do_mmap(&mm, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_ANON, NULL, 0) != 0) {
            LOG_DETAIL("  mmap failed at iteration %d\n", i);
            oom_triggered = 1;
            break;
        }
        
        uint32_t val = i;
        if (vm_write(&mm, addr, &val, sizeof(val)) != 0) {
            LOG_DETAIL("  OOM at iteration %d (write failed)\n", i);
            oom_triggered = 1;
            break;
        }
        alloc_count++;
        
        // 每 100 次记录到详细日志
        if (alloc_count % 100 == 0) {
            LOG_DETAIL("    Allocated %d pages...\n", alloc_count);
        }
    }
    restore_output();
    
    if (oom_triggered) {
        printf("  OOM correctly detected after %d allocations\n", alloc_count);
    } else {
        TEST_FAIL("oom_not_triggered", "OOM should be triggered when physical memory exhausted");
        printf("  Allocated %d pages without OOM\n", alloc_count);
        return;
    }
    
    // 验证已分配的页面仍然可用
    if (alloc_count > 0) {
        uint32_t val;
        vm_read(&mm, base, &val, sizeof(val));
        restore_output();
        if (val == 0) {
            printf("  Existing allocations still accessible\n");
        }
    } else {
        restore_output();
    }
    
    TEST_PASS("test_oom_handling");
}

int main(void) {
    // 打开详细日志文件
    detail_log = fopen("/tmp/vm_phase6_detail.log", "w");
    if (!detail_log) {
        fprintf(stderr, "Warning: Could not open detail log file\n");
    }
    
    printf("========================================\n");
    printf("  Phase 6: Memory Pressure & Reclaim\n");
    printf("           (Advanced Tests)\n");
    printf("========================================\n");
    if (detail_log) {
        printf("  Detail log: /tmp/vm_phase6_detail.log\n");
        printf("========================================\n");
    }
    
    phys_mem_init();
    
    test_memory_tracking();
    test_memory_pressure();
    test_lru_basic();
    test_dirty_pages();
    test_page_reclaim();
    test_access_pattern_lru();
    test_memory_statistics();
    test_oom_handling();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");
    
    if (tests_failed > 0) {
        printf("\n  Failed tests - check above for details\n");
    }
    
    if (detail_log) {
        fprintf(detail_log, "\n=== Final Results ===\n");
        fprintf(detail_log, "Passed: %d, Failed: %d\n", tests_passed, tests_failed);
        fclose(detail_log);
        printf("\nDetailed logs written to: /tmp/vm_phase6_detail.log\n");
    }
    
    if (tests_failed == 0) {
        printf("\n*** PHASE 6 COMPLETE! ***\n");
        return 0;
    }
    return 1;
}
