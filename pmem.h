#ifndef PMEM_H
#define PMEM_H

#include "mem.h"

#define PAGE_SHIFT 12

#define PAGE_ALIGN(addr)      (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define IS_PAGE_ALIGNED(addr) (((addr) & (PAGE_SIZE - 1)) == 0)

void pmem_init(void);
paddr_t pmem_alloc(void);
void pmem_free(paddr_t pa);
unsigned long pmem_free_count(void);

#endif
