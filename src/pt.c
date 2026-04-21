#include "vm_defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper macros for 4-level page table
#define PGDIR_SHIFT 39
#define PUD_SHIFT 30
#define PMD_SHIFT 21
#define PT_SHIFT 12
#define PTRS_PER_PGD 512
#define PTRS_PER_PUD 512
#define PTRS_PER_PMD 512
#define PTRS_PER_PTE 512

#define PGD_INDEX(x) (((x) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))
#define PUD_INDEX(x) (((x) >> PUD_SHIFT) & (PTRS_PER_PUD - 1))
#define PMD_INDEX(x) (((x) >> PMD_SHIFT) & (PTRS_PER_PMD - 1))
#define PTE_INDEX(x) (((x) >> PT_SHIFT) & (PTRS_PER_PTE - 1))

void pt_init(struct mm_struct *mm)
{
    // Allocate the top-level PGD
    struct page *p = alloc_page();
    if (!p) {
        fprintf(stderr, "Failed to allocate PGD\n");
        exit(1);
    }
    mm->pgd = (pte_t *) page_to_virt(p);
}

// Get the pointer to the PTE for address 'addr'.
// If 'alloc' is true, create intermediate levels.
pte_t *pt_walk(struct mm_struct *mm, uint64_t addr, bool alloc)
{
    if (!mm->pgd)
        pt_init(mm);

    pte_t *pgd = mm->pgd;
    pte_t *pud, *pmd, *pte;
    struct page *page;

    // TODO: Phase 2 - Implement 4-level page table walk

    return NULL;
}

int pt_map_page(struct mm_struct *mm, uint64_t addr, uint64_t pfn, uint32_t flags)
{
    pte_t *pte = pt_walk(mm, addr, true);
    if (!pte)
        return -1;

    // TODO: Phase 2 - Map the physical page

    return 0;
}
