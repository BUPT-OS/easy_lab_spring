#include "vm_defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct vm_area_struct *find_vma(struct mm_struct *mm, uint64_t addr)
{
    // Check cache first
    if (mm->mmap_cache && addr >= mm->mmap_cache->vm_start && addr < mm->mmap_cache->vm_end) {
        return mm->mmap_cache;
    }

    struct vm_area_struct *vma = mm->mmap;
    while (vma) {
        if (addr >= vma->vm_start && addr < vma->vm_end) {
            mm->mmap_cache = vma;
            return vma;
        }
        vma = vma->vm_next;
    }
    return NULL;
}

// Simple linear search for free space (first-fit)
static uint64_t get_unmapped_area(struct mm_struct *mm, uint64_t len)
{
    uint64_t addr = 0x400000; // Start at 4MB to simulate standard layout

    // Very simple check: try to append after the last VMA
    // In a real kernel, we'd check gaps between VMAs
    if (!mm->mmap)
        return addr;

    struct vm_area_struct *vma = mm->mmap;
    while (vma->vm_next) {
        vma = vma->vm_next;
    }
    // Check for integer overflow in a real impl
    return (vma->vm_end + 0xFFF) & ~0xFFF; // Align next
}

int do_mmap(struct mm_struct *mm, uint64_t addr, uint64_t len, uint32_t prot, uint32_t flags,
            const char *filepath, uint64_t offset)
{
    // 1. Align length to page size
    len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (len == 0)
        return -1;

    // 2. Find a free address range if addr is 0
    if (addr == 0) {
        addr = get_unmapped_area(mm, len);
    } else {
        // Validation: must be page aligned
        if (addr & (PAGE_SIZE - 1))
            return -1;
        // Validation: overlap check (omitted for brevity in this simple version)
    }

    // TODO: Phase 3 - Create and insert VMA

    printf("[MM] mmap: [%lx - %lx] Prot:%x Flags:%x %s\n", addr, addr + len, prot, flags,
           filepath ? filepath : "[anon]");

    return 0;
}

void print_vmas(struct mm_struct *mm)
{
    struct vm_area_struct *vma = mm->mmap;
    printf("--- Process VMAs ---\n");
    while (vma) {
        printf("%012lx - %012lx  %c%c%c  %s\n", vma->vm_start, vma->vm_end,
               (vma->vm_prot & VM_READ) ? 'r' : '-', (vma->vm_prot & VM_WRITE) ? 'w' : '-',
               (vma->vm_prot & VM_EXEC) ? 'x' : '-',
               vma->vm_file_path ? vma->vm_file_path : "[anon]");
        vma = vma->vm_next;
    }
    printf("--------------------\n");
}
