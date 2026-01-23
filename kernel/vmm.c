// vmm.c - User page table management
//
// Kernel page tables are now static (defined in tables.S).
// This file only handles dynamic user-space page tables.

#include "vmm.h"
#include "pmm.h"

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
	int indices[3] = {L0_INDEX(va), L1_INDEX(va), L2_INDEX(va)};
	for (int level = 0; level < 3; level++) {
		pte_t *entry = &table[indices[level]];
		if (*entry & PTE_VALID) {
			table = (pte_t *)PA_TO_VA(*entry & PTE_ADDR_MASK);
		} else {
			if (!alloc) {
				return 0;
			}
			paddr_t pa = pmm_alloc();
			if (pa == PMM_INVALID) {
				return 0;
			}
			*entry = make_table_desc(pa);
			table = (pte_t *)PA_TO_VA(pa);
		}
	}

	return &table[L3_INDEX(va)];
}

// Map a single 4KB page in a user page table
int vmm_map_page(pte_t *pagetable, unsigned long va, paddr_t pa, int write, int exec) {
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
pte_t *vmm_create(void) {
	paddr_t pa = pmm_alloc();
	if (pa == PMM_INVALID) {
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
			pmm_free(pa);
		} else {
			// L3 entry points to data page
			pmm_free(pa);
		}
	}
}

// Free a user page table and all its pages
void vmm_free(pte_t *pagetable) {
	if (pagetable == 0) {
		return;
	}
	freewalk(pagetable, 0);
	pmm_free(VA_TO_PA(pagetable));
}

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
			paddr_t dst_pa = pmm_alloc();
			if (dst_pa == PMM_INVALID) {
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
			paddr_t dst_pa = pmm_alloc();
			if (dst_pa == PMM_INVALID) {
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
pte_t *vmm_copy(pte_t *src) {
	pte_t *dst = vmm_create();
	if (dst == 0) {
		return 0;
	}

	if (copywalk(dst, src, 0, 0) < 0) {
		vmm_free(dst);
		return 0;
	}

	return dst;
}

// Validate a single user page for read access
// Returns 0 if valid, -1 if invalid
static int validate_page(pte_t *pagetable, unsigned long va) {
	if (va >= USER_STACK) {
		return -1;
	}

	pte_t *pte = walk(pagetable, va, 0);
	if (pte == 0 || (*pte & PTE_VALID) == 0) {
		return -1;
	}

	// Check EL0 access (bit 6 must be set for user access)
	if ((*pte & PTE_AP_EL0_BIT) == 0) {
		return -1;
	}

	return 0;
}

// Validate user pointer range
// Returns 0 if valid, -1 if invalid
// write=1 means we need write permission, write=0 means read-only is ok
int vmm_validate(pte_t *pagetable, unsigned long va, unsigned long len, int write) {
	if (len == 0) {
		return 0;
	}

	// Check for overflow
	if (va + len < va) {
		return -1;
	}

	// Check address is in user space (below the hole)
	if (va >= USER_STACK || va + len > USER_STACK) {
		return -1;
	}

	// Check each page in the range
	unsigned long start = va & ~(PAGE_SIZE - 1);
	unsigned long end = (va + len - 1) & ~(PAGE_SIZE - 1);

	for (unsigned long page = start; page <= end; page += PAGE_SIZE) {
		pte_t *pte = walk(pagetable, page, 0);
		if (pte == 0 || (*pte & PTE_VALID) == 0) {
			return -1;
		}

		// Check EL0 access (bit 6 must be set for user access)
		if ((*pte & PTE_AP_EL0_BIT) == 0) {
			return -1;
		}

		// If write access needed, check not read-only (bit 7 must be clear)
		if (write && (*pte & PTE_AP_RO_BIT)) {
			return -1;
		}
	}

	return 0;
}

// Safely copy a null-terminated string from user space to kernel buffer
// Returns string length on success, -1 on failure
int vmm_copyinstr(pte_t *pagetable, char *dst, unsigned long srcva, unsigned long max) {
	unsigned long i = 0;
	unsigned long cur_page = srcva & ~(PAGE_SIZE - 1);

	if (validate_page(pagetable, srcva) < 0) {
		return -1;
	}

	while (i < max - 1) {
		unsigned long addr = srcva + i;

		// Check if we crossed into a new page
		unsigned long page = addr & ~(PAGE_SIZE - 1);
		if (page != cur_page) {
			if (validate_page(pagetable, addr) < 0) {
				return -1;
			}
			cur_page = page;
		}

		// Get physical address and read byte
		pte_t *pte = walk(pagetable, addr, 0);
		paddr_t pa = (*pte & PTE_ADDR_MASK) + (addr & (PAGE_SIZE - 1));
		char c = *(char *)PA_TO_VA(pa);

		dst[i] = c;
		if (c == '\0') {
			return i;
		}
		i++;
	}

	dst[i] = '\0';
	return i;
}
