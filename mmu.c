#include "mmu.h"
#include "arch.h"
#include "uart.h"
#include "gic.h"
#include "pmem.h"

// Page tables placed in .pagetables section (4KB aligned)
#define SECTION_PAGETABLES __attribute__((section(".pagetables"), aligned(4096)))

// Identity map tables (TTBR0) - VA = PA during MMU enable transition
static pte_t identity_l0[PTE_PER_TABLE] SECTION_PAGETABLES;
static pte_t identity_l1[PTE_PER_TABLE] SECTION_PAGETABLES;
static pte_t identity_l2_dev[PTE_PER_TABLE] SECTION_PAGETABLES;
static pte_t identity_l2_ram[PTE_PER_TABLE] SECTION_PAGETABLES;

// Kernel map tables (TTBR1) - high address mapping
static pte_t kernel_l0[PTE_PER_TABLE] SECTION_PAGETABLES;
static pte_t kernel_l1[PTE_PER_TABLE] SECTION_PAGETABLES;
static pte_t kernel_l2_dev[PTE_PER_TABLE] SECTION_PAGETABLES;
static pte_t kernel_l2_ram[PTE_PER_TABLE] SECTION_PAGETABLES;

// Create a table descriptor pointing to next-level table
static pte_t make_table_desc(paddr_t next_table_pa) {
	return (next_table_pa & PTE_ADDR_MASK) | PTE_TABLE | PTE_VALID;
}

// Create a 2MB block descriptor for device memory (non-executable, non-cached)
static pte_t make_block_desc_device(paddr_t pa) {
	return (pa & PTE_ADDR_MASK) | PTE_AF | PTE_SH_OUTER |
	       PTE_ATTR_DEVICE | PTE_UXN | PTE_PXN | PTE_VALID;
}

// Create a 2MB block descriptor for normal memory
static pte_t make_block_desc_normal(paddr_t pa, int exec) {
	pte_t entry = (pa & PTE_ADDR_MASK) | PTE_AF | PTE_SH_INNER |
		      PTE_ATTR_NORMAL | PTE_UXN | PTE_VALID;
	if (!exec) {
		entry |= PTE_PXN;
	}
	return entry;
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

// Build identity mapping tables (TTBR0)
// Maps physical addresses to same virtual addresses for MMU enable transition
static void build_identity_tables(void) {
	// Clear all tables
	for (int i = 0; i < PTE_PER_TABLE; i++) {
		identity_l0[i] = 0;
		identity_l1[i] = 0;
		identity_l2_dev[i] = 0;
		identity_l2_ram[i] = 0;
	}

	// L0[0] -> L1 (covers first 512GB)
	identity_l0[0] = make_table_desc((paddr_t)identity_l1);

	// L1[0] -> L2_dev for device memory (covers first 1GB: 0x0 - 0x3FFF_FFFF)
	identity_l1[0] = make_table_desc((paddr_t)identity_l2_dev);

	// L1[1] -> L2_ram for RAM (covers second 1GB: 0x4000_0000 - 0x7FFF_FFFF)
	identity_l1[1] = make_table_desc((paddr_t)identity_l2_ram);

	// Device mappings in L2_dev (2MB blocks)
	identity_l2_dev[L2_INDEX(GICD_PHYS)] =
	    make_block_desc_device(GICD_PHYS);
	identity_l2_dev[L2_INDEX(UART0_PHYS)] =
	    make_block_desc_device(UART0_PHYS);

	// RAM mappings in L2_ram (2MB blocks)
	// Map 128MB of RAM starting at 0x4000_0000
	// L2 index 0 maps 0x4000_0000, index 1 maps 0x4020_0000, etc.
	int num_ram_blocks = RAM_SIZE / BLOCK_SIZE_2MB; // 64 blocks
	for (int i = 0; i < num_ram_blocks; i++) {
		paddr_t pa = RAM_BASE + (i * BLOCK_SIZE_2MB);
		// Executable for code region
		identity_l2_ram[i] = make_block_desc_normal(pa, 1);
	}
}

// Build kernel mapping tables (TTBR1)
// Maps kernel virtual addresses (0xFFFF_0000_xxxx_xxxx) to physical
static void build_kernel_tables(void) {
	// Clear all tables
	for (int i = 0; i < PTE_PER_TABLE; i++) {
		kernel_l0[i] = 0;
		kernel_l1[i] = 0;
		kernel_l2_dev[i] = 0;
		kernel_l2_ram[i] = 0;
	}

	// L0[0] -> L1 (covers VA 0xFFFF_0000_0000_0000 - 0xFFFF_007F_FFFF_FFFF)
	kernel_l0[0] = make_table_desc((paddr_t)kernel_l1);

	// L1[0] -> L2_dev for devices (covers VA 0xFFFF_0000_0000_0000 - 0xFFFF_0000_3FFF_FFFF)
	kernel_l1[0] = make_table_desc((paddr_t)kernel_l2_dev);

	// L1[1] -> L2_ram for kernel (covers VA 0xFFFF_0000_4000_0000 - 0xFFFF_0000_7FFF_FFFF)
	kernel_l1[1] = make_table_desc((paddr_t)kernel_l2_ram);

	// Device mappings in L2_dev (2MB blocks)
	// Kernel VA 0xFFFF_0000_xxxx_xxxx maps to PA xxxx_xxxx
	kernel_l2_dev[L2_INDEX(GICD_PHYS)] =
	    make_block_desc_device(GICD_PHYS);
	kernel_l2_dev[L2_INDEX(UART0_PHYS)] =
	    make_block_desc_device(UART0_PHYS);

	// RAM mappings in L2_ram (2MB blocks)
	// Kernel VA 0xFFFF_0000_4000_0000 -> PA 0x4000_0000
	int num_ram_blocks = RAM_SIZE / BLOCK_SIZE_2MB;
	for (int i = 0; i < num_ram_blocks; i++) {
		paddr_t pa = RAM_BASE + (i * BLOCK_SIZE_2MB);
		kernel_l2_ram[i] = make_block_desc_normal(pa, 1);
	}
}

// Configure MMU system registers
static void configure_mmu_registers(void) {
	// Configure memory attribute indirection register
	write_mair_el1(MAIR_VALUE);

	// Configure translation control register
	write_tcr_el1(TCR_VALUE);

	// Set TTBR0 for identity mapping (VA = PA)
	write_ttbr0_el1((unsigned long)identity_l0);

	// Set TTBR1 for kernel high-address mapping
	write_ttbr1_el1((unsigned long)kernel_l0);

	isb();
}

void mmu_init(void) {
	build_identity_tables();
	build_kernel_tables();
	configure_mmu_registers();
	mmu_enable();
}
