#include "vm_defs.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simulate Hardware MMU Translation
int mmu_translate(struct mm_struct *mm, uint64_t addr, int access_type, uint64_t *paddr_out)
{
    pte_t *pte = pt_walk(mm, addr, false); // Do not alloc in hardware walk

    // TODO: Phase 2 - Implement MMU Translation logic

    return -1; // Default failure (triggers fault handler)
}

// The Kernel Page Fault Handler
int handle_page_fault(struct mm_struct *mm, uint64_t addr, int access_type)
{
    printf("[Kernel] Page Fault @ %lx (Access: %d)\n", addr, access_type);

    // 1. Find VMA
    struct vm_area_struct *vma = find_vma(mm, addr);
    if (!vma) {
        printf("[Kernel] Segfault: No VMA for %lx\n", addr);
        return -1;
    }

    // TODO: Phase 4 - Implement Page Fault Handling

    return -1; // Fail if not handled
}

// User-space access simulation
int vm_read(struct mm_struct *mm, uint64_t addr, void *buf, size_t len)
{
    size_t done = 0;
    while (done < len) {
        uint64_t paddr;
        if (mmu_translate(mm, addr + done, ACC_READ, &paddr) != 0) {
            if (handle_page_fault(mm, addr + done, ACC_READ) != 0)
                return -1;
            continue; // Retry instruction
        }

        // Physical Memory Access
        uint64_t pfn = paddr >> PAGE_SHIFT;
        uint64_t off = paddr & (PAGE_SIZE - 1);

        // Simple byte copy
        ((uint8_t *) buf)[done] = g_phys_mem[pfn * PAGE_SIZE + off];
        done++;
    }
    return 0;
}

int vm_write(struct mm_struct *mm, uint64_t addr, void *buf, size_t len)
{
    size_t done = 0;
    while (done < len) {
        uint64_t paddr;
        if (mmu_translate(mm, addr + done, ACC_WRITE, &paddr) != 0) {
            if (handle_page_fault(mm, addr + done, ACC_WRITE) != 0)
                return -1;
            continue; // Retry instruction
        }

        uint64_t pfn = paddr >> PAGE_SHIFT;
        uint64_t off = paddr & (PAGE_SIZE - 1);

        g_phys_mem[pfn * PAGE_SIZE + off] = ((uint8_t *) buf)[done];
        done++;
    }
    return 0;
}
