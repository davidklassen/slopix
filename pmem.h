#ifndef PMEM_H
#define PMEM_H

#include "mmu.h"

#define PAGE_SIZE  4096UL
#define PAGE_SHIFT 12

#define PAGE_ALIGN(addr)      (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define IS_PAGE_ALIGNED(addr) (((addr) & (PAGE_SIZE - 1)) == 0)

#define KERNEL_BASE  0xFFFF000000000000UL
#define PA_TO_VA(pa) ((void *)((paddr_t)(pa) + KERNEL_BASE))
#define VA_TO_PA(va) ((paddr_t)(va) - KERNEL_BASE)

void pmem_init(void);
paddr_t pmem_alloc(void);
void pmem_free(paddr_t pa);
unsigned long pmem_free_count(void);

#endif
