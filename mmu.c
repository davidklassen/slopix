// mmu.c - User page table management
//
// Kernel page tables are now static (defined in tables.S).
// This file only handles dynamic user-space page tables.

#include "mmu.h"
#include "pmem.h"

// Create a table descriptor pointing to next-level table
static pte_t make_table_desc(paddr_t next_table_pa) {
	return (next_table_pa & PTE_ADDR_MASK) | PTE_TABLE | PTE_VALID;
}

// Create an L3 page descriptor for user memory
static pte_t make_page_desc_user(paddr_t pa, int write, int exec) {
	pte_t entry = (pa & PTE_ADDR_MASK) | PTE_AF | PTE_SH_INNER |
		      PTE_ATTR_NORMAL | PTE_PAGE | PTE_VALID;
	entry |= write ? PTE_AP_RW_ALL : PTE_AP_RO_ALL;
	if (!exec) {
		entry |= PTE_UXN;
	}
	entry |= PTE_PXN; // kernel never executes user pages
	return entry;
}

// Walk page table from L0 to L3, optionally allocating intermediate tables
static pte_t *walk(pte_t *pagetable, unsigned long va, int alloc) {
	pte_t *table = pagetable;

	// Walk L0 -> L1 -> L2 -> L3
	int indices[3] = {L0_INDEX(va), L1_INDEX(va), L2_INDEX(va)};
	for (int level = 0; level < 3; level++) {
		pte_t *entry = &table[indices[level]];
		if (*entry & PTE_VALID) {
			table = (pte_t *)PA_TO_VA(*entry & PTE_ADDR_MASK);
		} else {
			if (!alloc) {
				return 0;
			}
			paddr_t pa = pmem_alloc();
			if (pa == 0) {
				return 0;
			}
			*entry = make_table_desc(pa);
			table = (pte_t *)PA_TO_VA(pa);
		}
	}

	return &table[L3_INDEX(va)];
}

// Map a single 4KB page in a user page table
int uvm_map_page(pte_t *pagetable, unsigned long va, paddr_t pa, int write, int exec) {
	pte_t *pte = walk(pagetable, va, 1);
	if (pte == 0) {
		return -1;
	}
	if (*pte & PTE_VALID) {
		return -1;
	}
	*pte = make_page_desc_user(pa, write, exec);
	return 0;
}

// Allocate an empty user page table (just L0)
pte_t *uvm_create(void) {
	paddr_t pa = pmem_alloc();
	if (pa == 0) {
		return 0;
	}
	return (pte_t *)PA_TO_VA(pa);
}

// Recursively free page table entries
static void freewalk(pte_t *pagetable, int level) {
	for (int i = 0; i < PTE_PER_TABLE; i++) {
		pte_t entry = pagetable[i];
		if ((entry & PTE_VALID) == 0) {
			continue;
		}

		paddr_t pa = entry & PTE_ADDR_MASK;

		if (level < 3) {
			// Table descriptor, recurse then free table
			pte_t *child = (pte_t *)PA_TO_VA(pa);
			freewalk(child, level + 1);
			pmem_free(pa);
		} else {
			// L3 entry points to data page
			pmem_free(pa);
		}
	}
}

// Free a user page table and all its pages
void uvm_free(pte_t *pagetable) {
	if (pagetable == 0) {
		return;
	}
	freewalk(pagetable, 0);
	pmem_free(VA_TO_PA(pagetable));
}

// Helper to copy page data
static void copy_page(paddr_t dst, paddr_t src) {
	char *d = (char *)PA_TO_VA(dst);
	char *s = (char *)PA_TO_VA(src);
	for (unsigned long i = 0; i < PAGE_SIZE; i++) {
		d[i] = s[i];
	}
}

// Recursively copy page table entries
static int copywalk(pte_t *dst, pte_t *src, int level, unsigned long va) {
	for (int i = 0; i < PTE_PER_TABLE; i++) {
		pte_t entry = src[i];
		if ((entry & PTE_VALID) == 0) {
			continue;
		}

		paddr_t src_pa = entry & PTE_ADDR_MASK;

		if (level < 3) {
			// Table descriptor: allocate new table and recurse
			paddr_t dst_pa = pmem_alloc();
			if (dst_pa == 0) {
				return -1;
			}
			dst[i] = make_table_desc(dst_pa);

			pte_t *src_child = (pte_t *)PA_TO_VA(src_pa);
			pte_t *dst_child = (pte_t *)PA_TO_VA(dst_pa);

			unsigned long child_va = va;
			if (level == 0) {
				child_va |= (unsigned long)i << 39;
			} else if (level == 1) {
				child_va |= (unsigned long)i << 30;
			} else {
				child_va |= (unsigned long)i << 21;
			}

			if (copywalk(dst_child, src_child, level + 1, child_va) < 0) {
				return -1;
			}
		} else {
			// L3 entry: allocate new page and copy data
			paddr_t dst_pa = pmem_alloc();
			if (dst_pa == 0) {
				return -1;
			}
			copy_page(dst_pa, src_pa);
			// Copy entry with same flags, but new address
			dst[i] = (entry & ~PTE_ADDR_MASK) | dst_pa;
		}
	}
	return 0;
}

// Copy a user page table and all its pages
pte_t *uvm_copy(pte_t *src) {
	pte_t *dst = uvm_create();
	if (dst == 0) {
		return 0;
	}

	if (copywalk(dst, src, 0, 0) < 0) {
		uvm_free(dst);
		return 0;
	}

	return dst;
}
