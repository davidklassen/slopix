// board.h - Memory layout constants for QEMU virt board
#ifndef BOARD_H
#define BOARD_H

#define KERNEL_BASE 0xFFFF000000000000UL

#define RAM_BASE 0x40000000UL
#define RAM_SIZE 0x08000000UL // 128MB

#define UART0_PA 0x09000000UL
#define GICD_PA	 0x08000000UL
#define GICC_PA	 0x08010000UL

#define UART0_VA (KERNEL_BASE + UART0_PA)
#define GICD_VA	 (KERNEL_BASE + GICD_PA)
#define GICC_VA	 (KERNEL_BASE + GICC_PA)

#define PAGE_SIZE      4096UL
#define BLOCK_SIZE_2MB 0x200000UL

typedef unsigned long paddr_t;

// Address conversion
#define PA_TO_VA(pa) ((void *)((paddr_t)(pa) + KERNEL_BASE))
#define VA_TO_PA(va) ((paddr_t)(va) - KERNEL_BASE)

#endif
