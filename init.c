#include "init.h"
#include "elf.h"
#include "initramfs.h"
#include "kprintf.h"
#include "mmu.h"
#include "pmem.h"
#include "proc.h"

void init(const char *program) {
	struct initramfs_entry entry;
	if (initramfs_find(program, &entry) < 0) {
		kpanic("init: program not found");
	}

	pte_t *pt = uvm_create();
	if (!pt) {
		kpanic("init: failed to create page table");
	}

	unsigned long entry_addr;
	if (elf_load(entry.data, entry.size, pt, &entry_addr) < 0) {
		kpanic("init: failed to load ELF");
	}

	paddr_t stack_pa = pmem_alloc();
	if (stack_pa == 0) {
		kpanic("init: failed to allocate stack");
	}

	unsigned long stack_va = USER_STACK - PAGE_SIZE;
	if (uvm_map_page(pt, stack_va, stack_pa, 1, 0) < 0) {
		kpanic("init: failed to map stack");
	}

	if (proc_create_user(pt, entry_addr, USER_STACK) < 0) {
		kpanic("init: failed to create process");
	}
}
