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
// Note: For 39-bit VA (T0SZ=25), translation starts at Level 1, not Level 0
static unsigned long *l1_table = 0;        // Level 1 table (starting level, pointed to by TTBR0)
static unsigned long *ttbr1_l0 = 0;
static unsigned long *l2_table_low = 0;    // Level 2 for first 1GB (0x00000000-0x3FFFFFFF)
static unsigned long *l2_table_kernel = 0; // Level 2 for second 1GB (0x40000000-0x7FFFFFFF)

void mmu_init(void) {
    printf("[MMU] Initializing page tables...\n");

    // Allocate three page tables: L1, L2_low, L2_kernel
    // For 39-bit VA with T0SZ=25, translation starts at Level 1 (not Level 0)
    l1_table = (unsigned long *)pmm_alloc_page();
    l2_table_low = (unsigned long *)pmm_alloc_page();
    l2_table_kernel = (unsigned long *)pmm_alloc_page();

    if (!l1_table || !l2_table_low || !l2_table_kernel) {
        printf("[MMU] Failed to allocate page tables\n");
        return;
    }

    printf("[MMU] L1 table at: %x\n", (unsigned int)(uintptr_t)l1_table);
    printf("[MMU] L2_low table at: %x\n", (unsigned int)(uintptr_t)l2_table_low);
    printf("[MMU] L2_kernel table at: %x\n", (unsigned int)(uintptr_t)l2_table_kernel);

    // Zero all page table entries (512 entries * 8 bytes = 4096 bytes each)
    for (int i = 0; i < 512; i++) {
        WRITE_ONCE(l1_table[i], 0);
        WRITE_ONCE(l2_table_low[i], 0);
        WRITE_ONCE(l2_table_kernel[i], 0);
    }

    // L1[0] -> L2_low table (for 0x00000000-0x3FFFFFFF, first 1GB)
    // VA[38:30] = 0 will use this entry
    unsigned long l1_entry_low = (unsigned long)l2_table_low | PTE_TABLE | PTE_VALID;
    WRITE_ONCE(l1_table[0], l1_entry_low);
    printf("[MMU] L1[0] entry = 0x%x (points to L2_low at 0x%x)\n",
           (unsigned int)l1_table[0], (unsigned int)l2_table_low);

    // Fill L2_low with identity mapping for first 1GB (512 entries * 2MB each)
    for (unsigned int i = 0; i < 512; i++) {
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

    // L1[1] -> L2_kernel table (for 0x40000000-0x7FFFFFFF, second 1GB)
    // VA[38:30] = 1 will use this entry (covers kernel at 0x40000000)
    unsigned long l1_entry_kernel = (unsigned long)l2_table_kernel | PTE_TABLE | PTE_VALID;
    WRITE_ONCE(l1_table[1], l1_entry_kernel);
    printf("[MMU] L1[1] entry = 0x%x (points to L2_kernel at 0x%x)\n",
           (unsigned int)l1_table[1], (unsigned int)l2_table_kernel);

    // Fill L2_kernel with identity mapping for 1GB starting at 0x40000000
    for (unsigned int i = 0; i < 512; i++) {
        unsigned long phys_addr = 0x40000000UL + (((unsigned long)i) << 21);  // 0x40000000 + i * 2MB

        unsigned long entry = phys_addr | (MT_NORMAL << 2) | PTE_BLOCK | PTE_AF | PTE_VALID;
        WRITE_ONCE(l2_table_kernel[i], entry);
    }

    // TTBR1 not used yet
    ttbr1_l0 = 0;

    // CPU-level memory barrier: ensure all page table writes reach memory before returning
    // DSB (Data Synchronization Barrier) forces completion of all pending writes
    __asm__ volatile("dsb sy" ::: "memory");

    printf("[MMU] Page tables initialized (MMU still disabled)\n");
}

unsigned long mmu_get_ttbr0(void) {
    return (unsigned long)l1_table;  // L1 table is the starting level for TTBR0
}

unsigned long mmu_get_ttbr1(void) {
    return (unsigned long)ttbr1_l0;
}

unsigned long* mmu_get_l2_table(void) {
    return l2_table_low;  // Return the low memory L2 table for testing
}
