#include "init.h"
#include "elf.h"
#include "initramfs.h"
#include "kprintf.h"
#include "vmm.h"
#include "pmm.h"
#include "proc.h"
#include "fs.h"

void init(const char *program) {
	struct initramfs_entry entry;
	if (initramfs_find(program, &entry) < 0) {
		kpanic("init: program not found");
	}

	pte_t *pt = vmm_create();
	if (!pt) {
		kpanic("init: failed to create page table");
	}

	unsigned long entry_addr;
	unsigned long brk;
	if (elf_load(entry.data, entry.size, pt, &entry_addr, &brk) < 0) {
		kpanic("init: failed to load ELF");
	}

	paddr_t stack_pa = pmm_alloc();
	if (stack_pa == 0) {
		kpanic("init: failed to allocate stack");
	}

	unsigned long stack_va = USER_STACK - PAGE_SIZE;
	if (vmm_map_page(pt, stack_va, stack_pa, 1, 0) < 0) {
		kpanic("init: failed to map stack");
	}

	int pid = proc_create_user(pt, entry_addr, USER_STACK, brk);
	if (pid < 0) {
		kpanic("init: failed to create process");
	}

	for (int i = 0; i < NPROC; i++) {
		if (procs[i].pid == pid) {
			struct inode *root = iget(0, ROOTINO);
			ilock(root);
			iunlock(root);
			procs[i].cwd = root;
			break;
		}
	}
}
