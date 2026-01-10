#include "mmu.h"
#include "pmm.h"
#include "printf.h"
#include <stdint.h>
#include "memory.h"

// Prevent compiler from optimizing away writes to memory accessed by hardware
// (page tables are read by MMU, not C code, so compiler thinks writes are dead stores)
#define WRITE_ONCE(var, val) \
    (*((volatile typeof(val) *)&(var)) = (val))

// Page table pointers
static unsigned long *ttbr0_l0 = 0;
static unsigned long *ttbr1_l0 = 0;
static unsigned long *l1_table = 0;
static unsigned long *l2_table = 0;

void mmu_init(void) {
    printf("[MMU] Initializing page tables...\n");

    // Allocate three page tables: L0, L1, L2
    ttbr0_l0 = (unsigned long *)pmm_alloc_page();
    l1_table = (unsigned long *)pmm_alloc_page();
    l2_table = (unsigned long *)pmm_alloc_page();

    if (!ttbr0_l0 || !l1_table || !l2_table) {
        printf("[MMU] Failed to allocate page tables\n");
        return;
    }

    printf("[MMU] L0 table at: %x\n", (unsigned int)(uintptr_t)ttbr0_l0);
    printf("[MMU] L1 table at: %x\n", (unsigned int)(uintptr_t)l1_table);
    printf("[MMU] L2 table at: %x\n", (unsigned int)(uintptr_t)l2_table);

    // Zero all page table entries (512 entries * 8 bytes = 4096 bytes each)
    for (int i = 0; i < 512; i++) {
        WRITE_ONCE(ttbr0_l0[i], 0);
        WRITE_ONCE(l1_table[i], 0);
        WRITE_ONCE(l2_table[i], 0);
    }

    // Link L0[0] -> L1
    WRITE_ONCE(ttbr0_l0[0], (unsigned long)l1_table | PTE_TABLE | PTE_VALID);
    // Link L1[0] -> L2
    WRITE_ONCE(l1_table[0], (unsigned long)l2_table | PTE_TABLE | PTE_VALID);

    // Fill L2 with identity mapping for first 256MB (128 entries * 2MB each)
    // Each L2 entry maps 2MB using block descriptors
    for (unsigned int i = 0; i < 128; i++) {
        unsigned long phys_addr = ((unsigned long)i) << 21;  // 2MB blocks (shift left 21 bits)
        unsigned long entry = phys_addr | (MT_NORMAL << 2) | PTE_BLOCK | PTE_AF | PTE_VALID;
        WRITE_ONCE(l2_table[i], entry);
    }

    // TTBR1 not used yet
    ttbr1_l0 = 0;

    // Memory barrier: ensure all page table writes complete before returning
    __asm__ volatile("" ::: "memory");

    printf("[MMU] Page tables initialized (MMU still disabled)\n");
}

unsigned long mmu_get_ttbr0(void) {
    return (unsigned long)ttbr0_l0;
}

unsigned long mmu_get_ttbr1(void) {
    return (unsigned long)ttbr1_l0;
}

unsigned long* mmu_get_l2_table(void) {
    return l2_table;
}
