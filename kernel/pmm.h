#ifndef PMM_H
#define PMM_H

#include "board.h"

#define PAGE_SHIFT 12

#define PAGE_ALIGN(addr)      (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define IS_PAGE_ALIGNED(addr) (((addr) & (PAGE_SIZE - 1)) == 0)

// Returned by pmm_alloc() when no pages are available.
// Uses -1 (0xFFFF...) which is outside the 48-bit physical address space.
#define PMM_INVALID ((paddr_t) - 1)

void pmm_init(void);
void pmm_reserve_region(paddr_t start, paddr_t end);
paddr_t pmm_alloc(void);
void pmm_free(paddr_t pa);
unsigned long pmm_free_count(void);

#endif
