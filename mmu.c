#include "mmu.h"
#include "memory.h"
#include "pmm.h"
#include "printf.h"

// Page tables - 3 levels for 39-bit address space
// Level 0: 512 GB per entry
// Level 1: 1 GB per entry
// Level 2: 2 MB per entry (using blocks)
static unsigned long *ttbr0_l0;  // User space (low addresses)
static unsigned long *ttbr1_l0;  // Kernel space (high addresses)

static unsigned long *alloc_page_table(void) {
    void *page = pmm_alloc_page();
    if (!page) {
        printf("[MMU] ERROR: Failed to allocate page table\n");
        while (1);
    }

    // Zero the page table
    unsigned long *entries = (unsigned long *)page;
    for (int i = 0; i < 512; i++) {
        entries[i] = 0;
    }

    return (unsigned long *)page;
}

void mmu_init(void) {
    printf("[MMU] Initializing page tables...\n");

    // Allocate level 0 page tables for TTBR0 and TTBR1
    ttbr0_l0 = alloc_page_table();
    ttbr1_l0 = alloc_page_table();
    printf("[MMU] Allocated L0 tables\n");

    // Setup identity map for TTBR0 (0x0 -> 0x0)
    unsigned long *ttbr0_l1 = alloc_page_table();
    ttbr0_l0[0] = (unsigned long)ttbr0_l1 | PTE_TABLE | PTE_VALID;

    unsigned long *ttbr0_l2 = alloc_page_table();
    ttbr0_l1[0] = (unsigned long)ttbr0_l2 | PTE_TABLE | PTE_VALID;
    printf("[MMU] Allocated L1/L2 for TTBR0\n");

    // Map first 512 MB using 2MB blocks
    // Use device memory for GIC (0x08000000) and UART (0x09000000) regions
    unsigned long attrs_normal = (MT_NORMAL << 2);
    unsigned long attrs_device = (MT_DEVICE_nGnRnE << 2);

    // Entry 0-63: Normal memory (0x00000000-0x07FFFFFF = 0-128MB)
    int i;
    for (i = 0; i < 64; i++) {
        ttbr0_l2[i] = (i * 0x200000) | attrs_normal | PTE_BLOCK | PTE_AF | PTE_VALID;
    }
    // Entry 64-72: Device memory (0x08000000-0x09FFFFFF = 128MB-160MB, covers GIC and UART)
    for (i = 64; i < 73; i++) {
        ttbr0_l2[i] = (i * 0x200000) | attrs_device | PTE_BLOCK | PTE_AF | PTE_VALID;
    }
    // Entry 73-255: Normal memory (0x0A000000-0x1FFFFFFF = 160MB-512MB)
    for (i = 73; i < 256; i++) {
        ttbr0_l2[i] = (i * 0x200000) | attrs_normal | PTE_BLOCK | PTE_AF | PTE_VALID;
    }
    printf("[MMU] Mapped 512MB identity\n");

    // Setup TTBR1 (high addresses) - minimal setup, not used yet
    unsigned long *ttbr1_l1 = alloc_page_table();
    ttbr1_l0[511] = (unsigned long)ttbr1_l1 | PTE_TABLE | PTE_VALID;
    printf("[MMU] Setup TTBR1\n");

    printf("[MMU] Page tables ready\n");
}

unsigned long mmu_get_ttbr0(void) {
    return (unsigned long)ttbr0_l0;
}

unsigned long mmu_get_ttbr1(void) {
    return (unsigned long)ttbr1_l0;
}
