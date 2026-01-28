#include "init.h"
#include "elf.h"
#include "initramfs.h"
#include "kprintf.h"
#include "vmm.h"
#include "pmm.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "string.h"

void init(const char *program) {
	pte_t *pt = vmm_create();
	if (!pt) {
		kpanic("init: failed to create page table");
	}

	unsigned long entry_addr;
	unsigned long brk;

	if (strncmp(program, "initramfs:", 10) == 0) {
		struct initramfs_entry entry;
		if (initramfs_find(program + 10, &entry) < 0) {
			kpanic("init: program not found in initramfs");
		}
		if (elf_load(entry.data, entry.size, pt, &entry_addr, &brk) < 0) {
			vmm_free(pt);
			kpanic("init: failed to load ELF from initramfs");
		}
	} else if (program[0] == '/') {
		char path[128];
		strncpy(path, program, 127);
		path[127] = '\0';
		struct inode *ip = fs_namei(path);
		if (ip == 0) {
			vmm_free(pt);
			kpanic("init: program not found on disk");
		}
		fs_ilock(ip);
		if (ip->type != T_FILE) {
			fs_iunlockput(ip);
			vmm_free(pt);
			kpanic("init: not a file");
		}
		if (elf_load_from_inode(ip, pt, &entry_addr, &brk) < 0) {
			fs_iunlockput(ip);
			vmm_free(pt);
			kpanic("init: failed to load ELF from disk");
		}
		fs_iunlockput(ip);
	} else {
		vmm_free(pt);
		kpanic("init: invalid program path");
	}

	for (int i = 0; i < USER_STACK_PAGES; i++) {
		paddr_t stack_pa = pmm_alloc();
		if (stack_pa == PMM_INVALID) {
			kpanic("init: failed to allocate stack");
		}
		unsigned long stack_va = USER_STACK - (i + 1) * PAGE_SIZE;
		if (vmm_map_page(pt, stack_va, stack_pa, 1, 0) < 0) {
			kpanic("init: failed to map stack");
		}
	}

	int pid = proc_create_user(pt, entry_addr, USER_STACK, brk);
	if (pid < 0) {
		kpanic("init: failed to create process");
	}

	for (int i = 0; i < NPROC; i++) {
		if (procs[i].pid == pid) {
			struct inode *root = fs_iget(0, ROOTINO);
			fs_ilock(root);
			fs_iunlock(root);
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
