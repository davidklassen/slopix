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

static int is_page_free(paddr_t pa) {
	struct run *r = freelist;
	while (r) {
		if (VA_TO_PA((paddr_t)r) == pa) {
			return 1;
		}
		r = r->next;
	}
	return 0;
}

static void remove_page(paddr_t pa) {
	struct run **pp = &freelist;
	while (*pp) {
		if (VA_TO_PA((paddr_t)*pp) == pa) {
			*pp = (*pp)->next;
			free_count--;
			return;
		}
		pp = &(*pp)->next;
	}
}

paddr_t pmm_alloc_contiguous(int n) {
	if (n <= 0) {
		return PMM_INVALID;
	}
	if (n == 1) {
		return pmm_alloc();
	}

	struct run *r = freelist;
	while (r) {
		paddr_t base = VA_TO_PA((paddr_t)r);
		int found = 1;
		for (int i = 1; i < n; i++) {
			if (!is_page_free(base + i * PAGE_SIZE)) {
				found = 0;
				break;
			}
		}
		if (found) {
			for (int i = 0; i < n; i++) {
				paddr_t pa = base + i * PAGE_SIZE;
				remove_page(pa);
				zero_page(pa);
			}
			return base;
		}
		r = r->next;
	}
	return PMM_INVALID;
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

void pmm_free_contiguous(paddr_t pa, int n) {
	for (int i = 0; i < n; i++) {
		pmm_free(pa + i * PAGE_SIZE);
	}
}

unsigned long pmm_free_count(void) {
	return free_count;
}
