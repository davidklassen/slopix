// Physical memory allocator
//
// Manages free physical pages using a simple free list. Each free page
// stores a pointer to the next free page, avoiding separate metadata.
//
// This implementation is NOT reentrant. It relies on:
// - Single-core execution (QEMU virt with one CPU)
// - Callers not calling pmm functions from interrupt handlers
//
// For multi-core support, add spinlock protection around freelist access.

#include "pmm.h"
#include "kprintf.h"

extern char __stack_top[];

struct run {
	struct run *next;
};

static struct run *freelist;
static unsigned long free_count;
static paddr_t reserved_start;
static paddr_t reserved_end;

static void zero_page(paddr_t pa) {
	unsigned long *p = (unsigned long *)PA_TO_VA(pa);
	for (unsigned long i = 0; i < PAGE_SIZE / sizeof(unsigned long); i++) {
		p[i] = 0;
	}
}

void pmm_reserve_region(paddr_t start, paddr_t end) {
	reserved_start = start & ~(PAGE_SIZE - 1);
	reserved_end = PAGE_ALIGN(end);
}

void pmm_init(void) {
	paddr_t start = PAGE_ALIGN(VA_TO_PA((paddr_t)__stack_top));
	paddr_t end = RAM_BASE + RAM_SIZE;

	freelist = 0;
	free_count = 0;

	for (paddr_t pa = start; pa + PAGE_SIZE <= end; pa += PAGE_SIZE) {
		if (reserved_start != 0 && pa >= reserved_start && pa < reserved_end) {
			continue;
		}
		pmm_free(pa);
	}

	kprintf("pmm: %lu pages available\n", free_count);
}

paddr_t pmm_alloc(void) {
	struct run *r = freelist;
	if (!r) {
		return PMM_INVALID;
	}
	freelist = r->next;
	free_count--;

	paddr_t pa = VA_TO_PA((paddr_t)r);
	zero_page(pa);
	return pa;
}

void pmm_free(paddr_t pa) {
	if (pa < RAM_BASE || pa >= RAM_BASE + RAM_SIZE) {
		return;
	}
	if (!IS_PAGE_ALIGNED(pa)) {
		return;
	}

	zero_page(pa);

	struct run *r = (struct run *)PA_TO_VA(pa);
	r->next = freelist;
	freelist = r;
	free_count++;
}

unsigned long pmm_free_count(void) {
	return free_count;
}
