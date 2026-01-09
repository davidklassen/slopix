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
        printf("[MMU] Failed to allocate page table\n");
        while (1);
    }
    return (unsigned long *)page;
}

void mmu_init(void) {
    printf("[MMU] Initializing page tables...\n");

    // Allocate level 0 page tables for TTBR0 and TTBR1
    ttbr0_l0 = alloc_page_table();
    ttbr1_l0 = alloc_page_table();

    // Setup identity map for TTBR0 (0x0 -> 0x0)
    // This maps the first 1GB of physical memory
    unsigned long *ttbr0_l1 = alloc_page_table();
    ttbr0_l0[0] = (unsigned long)ttbr0_l1 | PTE_TABLE | PTE_VALID;

    unsigned long *ttbr0_l2 = alloc_page_table();
    ttbr0_l1[0] = (unsigned long)ttbr0_l2 | PTE_TABLE | PTE_VALID;

    // Map first 512 MB using 2MB blocks (256 entries)
    for (int i = 0; i < 256; i++) {
        unsigned long phys_addr = i * (2 * 1024 * 1024);
        unsigned long attrs = (MT_NORMAL << 2);  // Normal memory
        ttbr0_l2[i] = phys_addr | attrs | PTE_BLOCK | PTE_AF | PTE_VALID;
    }

    // Setup high memory map for TTBR1 (0xFFFF000000000000 -> 0x40000000)
    // For 39-bit addresses, we use entry 511 of L0 table
    unsigned long *ttbr1_l1 = alloc_page_table();
    ttbr1_l0[511] = (unsigned long)ttbr1_l1 | PTE_TABLE | PTE_VALID;

    // Entry 0 of L1 for the first GB
    unsigned long *ttbr1_l2 = alloc_page_table();
    ttbr1_l1[0] = (unsigned long)ttbr1_l2 | PTE_TABLE | PTE_VALID;

    // Map kernel: 0xFFFF000000000000 -> 0x40000000
    // Start at entry that corresponds to 0x40000000 (entry 32 for 2MB blocks)
    for (int i = 0; i < 256; i++) {
        unsigned long phys_addr = 0x40000000 + (i * (2 * 1024 * 1024));
        unsigned long attrs = (MT_NORMAL << 2);  // Normal memory
        ttbr1_l2[32 + i] = phys_addr | attrs | PTE_BLOCK | PTE_AF | PTE_VALID;
    }

    // Also map UART and GIC as device memory in TTBR0
    // UART at 0x09000000, GIC at 0x08000000
    // These are in the first 1GB, so already covered by identity map above
    // But we need to fix their memory type to device
    // Entry for 0x08000000 (entry 4 in 2MB blocks)
    unsigned long attrs_device = (MT_DEVICE_nGnRnE << 2);
    ttbr0_l2[4] = 0x08000000 | attrs_device | PTE_BLOCK | PTE_AF | PTE_VALID;  // GIC
    // Entry for 0x09000000 (entry 4, offset 0x01000000 / 2MB = entry 4 + 0.5)
    // Actually 0x09000000 / 2MB = entry 4.5, so it's in entry 4's block
    // We need finer granularity - let's keep it simple for now

    printf("[MMU] TTBR0 (identity): %x\n", ttbr0_l0);
    printf("[MMU] TTBR1 (kernel high): %x\n", ttbr1_l0);
}

unsigned long mmu_get_ttbr0(void) {
    return (unsigned long)ttbr0_l0;
}

unsigned long mmu_get_ttbr1(void) {
    return (unsigned long)ttbr1_l0;
}
