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
static unsigned long *l2_table_low = 0;   // L2 for first 512MB (0x00000000-0x1FFFFFFF)
static unsigned long *l2_table_kernel = 0; // L2 for second 512MB (0x40000000-0x5FFFFFFF)

void mmu_init(void) {
    printf("[MMU] Initializing page tables...\n");

    // Allocate four page tables: L0, L1, L2_low, L2_kernel
    ttbr0_l0 = (unsigned long *)pmm_alloc_page();
    l1_table = (unsigned long *)pmm_alloc_page();
    l2_table_low = (unsigned long *)pmm_alloc_page();
    l2_table_kernel = (unsigned long *)pmm_alloc_page();

    if (!ttbr0_l0 || !l1_table || !l2_table_low || !l2_table_kernel) {
        printf("[MMU] Failed to allocate page tables\n");
        return;
    }

    printf("[MMU] L0 table at: %x\n", (unsigned int)(uintptr_t)ttbr0_l0);
    printf("[MMU] L1 table at: %x\n", (unsigned int)(uintptr_t)l1_table);
    printf("[MMU] L2_low table at: %x\n", (unsigned int)(uintptr_t)l2_table_low);
    printf("[MMU] L2_kernel table at: %x\n", (unsigned int)(uintptr_t)l2_table_kernel);

    // Zero all page table entries (512 entries * 8 bytes = 4096 bytes each)
    for (int i = 0; i < 512; i++) {
        WRITE_ONCE(ttbr0_l0[i], 0);
        WRITE_ONCE(l1_table[i], 0);
        WRITE_ONCE(l2_table_low[i], 0);
        WRITE_ONCE(l2_table_kernel[i], 0);
    }

    // Link L0[0] -> L1
    WRITE_ONCE(ttbr0_l0[0], (unsigned long)l1_table | PTE_TABLE | PTE_VALID);

    // L1[0] -> L2_low table (for 0x00000000-0x1FFFFFFF, first 512MB)
    WRITE_ONCE(l1_table[0], (unsigned long)l2_table_low | PTE_TABLE | PTE_VALID);

    // Fill L2_low with identity mapping for first 512MB (256 entries * 2MB each)
    for (unsigned int i = 0; i < 256; i++) {
        unsigned long phys_addr = ((unsigned long)i) << 21;  // i * 2MB

        // Use device memory for MMIO regions, normal memory for RAM
        unsigned long attr;
        if (phys_addr >= 0x08000000 && phys_addr < 0x10000000) {
            // Device region (GIC at 0x08000000, UART at 0x09000000)
            attr = MT_DEVICE_nGnRnE;
        } else {
            // Normal memory
            attr = MT_NORMAL;
        }

        unsigned long entry = phys_addr | (attr << 2) | PTE_BLOCK | PTE_AF | PTE_VALID;
        WRITE_ONCE(l2_table_low[i], entry);
    }

    // L1[1] -> L2_kernel table (for 0x40000000-0x5FFFFFFF, covers kernel)
    WRITE_ONCE(l1_table[1], (unsigned long)l2_table_kernel | PTE_TABLE | PTE_VALID);

    // Fill L2_kernel with identity mapping for 512MB starting at 0x40000000
    for (unsigned int i = 0; i < 256; i++) {
        unsigned long phys_addr = 0x40000000UL + (((unsigned long)i) << 21);  // 0x40000000 + i * 2MB

        unsigned long entry = phys_addr | (MT_NORMAL << 2) | PTE_BLOCK | PTE_AF | PTE_VALID;
        WRITE_ONCE(l2_table_kernel[i], entry);
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
    return l2_table_low;  // Return the low memory L2 table for testing
}
