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

	struct run **tail = &freelist;
	for (paddr_t pa = start; pa + PAGE_SIZE <= end; pa += PAGE_SIZE) {
		if (reserved_start != 0 && pa >= reserved_start && pa < reserved_end)
			continue;
		zero_page(pa);
		struct run *r = (struct run *)PA_TO_VA(pa);
		r->next = 0;
		*tail = r;
		tail = &r->next;
		free_count++;
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

paddr_t pmm_alloc_contiguous(int n) {
	if (n <= 0)
		return PMM_INVALID;
	if (n == 1)
		return pmm_alloc();

	if (!freelist)
		return PMM_INVALID;

	struct run **run_start = &freelist;
	paddr_t base = VA_TO_PA((paddr_t)freelist);
	int count = 1;

	for (struct run **pp = &freelist; *pp; pp = &(*pp)->next) {
		if (count >= n)
			break;
		struct run *next = (*pp)->next;
		if (!next)
			break;
		if (VA_TO_PA((paddr_t)next) ==
		    VA_TO_PA((paddr_t)*pp) + PAGE_SIZE) {
			count++;
		} else {
			run_start = &(*pp)->next;
			base = VA_TO_PA((paddr_t)next);
			count = 1;
		}
	}

	if (count < n)
		return PMM_INVALID;

	struct run *end = *run_start;
	for (int i = 0; i < n; i++)
		end = end->next;
	*run_start = end;
	free_count -= n;

	for (int i = 0; i < n; i++)
		zero_page(base + i * PAGE_SIZE);
	return base;
}

void pmm_free(paddr_t pa) {
	if (pa < RAM_BASE || pa >= RAM_BASE + RAM_SIZE)
		return;
	if (!IS_PAGE_ALIGNED(pa))
		return;

	struct run *r = (struct run *)PA_TO_VA(pa);
	struct run **pp = &freelist;
	while (*pp && (paddr_t)*pp < (paddr_t)r)
		pp = &(*pp)->next;
	if (*pp == r)
		return;

	zero_page(pa);
	r->next = *pp;
	*pp = r;
	free_count++;
}

void pmm_free_contiguous(paddr_t pa, int n) {
	for (int i = 0; i < n; i++) {
		pmm_free(pa + i * PAGE_SIZE);
	}
}

unsigned long pmm_free_count(void) {
	return free_count;
}
