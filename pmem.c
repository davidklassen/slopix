#include "pmem.h"

extern char __stack_top[];

struct run {
	struct run *next;
};

static struct run *freelist;
static unsigned long free_count;

static void zero_page(paddr_t pa) {
	unsigned long *p = (unsigned long *)PA_TO_VA(pa);
	for (unsigned long i = 0; i < PAGE_SIZE / sizeof(unsigned long); i++) {
		p[i] = 0;
	}
}

void pmem_init(void) {
	paddr_t start = PAGE_ALIGN((paddr_t)__stack_top);
	paddr_t end = RAM_BASE + RAM_SIZE;

	freelist = 0;
	free_count = 0;

	for (paddr_t pa = start; pa + PAGE_SIZE <= end; pa += PAGE_SIZE) {
		pmem_free(pa);
	}
}

paddr_t pmem_alloc(void) {
	struct run *r = freelist;
	if (!r) {
		return 0;
	}
	freelist = r->next;
	free_count--;

	paddr_t pa = VA_TO_PA((paddr_t)r);
	zero_page(pa);
	return pa;
}

void pmem_free(paddr_t pa) {
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

unsigned long pmem_free_count(void) {
	return free_count;
}
