#ifndef MMU_H
#define MMU_H

#include "mem.h"

// Page table sizes
#define PTE_PER_TABLE 512

// Page table index extraction (4KB granule, 48-bit VA)
#define L0_INDEX(va)   (((va) >> 39) & 0x1FF)
#define L1_INDEX(va)   (((va) >> 30) & 0x1FF)
#define L2_INDEX(addr) (((addr) >> 21) & 0x1FF)
#define L3_INDEX(va)   (((va) >> 12) & 0x1FF)

// Page table entry type (64-bit on AArch64)
typedef unsigned long pte_t;

// TCR_EL1 bit fields
#define TCR_T0SZ(x)	(((x) & 0x3F) << 0)  // TTBR0 VA size = 64 - T0SZ
#define TCR_T1SZ(x)	(((x) & 0x3F) << 16) // TTBR1 VA size = 64 - T1SZ
#define TCR_TG0_4KB	(0UL << 14)	     // TTBR0 granule = 4KB
#define TCR_TG1_4KB	(2UL << 30)	     // TTBR1 granule = 4KB
#define TCR_SH0_INNER	(3UL << 12)	     // TTBR0 inner shareable
#define TCR_SH1_INNER	(3UL << 28)	     // TTBR1 inner shareable
#define TCR_ORGN0_WB_WA (1UL << 10)	     // TTBR0 outer write-back write-allocate
#define TCR_IRGN0_WB_WA (1UL << 8)	     // TTBR0 inner write-back write-allocate
#define TCR_ORGN1_WB_WA (1UL << 26)	     // TTBR1 outer write-back write-allocate
#define TCR_IRGN1_WB_WA (1UL << 24)	     // TTBR1 inner write-back write-allocate
#define TCR_IPS_36BIT	(1UL << 32)	     // 36-bit PA space (64GB)

// TCR value for 48-bit VA, 4KB granule
#define TCR_VALUE                                                            \
	(TCR_T0SZ(16) | TCR_T1SZ(16) | TCR_TG0_4KB | TCR_TG1_4KB |           \
	 TCR_SH0_INNER | TCR_SH1_INNER | TCR_ORGN0_WB_WA | TCR_IRGN0_WB_WA | \
	 TCR_ORGN1_WB_WA | TCR_IRGN1_WB_WA | TCR_IPS_36BIT)

// MAIR_EL1 memory attribute indices
#define MAIR_DEVICE_nGnRnE 0x00 // Device memory: non-Gathering, non-Reordering, no Early write ack
#define MAIR_NORMAL_WB	   0xFF // Normal memory: write-back, read/write allocate

// MAIR index encoding (for AttrIndx field)
#define MAIR_IDX_DEVICE 0
#define MAIR_IDX_NORMAL 1

// MAIR value: Attr0 = device, Attr1 = normal
#define MAIR_VALUE ((MAIR_NORMAL_WB << 8) | MAIR_DEVICE_nGnRnE)

// Page table entry bits
#define PTE_VALID    (1UL << 0)	 // Entry is valid
#define PTE_TABLE    (1UL << 1)	 // Points to next-level table (vs block)
#define PTE_PAGE     (1UL << 1)	 // L3 page descriptor (same bit as PTE_TABLE)
#define PTE_AF	     (1UL << 10) // Access flag (must be 1 to avoid fault)
#define PTE_SH_INNER (3UL << 8)	 // Inner shareable
#define PTE_SH_OUTER (2UL << 8)	 // Outer shareable
#define PTE_UXN	     (1UL << 54) // Unprivileged execute never
#define PTE_PXN	     (1UL << 53) // Privileged execute never

// Access Permission bits [7:6]
#define PTE_AP_RW_EL1 (0UL << 6) // EL1 R/W, EL0 none (kernel only)
#define PTE_AP_RW_ALL (1UL << 6) // EL1 R/W, EL0 R/W (user data)
#define PTE_AP_RO_EL1 (2UL << 6) // EL1 R/O, EL0 none
#define PTE_AP_RO_ALL (3UL << 6) // EL1 R/O, EL0 R/O (user code)

// AttrIndx encoding (bits 4:2)
#define PTE_ATTR_DEVICE (MAIR_IDX_DEVICE << 2) // Use MAIR index 0
#define PTE_ATTR_NORMAL (MAIR_IDX_NORMAL << 2) // Use MAIR index 1

// Output address mask for block/table descriptors
#define PTE_ADDR_MASK 0x0000FFFFFFFFF000UL

// SCTLR_EL1 bits
#define SCTLR_M (1UL << 0)  // MMU enable
#define SCTLR_C (1UL << 2)  // Data cache enable
#define SCTLR_I (1UL << 12) // Instruction cache enable

// User memory layout
#define USER_STACK 0x0000000080000000UL

// User-space page table management
pte_t *uvm_create(void);
void uvm_free(pte_t *pagetable);
int uvm_map_page(pte_t *pagetable, unsigned long va, paddr_t pa, int write, int exec);
pte_t *uvm_copy(pte_t *src);

#endif
