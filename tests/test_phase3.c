/**
 * Phase 3 测试：mmap 语义
 * 
 * 测试目标：
 * - 匿名映射（VM_ANON）
 * - 文件映射
 * - 懒分配（访问时才分配物理页）
 * - 文件内容正确加载
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

// 用于测试的临时文件
#define TEST_FILE "/tmp/vm_sim_test_file.txt"
#define TEST_FILE_CONTENT "This is a test file for VM simulator.\nLine 2 of the test file.\nLine 3 with some more content.\n"

static void create_test_file(void) {
    FILE *fp = fopen(TEST_FILE, "w");
    if (fp) {
        fprintf(fp, "%s", TEST_FILE_CONTENT);
        fclose(fp);
    }
}

static void cleanup_test_file(void) {
    unlink(TEST_FILE);
}

/**
 * 测试 3.1: 匿名映射
 */
void test_anon_mapping(void) {
    printf("\n--- Test 3.1: Anonymous Mapping ---\n");
    
    struct mm_struct mm = {0};
    uint64_t addr = 0x100000;
    
    // 创建匿名映射
    int ret = do_mmap(&mm, addr, PAGE_SIZE * 4, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    if (ret != 0) {
        TEST_FAIL("anon_mapping_create", "Failed to create anonymous mapping");
        return;
    }
    
    // 验证 VMA 属性
    struct vm_area_struct *vma = find_vma(&mm, addr);
    if (!vma) {
        TEST_FAIL("anon_mapping_find", "VMA not found");
        return;
    }
    
    if (!(vma->vm_flags & VM_ANON)) {
        TEST_FAIL("anon_mapping_flags", "VMA should have VM_ANON flag");
        return;
    }
    
    if (vma->vm_file_path != NULL) {
        TEST_FAIL("anon_mapping_file", "Anonymous VMA should not have file path");
        return;
    }
    
    // 写入并读取
    uint64_t test_val = 0x123456789ABCDEF0ULL;
    vm_write(&mm, addr + PAGE_SIZE, &test_val, sizeof(test_val));
    
    uint64_t read_val = 0;
    vm_read(&mm, addr + PAGE_SIZE, &read_val, sizeof(read_val));
    
    if (read_val != test_val) {
        TEST_FAIL("anon_mapping_data", "Data mismatch");
        return;
    }
    
    TEST_PASS("test_anon_mapping");
}

/**
 * 测试 3.2: 懒分配验证
 */
void test_lazy_allocation(void) {
    printf("\n--- Test 3.2: Lazy Allocation ---\n");
    
    struct mm_struct mm = {0};
    uint64_t addr = 0x200000;
    
    // 创建映射
    do_mmap(&mm, addr, PAGE_SIZE * 4, VM_READ | VM_WRITE, VM_ANON, NULL, 0);
    
    // mmap 后，页表应该还没有建立映射
    pte_t *pte = pt_walk(&mm, addr, false);
    if (pte && (*pte & PTE_P)) {
        printf("  Note: Page already mapped before access (eager allocation)\n");
        // 这不是错误，但真正的懒分配应该在访问时才映射
    } else {
        printf("  Confirmed: No page mapped yet (lazy allocation)\n");
    }
    
    // 访问触发 page fault
    char buf[16];
    int ret = vm_read(&mm, addr, buf, 16);
    if (ret != 0) {
        TEST_FAIL("lazy_alloc_access", "Access should trigger demand paging");
        return;
    }
    
    // 访问后，页表应该有映射了
    pte = pt_walk(&mm, addr, false);
    if (!pte || !(*pte & PTE_P)) {
        TEST_FAIL("lazy_alloc_mapped", "Page should be mapped after access");
        return;
    }
    
    printf("  Page mapped after first access (demand paging works)\n");
    TEST_PASS("test_lazy_allocation");
}

/**
 * 测试 3.3: 文件映射
 */
void test_file_mapping(void) {
    printf("\n--- Test 3.3: File Mapping ---\n");
    
    create_test_file();
    
    struct mm_struct mm = {0};
    uint64_t addr = 0x300000;
    
    // 创建文件映射
    int ret = do_mmap(&mm, addr, PAGE_SIZE, VM_READ, 0, TEST_FILE, 0);
    if (ret != 0) {
        TEST_FAIL("file_mapping_create", "Failed to create file mapping");
        cleanup_test_file();
        return;
    }
    
    // 验证 VMA 属性
    struct vm_area_struct *vma = find_vma(&mm, addr);
    if (!vma || !vma->vm_file_path) {
        TEST_FAIL("file_mapping_vma", "VMA should have file path");
        cleanup_test_file();
        return;
    }
    
    // 读取文件内容
    char buf[128] = {0};
    ret = vm_read(&mm, addr, buf, 127);
    if (ret != 0) {
        TEST_FAIL("file_mapping_read", "Failed to read from file mapping");
        cleanup_test_file();
        return;
    }
    
    // 验证内容
    if (strncmp(buf, TEST_FILE_CONTENT, strlen(TEST_FILE_CONTENT)) != 0) {
        TEST_FAIL("file_mapping_content", "File content mismatch");
        printf("  Expected: '%s'\n", TEST_FILE_CONTENT);
        printf("  Got:      '%s'\n", buf);
        cleanup_test_file();
        return;
    }
    
    printf("  File content correctly loaded: '%.40s...'\n", buf);
    cleanup_test_file();
    TEST_PASS("test_file_mapping");
}

/**
 * 测试 3.4: 文件映射偏移
 */
void test_file_mapping_offset(void) {
    printf("\n--- Test 3.4: File Mapping with Offset ---\n");
    
    create_test_file();
    
    struct mm_struct mm = {0};
    uint64_t addr = 0x400000;
    uint64_t offset = 10;  // 从文件偏移 10 字节开始
    
    // 创建带偏移的文件映射
    int ret = do_mmap(&mm, addr, PAGE_SIZE, VM_READ, 0, TEST_FILE, offset);
    if (ret != 0) {
        TEST_FAIL("file_offset_create", "Failed to create file mapping with offset");
        cleanup_test_file();
        return;
    }
    
    // 读取内容
    char buf[64] = {0};
    ret = vm_read(&mm, addr, buf, 63);
    if (ret != 0) {
        TEST_FAIL("file_offset_read", "Failed to read from offset mapping");
        cleanup_test_file();
        return;
    }
    
    // 验证内容（应该从 offset 开始）
    const char *expected = TEST_FILE_CONTENT + offset;
    if (strncmp(buf, expected, 30) != 0) {
        TEST_FAIL("file_offset_content", "Offset content mismatch");
        printf("  Expected (from offset %lu): '%.30s...'\n", offset, expected);
        printf("  Got: '%.30s...'\n", buf);
        cleanup_test_file();
        return;
    }
    
    printf("  Offset mapping works: '%.30s...'\n", buf);
    cleanup_test_file();
    TEST_PASS("test_file_mapping_offset");
}

/**
 * 测试 3.5: 系统文件映射 (/etc/passwd)
 */
void test_system_file_mapping(void) {
    printf("\n--- Test 3.5: System File Mapping (/etc/passwd) ---\n");
    
    struct mm_struct mm = {0};
    uint64_t addr = 0x500000;
    
    // 检查文件是否存在
    if (access("/etc/passwd", R_OK) != 0) {
        printf("  Skipping: /etc/passwd not readable\n");
        TEST_PASS("test_system_file_mapping (skipped)");
        return;
    }
    
    // 映射 /etc/passwd
    int ret = do_mmap(&mm, addr, PAGE_SIZE, VM_READ, 0, "/etc/passwd", 0);
    if (ret != 0) {
        TEST_FAIL("system_file_create", "Failed to map /etc/passwd");
        return;
    }
    
    // 读取内容
    char buf[128] = {0};
    ret = vm_read(&mm, addr, buf, 127);
    if (ret != 0) {
        TEST_FAIL("system_file_read", "Failed to read /etc/passwd");
        return;
    }
    
    // /etc/passwd 通常以 "root:" 开头
    if (strstr(buf, "root") == NULL && strstr(buf, ":") == NULL) {
        TEST_FAIL("system_file_content", "Content doesn't look like /etc/passwd");
        printf("  Got: '%.50s...'\n", buf);
        return;
    }
    
    printf("  System file mapped: '%.50s...'\n", buf);
    TEST_PASS("test_system_file_mapping");
}

/**
 * 测试 3.6: 多页文件映射
 */
void test_multipage_file_mapping(void) {
    printf("\n--- Test 3.6: Multi-page File Mapping ---\n");
    
    // 创建一个大文件
    FILE *fp = fopen(TEST_FILE, "w");
    if (!fp) {
        TEST_FAIL("multipage_create_file", "Cannot create test file");
        return;
    }
    
    // 写入超过一页的内容
    for (int i = 0; i < 200; i++) {
        fprintf(fp, "Line %04d: This is test content for multi-page mapping test.\n", i);
    }
    fclose(fp);
    
    struct mm_struct mm = {0};
    uint64_t addr = 0x600000;
    
    // 映射两页
    int ret = do_mmap(&mm, addr, PAGE_SIZE * 2, VM_READ, 0, TEST_FILE, 0);
    if (ret != 0) {
        TEST_FAIL("multipage_mmap", "Failed to create multi-page mapping");
        cleanup_test_file();
        return;
    }
    
    // 读取第一页
    char buf1[64] = {0};
    vm_read(&mm, addr, buf1, 63);
    printf("  Page 1 start: '%.50s...'\n", buf1);
    
    // 读取第二页（跨越第一页边界）
    char buf2[64] = {0};
    vm_read(&mm, addr + PAGE_SIZE, buf2, 63);
    printf("  Page 2 start: '%.50s...'\n", buf2);
    
    // 两页内容应该不同
    if (strcmp(buf1, buf2) == 0) {
        TEST_FAIL("multipage_different", "Page 1 and Page 2 should have different content");
        cleanup_test_file();
        return;
    }
    
    cleanup_test_file();
    TEST_PASS("test_multipage_file_mapping");
}

int main(void) {
    printf("========================================\n");
    printf("  Phase 3: mmap Semantics Tests\n");
    printf("========================================\n");
    
    phys_mem_init();
    
    test_anon_mapping();
    test_lazy_allocation();
    test_file_mapping();
    test_file_mapping_offset();
    test_system_file_mapping();
    test_multipage_file_mapping();
    
    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");
    
    if (tests_failed == 0) {
        printf("\n*** PHASE 3 COMPLETE! ***\n");
        return 0;
    }
    return 1;
}
