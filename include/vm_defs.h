#ifndef VM_DEFS_H
#define VM_DEFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* --- Configuration --- */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PFN_MASK 0x0000FFFFFFFFF000ULL
#define PHYS_MEM_SIZE (16 * 1024 * 1024) // 16MB simulated RAM
#define NUM_PHYS_PAGES (PHYS_MEM_SIZE / PAGE_SIZE)

/* --- Page Table Flags --- */
#define PTE_P 0x001 // Present
#define PTE_W 0x002 // Writable
#define PTE_U 0x004 // User
#define PTE_A 0x020 // Accessed
#define PTE_D 0x040 // Dirty

/* --- VMA Flags --- */
#define VM_READ 0x001
#define VM_WRITE 0x002
#define VM_EXEC 0x004
#define VM_SHARED 0x008
#define VM_ANON 0x010

/* --- Access Types for Simulation --- */
#define ACC_READ 0
#define ACC_WRITE 1
#define ACC_FETCH 2

/* --- Core Data Structures --- */

// 64-bit Page Table Entry
// [63:N] Reserved | [M:12] PFN | [11:0] Flags
typedef uint64_t pte_t;

// Physical Page Frame metadata
struct page {
    uint32_t refcount;
    uint32_t flags;
    // For simple LRU
    struct page *lru_next;
    struct page *lru_prev;
};

// Virtual Memory Area (VMA)
struct vm_area_struct {
    uint64_t vm_start;
    uint64_t vm_end;
    uint32_t vm_prot;  // R/W/X
    uint32_t vm_flags; // Shared/Anon

    // For file mappings (simplified)
    char *vm_file_path;
    uint64_t vm_pgoff;

    struct vm_area_struct *vm_next;
    struct vm_area_struct *vm_prev;
};

// Process Memory Descriptor
struct mm_struct {
    pte_t *pgd;                        // Pointer to Page Global Directory (Level 4)
    struct vm_area_struct *mmap;       // List of VMAs
    struct vm_area_struct *mmap_cache; // Simple optimization
};

/* --- Global Simulation State --- */
extern uint8_t g_phys_mem[PHYS_MEM_SIZE];
extern struct page g_pages[NUM_PHYS_PAGES];

/* --- API Prototypes --- */

// Physical Memory
void phys_mem_init(void);
struct page *alloc_page(void);
void free_page(struct page *page);
void *page_to_virt(struct page *page); // Get pointer to simulated RAM buffer
struct page *pfn_to_page(uint64_t pfn);
uint64_t page_to_pfn(struct page *page);

// Page Table Operations
void pt_init(struct mm_struct *mm);
pte_t *pt_walk(struct mm_struct *mm, uint64_t addr, bool alloc);
int pt_map_page(struct mm_struct *mm, uint64_t addr, uint64_t pfn, uint32_t flags);

// VMA Management
struct vm_area_struct *find_vma(struct mm_struct *mm, uint64_t addr);
int do_mmap(struct mm_struct *mm, uint64_t addr, uint64_t len, uint32_t prot, uint32_t flags,
            const char *filepath, uint64_t offset);
void print_vmas(struct mm_struct *mm);

// Fault Handling
int handle_page_fault(struct mm_struct *mm, uint64_t addr, int access_type);

// Hardware Simulation (MMU)
// Returns 0 on success, -1 on fault (triggering internal fault handler attempt)
int mmu_translate(struct mm_struct *mm, uint64_t addr, int access_type, uint64_t *paddr_out);
// User-facing accessors (simulate CPU load/store)
int vm_read(struct mm_struct *mm, uint64_t addr, void *buf, size_t len);
int vm_write(struct mm_struct *mm, uint64_t addr, void *buf, size_t len);

#endif
