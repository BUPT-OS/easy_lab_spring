/**
 * Phase 5 测试：Page Cache（进阶）
 * 
 * 测试目标：
 * - Page Cache 数据结构
 * - 多进程共享文件映射
 * - 共享映射 vs 私有映射
 * - 文件映射的 COW
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
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

#define TEST_FILE "/tmp/vm_sim_page_cache_test.txt"

static void create_test_file(const char *content) {
    FILE *fp = fopen(TEST_FILE, "w");
    if (fp) {
        fprintf(fp, "%s", content);
        fclose(fp);
    }
}

static void cleanup_test_file(void) {
    unlink(TEST_FILE);
}

/**
 * 测试 5.1: 同一进程多次映射同一文件
 */
void test_same_process_double_map(void) {
    printf("\n--- Test 5.1: Same Process Double Map ---\n");
    
    create_test_file("Page Cache Test Content - This should be shared!");
    
    struct mm_struct mm = {0};
    uint64_t addr1 = 0x100000;
    uint64_t addr2 = 0x200000;
    
    // 两次映射同一文件
    do_mmap(&mm, addr1, PAGE_SIZE, VM_READ, 0, TEST_FILE, 0);
    do_mmap(&mm, addr2, PAGE_SIZE, VM_READ, 0, TEST_FILE, 0);
    
    // 读取两个映射
    char buf1[64] = {0};
    char buf2[64] = {0};
    
    vm_read(&mm, addr1, buf1, 63);
    vm_read(&mm, addr2, buf2, 63);
    
    // 内容应该相同
    if (strcmp(buf1, buf2) != 0) {
        TEST_FAIL("double_map_content", "Both mappings should read same content");
        printf("  addr1: '%s'\n", buf1);
        printf("  addr2: '%s'\n", buf2);
        cleanup_test_file();
        return;
    }
    
    // 理想情况：两个映射应该共享同一物理页
    pte_t *pte1 = pt_walk(&mm, addr1, false);
    pte_t *pte2 = pt_walk(&mm, addr2, false);
    
    if (pte1 && pte2) {
        uint64_t pfn1 = (*pte1 & PFN_MASK) >> PAGE_SHIFT;
        uint64_t pfn2 = (*pte2 & PFN_MASK) >> PAGE_SHIFT;
        
        if (pfn1 == pfn2) {
            printf("  Page Cache working: Both mappings share PFN %lu\n", pfn1);
        } else {
            TEST_FAIL("double_map_pfn", "Page Cache not implemented - different PFNs");
            printf("  Expected same PFN, got %lu and %lu\n", pfn1, pfn2);
            cleanup_test_file();
            return;
        }
    }
    
    printf("  Both mappings read: '%.40s...'\n", buf1);
    cleanup_test_file();
    TEST_PASS("test_same_process_double_map");
}

/**
 * 测试 5.2: 两个进程共享文件映射（模拟）
 */
void test_two_process_shared_mapping(void) {
    printf("\n--- Test 5.2: Two Process Shared Mapping ---\n");
    
    create_test_file("Shared file content between processes");
    
    struct mm_struct mm1 = {0};  // Process 1
    struct mm_struct mm2 = {0};  // Process 2
    uint64_t addr = 0x300000;
    
    // 两个进程映射同一文件
    do_mmap(&mm1, addr, PAGE_SIZE, VM_READ, 0, TEST_FILE, 0);
    do_mmap(&mm2, addr, PAGE_SIZE, VM_READ, 0, TEST_FILE, 0);
    
    // 触发两个进程的 page fault
    char buf1[64] = {0};
    char buf2[64] = {0};
    
    vm_read(&mm1, addr, buf1, 63);
    vm_read(&mm2, addr, buf2, 63);
    
    // 内容应该相同
    if (strcmp(buf1, buf2) != 0) {
        TEST_FAIL("two_proc_content", "Both processes should read same content");
        cleanup_test_file();
        return;
    }
    
    // 检查是否共享物理页
    pte_t *pte1 = pt_walk(&mm1, addr, false);
    pte_t *pte2 = pt_walk(&mm2, addr, false);
    
    if (pte1 && pte2) {
        uint64_t pfn1 = (*pte1 & PFN_MASK) >> PAGE_SHIFT;
        uint64_t pfn2 = (*pte2 & PFN_MASK) >> PAGE_SHIFT;
        
        if (pfn1 == pfn2) {
            struct page *page = pfn_to_page(pfn1);
            printf("  Page Cache working: Both processes share PFN %lu (refcount: %d)\n", 
                   pfn1, page->refcount);
        } else {
            TEST_FAIL("two_proc_pfn", "Page Cache not working - different PFNs");
            printf("  Expected same PFN, got %lu and %lu\n", pfn1, pfn2);
            cleanup_test_file();
            return;
        }
    }
    
    cleanup_test_file();
    TEST_PASS("test_two_process_shared_mapping");
}

