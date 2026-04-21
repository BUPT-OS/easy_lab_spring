#include "vm_defs.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

uint8_t g_phys_mem[PHYS_MEM_SIZE];
struct page g_pages[NUM_PHYS_PAGES];

static struct page *free_list = NULL;

void phys_mem_init(void)
{
    memset(g_phys_mem, 0, PHYS_MEM_SIZE);
    memset(g_pages, 0, sizeof(g_pages));

    // Initialize free list
    for (int i = 0; i < NUM_PHYS_PAGES; i++) {
        g_pages[i].refcount = 0;
        // Check for reserved pages if any (not needed for this sim)

        // Link to free list
        g_pages[i].lru_next = free_list;
        free_list = &g_pages[i];
    }
    printf("[Phys] Initialized %d physical pages (%d MB)\n", NUM_PHYS_PAGES,
           PHYS_MEM_SIZE / (1024 * 1024));
}

struct page *alloc_page(void)
{
    if (!free_list) {
        printf("[Phys] OOM: No free pages!\n");
        return NULL;
    }

    // TODO: Phase 1 - Implement physical page allocation

    return NULL;
}

void free_page(struct page *p)
{
    // TODO: Phase 1 - Implement physical page freeing
}

void *page_to_virt(struct page *page)
{
    uint64_t idx = page - g_pages;
    return (void *) &g_phys_mem[idx * PAGE_SIZE];
}

struct page *pfn_to_page(uint64_t pfn)
{
    if (pfn >= NUM_PHYS_PAGES)
        return NULL;
    return &g_pages[pfn];
}

uint64_t page_to_pfn(struct page *page)
{
    return page - g_pages;
}
