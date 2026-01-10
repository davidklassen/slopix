#ifndef MEMORY_H
#define MEMORY_H

// Page size
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

// Memory layout
// For T1SZ=25 (39-bit VA), TTBR1 range starts at 2^64 - 2^39 = 0xFFFFFF8000000000
// Kernel physical base is 0x40000000, so kernel virtual base is TTBR1_base + kernel_PA
#define KERNEL_VIRT_BASE 0xFFFFFF8040000000UL
#define KERNEL_PHYS_BASE 0x40000000UL

// Higher-half kernel offset for address translation
#define KERNEL_VIRT_OFFSET  (KERNEL_VIRT_BASE - KERNEL_PHYS_BASE)

// Check if address is in higher-half range (TTBR1)
#define IS_HIGHER_HALF(addr) ((unsigned long)(addr) >= 0xFFFF000000000000UL)

// Convert physical address to higher-half virtual address
#define ADDR_TO_HIGHER_HALF(addr) ((void *)((unsigned long)(addr) + KERNEL_VIRT_OFFSET))

// Physical memory - QEMU virt machine typically provides 128MB starting at 0x40000000
#define PHYS_MEMORY_START 0x40000000UL
#define PHYS_MEMORY_SIZE  (128 * 1024 * 1024)  // 128 MB
#define PHYS_MEMORY_END   (PHYS_MEMORY_START + PHYS_MEMORY_SIZE)

// Device memory region (QEMU virt platform)
#define DEVICE_REGION_START  0x08000000UL
#define DEVICE_REGION_END    0x10000000UL

// Page table layout (4KB granule, 39-bit VA)
#define PAGE_TABLE_ENTRIES   512
#define L2_BLOCK_SIZE        (2 * 1024 * 1024)  // 2MB
#define L2_BLOCK_SHIFT       21

// Convert between physical and virtual addresses
#define PHYS_TO_VIRT(addr) ((void *)((unsigned long)(addr) + KERNEL_VIRT_BASE - KERNEL_PHYS_BASE))
#define VIRT_TO_PHYS(addr) ((unsigned long)(addr) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)

// Page table entry bits
#define PTE_VALID       (1UL << 0)
#define PTE_TABLE       (1UL << 1)
#define PTE_BLOCK       (0UL << 1)
#define PTE_AF          (1UL << 10)  // Access flag
#define PTE_KERNEL      (0UL << 6)   // AP[2:1] = 00 (kernel RW, user no access)
#define PTE_RO          (2UL << 6)   // AP[2:1] = 10 (kernel RO, user no access)
#define PTE_USER        (1UL << 6)   // AP[2:1] = 01 (kernel RW, user RW)

// Memory attribute indexes (for MAIR_EL1)
#define MAIR_DEVICE_nGnRnE  0x00
#define MAIR_NORMAL_NC      0x44
#define MAIR_NORMAL         0xFF

#define MT_DEVICE_nGnRnE    0
#define MT_NORMAL_NC        1
#define MT_NORMAL           2

#endif