/**
 * 测试 5.3: 共享映射写入
 */
void test_shared_mapping_write(void) {
    printf("\n--- Test 5.3: Shared Mapping Write ---\n");
    
    create_test_file("Original shared content here");
    
    struct mm_struct mm1 = {0};
    struct mm_struct mm2 = {0};
    uint64_t addr = 0x400000;
    
    // 两个进程以共享模式映射（VM_SHARED）
    do_mmap(&mm1, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_SHARED, TEST_FILE, 0);
    do_mmap(&mm2, addr, PAGE_SIZE, VM_READ | VM_WRITE, VM_SHARED, TEST_FILE, 0);
    
    // 触发映射
    char buf[64] = {0};
    vm_read(&mm1, addr, buf, 32);
    vm_read(&mm2, addr, buf, 32);
    
    // 进程1写入
    char *new_data = "Modified by Process 1!";
    int ret = vm_write(&mm1, addr, new_data, strlen(new_data) + 1);
    
    if (ret != 0) {
        TEST_FAIL("shared_write_failed", "Shared write not implemented or failed");
        cleanup_test_file();
        return;
    }
    
    // 进程2应该看到修改
    char buf2[64] = {0};
    vm_read(&mm2, addr, buf2, 63);
    
    if (strcmp(buf2, new_data) == 0) {
        printf("  Shared mapping works: Process 2 sees: '%s'\n", buf2);
    } else {
        TEST_FAIL("shared_write_visibility", "Process 2 should see shared modification");
        printf("  P2 sees '%s', expected '%s'\n", buf2, new_data);
        cleanup_test_file();
        return;
    }
    
    cleanup_test_file();
    TEST_PASS("test_shared_mapping_write");
}

/**
 * 测试 5.4: 私有映射写入（COW）
 */
void test_private_mapping_cow(void) {
    printf("\n--- Test 5.4: Private Mapping COW ---\n");
    
    create_test_file("Private mapping test content");
    
    struct mm_struct mm1 = {0};
    struct mm_struct mm2 = {0};
    uint64_t addr = 0x500000;
    
    // 两个进程以私有模式映射（不带 VM_SHARED）
    do_mmap(&mm1, addr, PAGE_SIZE, VM_READ | VM_WRITE, 0, TEST_FILE, 0);
    do_mmap(&mm2, addr, PAGE_SIZE, VM_READ | VM_WRITE, 0, TEST_FILE, 0);
    
    // 触发映射
    char buf1[64] = {0};
    char buf2[64] = {0};
    vm_read(&mm1, addr, buf1, 63);
    vm_read(&mm2, addr, buf2, 63);
    
    printf("  Initial - P1: '%.30s...'\n", buf1);
    printf("  Initial - P2: '%.30s...'\n", buf2);
    
    // 进程1写入
    char *modified = "Modified by P1 only";
    int ret = vm_write(&mm1, addr, modified, strlen(modified) + 1);
    
    if (ret != 0) {
        TEST_FAIL("private_write_failed", "Private write failed - COW not implemented");
        cleanup_test_file();
        return;
    }
    
    // 重新读取
    memset(buf1, 0, sizeof(buf1));
    memset(buf2, 0, sizeof(buf2));
    vm_read(&mm1, addr, buf1, 63);
    vm_read(&mm2, addr, buf2, 63);
    
    printf("  After write - P1: '%.30s...'\n", buf1);
    printf("  After write - P2: '%.30s...'\n", buf2);
    
    // P1 应该看到修改，P2 不应该
    if (strcmp(buf1, modified) != 0) {
        TEST_FAIL("private_cow_p1", "P1 should see its own modification");
        cleanup_test_file();
        return;
    }
    
    if (strcmp(buf2, modified) == 0) {
        TEST_FAIL("private_cow_isolation", "COW not working - P2 sees P1's modification");
        cleanup_test_file();
        return;
    } else {
        printf("  Private mapping COW works: P2 has original content\n");
    }
    
    cleanup_test_file();
    TEST_PASS("test_private_mapping_cow");
}

/**
 * 测试 5.5: Page Cache 一致性
 */
