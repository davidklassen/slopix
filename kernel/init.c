#include "init.h"
#include "elf.h"
#include "initramfs.h"
#include "kprintf.h"
#include "vmm.h"
#include "pmm.h"
#include "proc.h"
#include "fs.h"
#include "file.h"

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

			// Set up stdin (fd 0)
			struct file *fin = filealloc();
			if (fin) {
				fin->type = FD_DEVICE;
				fin->major = CONSOLE;
				fin->readable = 1;
				fin->writable = 0;
				procs[i].ofile[0] = fin;
			}

			// Set up stdout (fd 1)
			struct file *fout = filealloc();
			if (fout) {
				fout->type = FD_DEVICE;
				fout->major = CONSOLE;
				fout->readable = 0;
				fout->writable = 1;
				procs[i].ofile[1] = fout;
			}

			// Set up stderr (fd 2)
			struct file *ferr = filealloc();
			if (ferr) {
				ferr->type = FD_DEVICE;
				ferr->major = CONSOLE;
				ferr->readable = 0;
				ferr->writable = 1;
				procs[i].ofile[2] = ferr;
			}

			break;
		}
	}
}