void test_page_cache_consistency(void) {
    printf("\n--- Test 5.5: Page Cache Consistency ---\n");
    
    // 创建一个有特定内容的文件
    create_test_file("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    
    struct mm_struct mm = {0};
    uint64_t addr1 = 0x600000;
    uint64_t addr2 = 0x700000;
    
    // 第一次映射并读取
    do_mmap(&mm, addr1, PAGE_SIZE, VM_READ, 0, TEST_FILE, 0);
    char buf1[16] = {0};
    vm_read(&mm, addr1, buf1, 15);
    printf("  First mapping reads: '%s'\n", buf1);
    
    // 修改文件内容（模拟外部修改）
    FILE *fp = fopen(TEST_FILE, "w");
    if (fp) {
        fprintf(fp, "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");
        fclose(fp);
    }
    
    // 第二次映射同一文件
    do_mmap(&mm, addr2, PAGE_SIZE, VM_READ, 0, TEST_FILE, 0);
    char buf2[16] = {0};
    vm_read(&mm, addr2, buf2, 15);
    printf("  Second mapping reads: '%s'\n", buf2);
    
    // 如果有 Page Cache，第一个映射的缓存可能还是旧内容
    // 如果没有 Page Cache，每次都从文件读取
    
    if (buf1[0] == 'A' && buf2[0] == 'B') {
        printf("  Each mapping reads from file directly (no cache or cache invalidated)\n");
    } else if (buf1[0] == 'A' && buf2[0] == 'A') {
        printf("  Page Cache caching the content (second read got cached 'A')\n");
    }
    
    cleanup_test_file();
    TEST_PASS("test_page_cache_consistency");
}

/**
 * 测试 5.6: 不同偏移的页缓存
 */
void test_page_cache_different_offset(void) {
    printf("\n--- Test 5.6: Page Cache Different Offset ---\n");
    
    // 创建一个大文件
    FILE *fp = fopen(TEST_FILE, "w");
    if (fp) {
        for (int i = 0; i < 300; i++) {
            fprintf(fp, "Line %04d: Content for page cache offset test.\n", i);
        }
        fclose(fp);
    }
    
    struct mm_struct mm = {0};
    uint64_t addr1 = 0x800000;
    uint64_t addr2 = 0x900000;
    
    // 映射文件的不同部分
    do_mmap(&mm, addr1, PAGE_SIZE, VM_READ, 0, TEST_FILE, 0);           // offset 0
    do_mmap(&mm, addr2, PAGE_SIZE, VM_READ, 0, TEST_FILE, PAGE_SIZE);   // offset 4096
    
    char buf1[64] = {0};
    char buf2[64] = {0};
    
    vm_read(&mm, addr1, buf1, 63);
    vm_read(&mm, addr2, buf2, 63);
    
    printf("  Offset 0 reads:    '%.40s...'\n", buf1);
    printf("  Offset 4096 reads: '%.40s...'\n", buf2);
    
    // 不同偏移应该有不同内容
    if (strcmp(buf1, buf2) == 0) {
        TEST_FAIL("cache_offset_diff", "Different offsets should have different content");
        cleanup_test_file();
        return;
    }
    
    // 两个映射应该在 Page Cache 中有不同的条目
    pte_t *pte1 = pt_walk(&mm, addr1, false);
    pte_t *pte2 = pt_walk(&mm, addr2, false);
    
    if (pte1 && pte2) {
        uint64_t pfn1 = (*pte1 & PFN_MASK) >> PAGE_SHIFT;
        uint64_t pfn2 = (*pte2 & PFN_MASK) >> PAGE_SHIFT;
        
        if (pfn1 != pfn2) {
            printf("  Correct: Different offsets use different physical pages\n");
        } else {
            TEST_FAIL("cache_offset_pfn", "Different offsets should use different pages");
            cleanup_test_file();
            return;
        }
    }
    
    cleanup_test_file();
    TEST_PASS("test_page_cache_different_offset");
}

int main(void) {
    printf("========================================\n");
    printf("  Phase 5: Page Cache Tests (Advanced)\n");
    printf("========================================\n");
    
    phys_mem_init();
    
    test_same_process_double_map();
    test_two_process_shared_mapping();
    test_shared_mapping_write();
    test_private_mapping_cow();
    test_page_cache_consistency();
    test_page_cache_different_offset();
    
    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");
    
    if (tests_failed == 0) {
        printf("\n*** PHASE 5 COMPLETE! ***\n");
        return 0;
    }
    return 1;
}
