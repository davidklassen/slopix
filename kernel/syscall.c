#include "syscall.h"
#include "exception.h"
#include "proc.h"
#include "uart.h"
#include "kprintf.h"
#include "initramfs.h"
#include "elf.h"
#include "pmm.h"
#include "cpu.h"
#include "vmm.h"
#include "psci.h"
#include "fs.h"
#include "file.h"
#include "pipe.h"
#include "string.h"
#include "sync.h"

static long sys_write(int fd, const char *buf, unsigned long len) {
	if (fd < 0 || fd >= NOFILE) {
		return -1;
	}

	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -1;
	}

	if (vmm_validate(current->pagetable, (unsigned long)buf, len, 0) < 0) {
		return -1;
	}

	return filewrite(f, buf, len);
}

static long sys_exit(int status) {
	for (int fd = 0; fd < NOFILE; fd++) {
		if (current->ofile[fd]) {
			fileclose(current->ofile[fd]);
			current->ofile[fd] = 0;
		}
	}

	if (current->cwd) {
		iput(current->cwd);
		current->cwd = 0;
	}
	current->exit_status = status;
	if (current->parent) {
		current->state = ZOMBIE;
		wakeup(current->parent);
	} else {
		current->state = UNUSED;
	}
	sched();
	return 0;
}

static long sys_read(int fd, char *buf, unsigned long len) {
	if (fd < 0 || fd >= NOFILE) {
		return -1;
	}

	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -1;
	}

	if (vmm_validate(current->pagetable, (unsigned long)buf, len, 1) < 0) {
		return -1;
	}

	return fileread(f, buf, len);
}

static long sys_sleep(unsigned long ms) {
	unsigned long ticks = ms / 10;
	if (ticks == 0 && ms > 0) {
		ticks = 1;
	}
	ksleep(ticks);
	return 0;
}

static long sys_getpid(void) {
	return current->pid;
}

static long sys_exec(const char *cmdline) {
	// Safely copy command line from user space
	char kcmd[128];
	if (vmm_copyinstr(current->pagetable, kcmd, (unsigned long)cmdline, 128) < 0) {
		return -1;
	}

	// Parse into argv (max 16 args)
	char *argv[16];
	int argc = 0;
	char *p = kcmd;

	while (*p && argc < 16) {
		while (*p == ' ') {
			p++;
		}
		if (*p == '\0') {
			break;
		}
		argv[argc++] = p;
		while (*p && *p != ' ') {
			p++;
		}
		if (*p) {
			*p++ = '\0';
		}
	}

	if (argc == 0) {
		return -1;
	}

	unsigned long entry_addr = 0;
	unsigned long brk = 0;
	pte_t *new_pt = 0;

	if (argv[0][0] == '/') {
		struct inode *ip = namei(argv[0]);
		if (ip == 0) {
			return -1;
		}
		ilock(ip);
		if (ip->type != T_FILE) {
			iunlockput(ip);
			return -1;
		}
		new_pt = vmm_create();
		if (!new_pt) {
			iunlockput(ip);
			return -1;
		}
		if (elf_load_from_inode(ip, new_pt, &entry_addr, &brk) < 0) {
			iunlockput(ip);
			vmm_free(new_pt);
			return -1;
		}
		iunlockput(ip);
	} else {
		struct initramfs_entry entry;
		if (initramfs_find(argv[0], &entry) < 0) {
			return -1;
		}
		new_pt = vmm_create();
		if (!new_pt) {
			return -1;
		}
		if (elf_load(entry.data, entry.size, new_pt, &entry_addr, &brk) < 0) {
			vmm_free(new_pt);
			return -1;
		}
	}

	paddr_t stack_pa = pmm_alloc();
	if (stack_pa == 0) {
		vmm_free(new_pt);
		return -1;
	}

	if (vmm_map_page(new_pt, USER_STACK - PAGE_SIZE, stack_pa, 1, 0) < 0) {
		pmm_free(stack_pa);
		vmm_free(new_pt);
		return -1;
	}

	// Set up argc/argv on stack
	// Layout (high to low): strings, argv[argc]=NULL, argv[0..argc-1], sp
	char *kstack_top = (char *)PA_TO_VA(stack_pa) + PAGE_SIZE;
	unsigned long ustack_top = USER_STACK;

	// Copy strings to stack (from top down)
	unsigned long ustr[16];
	for (int j = argc - 1; j >= 0; j--) {
		int len = 0;
		while (argv[j][len]) {
			len++;
		}
		len++;
		kstack_top -= len;
		ustack_top -= len;
		for (int k = 0; k < len; k++) {
			kstack_top[k] = argv[j][k];
		}
		ustr[j] = ustack_top;
	}

	// Align to 8 bytes
	ustack_top &= ~7UL;
	kstack_top = (char *)PA_TO_VA(stack_pa) + PAGE_SIZE - (USER_STACK - ustack_top);

	// argv array: argv[0..argc-1], NULL
	int argv_size = (argc + 1) * 8;
	ustack_top -= argv_size;
	kstack_top -= argv_size;
	unsigned long *kargv = (unsigned long *)kstack_top;
	for (int j = 0; j < argc; j++) {
		kargv[j] = ustr[j];
	}
	kargv[argc] = 0;

	// 16-byte align sp
	unsigned long sp = ustack_top & ~0xFUL;

	// Switch to new address space
	pte_t *old_pt = current->pagetable;
	current->pagetable = new_pt;
	current->sz = brk;
	write_ttbr0_el1(VA_TO_PA(new_pt));
	tlbi_vmalle1();
	if (old_pt) {
		vmm_free(old_pt);
	}

	// Update trap frame for new program
	current->tf->elr = entry_addr;
	current->tf->sp_el0 = sp;
	current->tf->regs[1] = ustack_top;

	return argc;
}

static long sys_fork(void) {
	pte_t *child_pt = vmm_copy(current->pagetable);
	if (child_pt == 0) {
		return -1;
	}

	struct proc *child = proc_alloc();
	if (child == 0) {
		vmm_free(child_pt);
		return -1;
	}

	child->pagetable = child_pt;
	child->parent = current;

	// Copy trap frame to child's kernel stack
	char *sp = child->kstack + PAGE_SIZE;
	sp -= sizeof(struct trap_frame);
	sp = (char *)((unsigned long)sp & ~0xFUL);

	struct trap_frame *child_tf = (struct trap_frame *)sp;
	child->tf = child_tf;

	// Copy parent's trap frame
	for (int i = 0; i < 31; i++) {
		child_tf->regs[i] = current->tf->regs[i];
	}
	child_tf->sp_el0 = current->tf->sp_el0;
	child_tf->elr = current->tf->elr;
	child_tf->spsr = current->tf->spsr;

	// Child returns 0 from fork
	child_tf->regs[0] = 0;

	// Set up child context to return to userspace
	extern void usertrap_first(void);
	child->ctx.x30 = (unsigned long)usertrap_first;
	child->ctx.sp = (unsigned long)child_tf;
	child->ctx.x29 = 0;

	// Copy cwd
	if (current->cwd) {
		child->cwd = idup(current->cwd);
	}

	// Copy file descriptors
	for (int fd = 0; fd < NOFILE; fd++) {
		if (current->ofile[fd]) {
			child->ofile[fd] = filedup(current->ofile[fd]);
		}
	}

	// Parent returns child's pid
	return child->pid;
}

static long sys_wait(void) {
	for (;;) {
		int has_children = 0;
		for (int i = 0; i < NPROC; i++) {
			struct proc *p = &procs[i];
			if (p->parent == current && p->state != UNUSED) {
				has_children = 1;
				if (p->state == ZOMBIE) {
					int status = p->exit_status;
					p->state = UNUSED;
					return status;
				}
			}
		}
		if (!has_children) {
			return -1;
		}
		sleep(current);
	}
}

static long sys_poll(int fd, long timeout_ms) {
	if (fd < 0 || fd >= NOFILE) {
		return -1;
	}

	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -1;
	}

	// For now, only console device supports polling
	if (f->type != FD_DEVICE || f->major != CONSOLE) {
		return -1;
	}

	unsigned long ticks = timeout_ms / 10;
	if (ticks == 0 && timeout_ms > 0) {
		ticks = 1;
	}

	return uart_poll_timeout(ticks);
}

static long sys_poweroff(void) {
	psci_system_off();
	return 0;
}

static long sys_sbrk(long n) {
	unsigned long old_sz = current->sz;
	unsigned long new_sz = old_sz + n;

	if (n < 0 && new_sz > old_sz) {
		return -1;
	}

	if (n > 0) {
		unsigned long old_end = (old_sz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		unsigned long new_end = (new_sz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

		for (unsigned long va = old_end; va < new_end; va += PAGE_SIZE) {
			paddr_t pa = pmm_alloc();
			if (pa == 0) {
				return -1;
			}
			if (vmm_map_page(current->pagetable, va, pa, 1, 0) < 0) {
				pmm_free(pa);
				return -1;
			}
		}
	}

	current->sz = new_sz;
	return old_sz;
}

static long sys_open(const char *path, int flags) {
	char kpath[128];
	if (vmm_copyinstr(current->pagetable, kpath, (unsigned long)path, 128) < 0) {
		return -1;
	}

	struct inode *ip;
	if (flags & O_CREAT) {
		ip = create(kpath, T_FILE, 0, 0);
		if (ip == 0) {
			return -1;
		}
	} else {
		ip = namei(kpath);
		if (ip == 0) {
			return -1;
		}
		ilock(ip);
	}

	struct file *f = filealloc();
	if (f == 0) {
		iunlockput(ip);
		return -1;
	}

	if (ip->type == T_DEVICE) {
		f->type = FD_DEVICE;
		f->major = ip->major;
	} else if (ip->type == T_BDEVICE) {
		f->type = FD_BDEVICE;
		f->major = ip->major;
	} else {
		f->type = FD_INODE;
	}
	f->ip = ip;
	f->off = 0;
	f->readable = !(flags & O_WRONLY);
	f->writable = (flags & O_WRONLY) || (flags & O_RDWR);
	f->append = !!(flags & O_APPEND);

	if ((flags & O_TRUNC) && f->writable && ip->type == T_FILE) {
		itrunc(ip);
	}

	iunlock(ip);

	int fd = fdalloc(f);
	if (fd < 0) {
		fileclose(f);
		return -1;
	}

	return fd;
}

static long sys_close(int fd) {
	if (fd < 0 || fd >= NOFILE) {
		return -1;
	}
	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -1;
	}
	current->ofile[fd] = 0;
	fileclose(f);
	return 0;
}

static long sys_fstat(int fd, struct stat *st) {
	if (fd < 0 || fd >= NOFILE) {
		return -1;
	}
	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -1;
	}
	if (vmm_validate(current->pagetable, (unsigned long)st, sizeof(struct stat), 1) < 0) {
		return -1;
	}
	return filestat(f, st);
}

static long sys_dup(int fd) {
	if (fd < 0 || fd >= NOFILE) {
		return -1;
	}
	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -1;
	}
	int newfd = fdalloc(f);
	if (newfd < 0) {
		return -1;
	}
	filedup(f);
	return newfd;
}

static long sys_mkdir(const char *path) {
	char kpath[128];
	if (vmm_copyinstr(current->pagetable, kpath, (unsigned long)path, 128) < 0) {
		return -1;
	}

	struct inode *ip = create(kpath, T_DIR, 0, 0);
	if (ip == 0) {
		return -1;
	}
	iunlockput(ip);
	return 0;
}

static long sys_mknod(const char *path, int major, int minor) {
	char kpath[128];
	if (vmm_copyinstr(current->pagetable, kpath, (unsigned long)path, 128) < 0) {
		return -1;
	}

	struct inode *ip = create(kpath, T_DEVICE, major, minor);
	if (ip == 0) {
		return -1;
	}
	iunlockput(ip);
	return 0;
}

static long sys_link(const char *old, const char *new) {
	char kold[128], knew[128], name[DIRSIZ];

	if (vmm_copyinstr(current->pagetable, kold, (unsigned long)old, 128) < 0) {
		return -1;
	}
	if (vmm_copyinstr(current->pagetable, knew, (unsigned long)new, 128) < 0) {
		return -1;
	}

	struct inode *ip = namei(kold);
	if (ip == 0) {
		return -1;
	}

	ilock(ip);
	if (ip->type == T_DIR) {
		iunlockput(ip);
		return -1;
	}
	ip->nlink++;
	iupdate(ip);
	iunlock(ip);

	struct inode *dp = nameiparent(knew, name);
	if (dp == 0) {
		goto bad;
	}

	ilock(dp);
	if (dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0) {
		iunlockput(dp);
		goto bad;
	}
	iunlockput(dp);
	iput(ip);
	return 0;

bad:
	ilock(ip);
	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);
	return -1;
}

static long sys_unlink(const char *path) {
	char kpath[128], name[DIRSIZ];
	if (vmm_copyinstr(current->pagetable, kpath, (unsigned long)path, 128) < 0) {
		return -1;
	}

	struct inode *dp = nameiparent(kpath, name);
	if (dp == 0) {
		return -1;
	}

	ilock(dp);

	if (name[0] == '.' && name[1] == '\0') {
		iunlockput(dp);
		return -1;
	}
	if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
		iunlockput(dp);
		return -1;
	}

	unsigned int off;
	struct inode *ip = dirlookup(dp, name, &off);
	if (ip == 0) {
		iunlockput(dp);
		return -1;
	}

	ilock(ip);

	if (ip->type == T_DIR && !isdirempty(ip)) {
		iunlockput(ip);
		iunlockput(dp);
		return -1;
	}

	struct dirent de;
	memset(&de, 0, sizeof(de));
	if (writei(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
		iunlockput(ip);
		iunlockput(dp);
		return -1;
	}

	if (ip->type == T_DIR) {
		dp->nlink--;
		iupdate(dp);
	}
	iunlockput(dp);

	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);

	return 0;
}

static long sys_chdir(const char *path) {
	char kpath[128];
	if (vmm_copyinstr(current->pagetable, kpath, (unsigned long)path, 128) < 0) {
		return -1;
	}

	struct inode *ip = namei(kpath);
	if (ip == 0) {
		return -1;
	}

	ilock(ip);
	if (ip->type != T_DIR) {
		iunlockput(ip);
		return -1;
	}
	iunlock(ip);

	iput(current->cwd);
	current->cwd = ip;
	return 0;
}

static long sys_pipe(int *fdarray) {
	if (vmm_validate(current->pagetable, (unsigned long)fdarray, 8, 1) < 0) {
		return -1;
	}

	struct file *rf, *wf;
	if (pipealloc(&rf, &wf) < 0) {
		return -1;
	}

	int fd0 = fdalloc(rf);
	if (fd0 < 0) {
		fileclose(rf);
		fileclose(wf);
		return -1;
	}

	int fd1 = fdalloc(wf);
	if (fd1 < 0) {
		current->ofile[fd0] = 0;
		fileclose(rf);
		fileclose(wf);
		return -1;
	}

	fdarray[0] = fd0;
	fdarray[1] = fd1;
	return 0;
}

static long sys_stat(const char *path, struct stat *st) {
	char kpath[128];
	if (vmm_copyinstr(current->pagetable, kpath, (unsigned long)path, 128) < 0) {
		return -1;
	}
	if (vmm_validate(current->pagetable, (unsigned long)st, sizeof(struct stat), 1) < 0) {
		return -1;
	}

	struct inode *ip = namei(kpath);
	if (ip == 0) {
		return -1;
	}

	ilock(ip);
	stati(ip, st);
	iunlockput(ip);
	return 0;
}

static long sys_getcwd(char *buf, unsigned long size) {
	if (size < 2 || vmm_validate(current->pagetable, (unsigned long)buf, size, 1) < 0) {
		return -1;
	}

	char names[16][DIRSIZ];
	int depth = 0;

	struct inode *ip = idup(current->cwd);

	while (ip->inum != ROOTINO && depth < 16) {
		ilock(ip);
		struct inode *parent = dirlookup(ip, "..", 0);
		if (parent == 0) {
			iunlockput(ip);
			return -1;
		}
		iunlock(ip);

		ilock(parent);
		struct dirent de;
		int found = 0;
		for (unsigned int off = 0; off < parent->size; off += sizeof(de)) {
			if (readi(parent, (char *)&de, off, sizeof(de)) != sizeof(de)) {
				break;
			}
			if (de.inum == ip->inum && strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0) {
				strncpy(names[depth], de.name, DIRSIZ);
				found = 1;
				break;
			}
		}
		iunlock(parent);
		iput(ip);
		if (!found) {
			iput(parent);
			return -1;
		}
		ip = parent;
		depth++;
	}
	iput(ip);

	unsigned long pos = 0;
	if (depth == 0) {
		buf[pos++] = '/';
	} else {
		for (int i = depth - 1; i >= 0; i--) {
			buf[pos++] = '/';
			for (int j = 0; j < DIRSIZ && names[i][j]; j++) {
				if (pos + 1 >= size) {
					return -1;
				}
				buf[pos++] = names[i][j];
			}
		}
	}
	buf[pos] = '\0';
	return pos;
}

static long sys_lseek(int fd, long offset, int whence) {
	if (fd < 0 || fd >= NOFILE) {
		return -1;
	}
	struct file *f = current->ofile[fd];
	if (f == 0 || (f->type != FD_INODE && f->type != FD_BDEVICE)) {
		return -1;
	}

	long newoff;
	switch (whence) {
	case 0:
		newoff = offset;
		break;
	case 1:
		newoff = (long)f->off + offset;
		break;
	case 2:
		ilock(f->ip);
		newoff = (long)f->ip->size + offset;
		iunlock(f->ip);
		break;
	default:
		return -1;
	}

	if (newoff < 0) {
		return -1;
	}
	f->off = (unsigned int)newoff;
	return newoff;
}

static struct sleeplock rename_lock = SLEEPLOCK_INIT("rename");

static long sys_rename(const char *oldpath, const char *newpath) {
	char kold[128], knew[128], oldname[DIRSIZ], newname[DIRSIZ];
	if (vmm_copyinstr(current->pagetable, kold, (unsigned long)oldpath, 128) <
	    0) {
		return -1;
	}
	if (vmm_copyinstr(current->pagetable, knew, (unsigned long)newpath, 128) <
	    0) {
		return -1;
	}

	sleep_lock(&rename_lock);

	struct inode *ip = namei(kold);
	if (ip == 0) {
		sleep_unlock(&rename_lock);
		return -1;
	}

	int is_dir = 0;
	ilock(ip);
	is_dir = (ip->type == T_DIR);
	ip->nlink++;
	iupdate(ip);
	iunlock(ip);

	struct inode *new_dp = nameiparent(knew, newname);
	if (new_dp == 0) {
		goto fail;
	}

	// Cycle detection for directories: walk from new_dp to root,
	// ensure we don't hit ip (would create unreachable cycle)
	if (is_dir) {
		struct inode *check = idup(new_dp);
		while (check->inum != ROOTINO) {
			if (check->inum == ip->inum) {
				iput(check);
				iput(new_dp);
				goto fail;
			}
			ilock(check);
			struct inode *parent = dirlookup(check, "..", 0);
			iunlockput(check);
			if (parent == 0) {
				iput(new_dp);
				goto fail;
			}
			check = parent;
		}
		iput(check);
	}

	unsigned int new_parent_inum = new_dp->inum;
	ilock(new_dp);

	unsigned int off;
	struct inode *existing = dirlookup(new_dp, newname, &off);
	if (existing) {
		ilock(existing);
		if (existing->type == T_DIR) {
			iunlockput(existing);
			iunlockput(new_dp);
			goto fail;
		}
		struct dirent de;
		memset(&de, 0, sizeof(de));
		writei(new_dp, (char *)&de, off, sizeof(de));
		existing->nlink--;
		iupdate(existing);
		iunlockput(existing);
	}

	if (new_dp->dev != ip->dev || dirlink(new_dp, newname, ip->inum) < 0) {
		iunlockput(new_dp);
		goto fail;
	}

	// For directories: increment new parent's nlink (new ".." reference)
	if (is_dir) {
		new_dp->nlink++;
		iupdate(new_dp);
	}
	iunlockput(new_dp);

	// Unlink from old location
	struct inode *old_dp = nameiparent(kold, oldname);
	if (old_dp == 0) {
		iput(ip);
		sleep_unlock(&rename_lock);
		return -1;
	}
	ilock(old_dp);
	struct inode *check = dirlookup(old_dp, oldname, &off);
	if (check == 0 || check->inum != ip->inum) {
		if (check) {
			iput(check);
		}
		iunlockput(old_dp);
		iput(ip);
		sleep_unlock(&rename_lock);
		return -1;
	}
	iput(check);
	struct dirent de;
	memset(&de, 0, sizeof(de));
	writei(old_dp, (char *)&de, off, sizeof(de));

	// For directories: decrement old parent's nlink (removed ".." reference)
	if (is_dir) {
		old_dp->nlink--;
		iupdate(old_dp);
	}
	iunlockput(old_dp);

	// Update ".." inside moved directory to point to new parent
	if (is_dir) {
		ilock(ip);
		struct dirent dotdot;
		for (unsigned int o = 0; o < ip->size; o += sizeof(dotdot)) {
			if (readi(ip, (char *)&dotdot, o, sizeof(dotdot)) !=
			    sizeof(dotdot)) {
				break;
			}
			if (strcmp(dotdot.name, "..") == 0) {
				dotdot.inum = new_parent_inum;
				writei(ip, (char *)&dotdot, o, sizeof(dotdot));
				break;
			}
		}
		iunlock(ip);
	}

	ilock(ip);
	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);
	sleep_unlock(&rename_lock);
	return 0;

fail:
	ilock(ip);
	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);
	sleep_unlock(&rename_lock);
	return -1;
}

void syscall(struct trap_frame *tf) {
	long ret = -1;
	unsigned long num = tf->regs[8];

	switch (num) {
	case SYS_write:
		ret = sys_write(tf->regs[0], (const char *)tf->regs[1], tf->regs[2]);
		break;
	case SYS_exit:
		ret = sys_exit(tf->regs[0]);
		break;
	case SYS_read:
		ret = sys_read(tf->regs[0], (char *)tf->regs[1], tf->regs[2]);
		break;
	case SYS_sleep:
		ret = sys_sleep(tf->regs[0]);
		break;
	case SYS_getpid:
		ret = sys_getpid();
		break;
	case SYS_exec:
		ret = sys_exec((const char *)tf->regs[0]);
		break;
	case SYS_fork:
		ret = sys_fork();
		break;
	case SYS_wait:
		ret = sys_wait();
		break;
	case SYS_poll:
		ret = sys_poll(tf->regs[0], tf->regs[1]);
		break;
	case SYS_poweroff:
		ret = sys_poweroff();
		break;
	case SYS_sbrk:
		ret = sys_sbrk(tf->regs[0]);
		break;
	case SYS_open:
		ret = sys_open((const char *)tf->regs[0], (int)tf->regs[1]);
		break;
	case SYS_close:
		ret = sys_close((int)tf->regs[0]);
		break;
	case SYS_fstat:
		ret = sys_fstat((int)tf->regs[0], (struct stat *)tf->regs[1]);
		break;
	case SYS_dup:
		ret = sys_dup((int)tf->regs[0]);
		break;
	case SYS_mkdir:
		ret = sys_mkdir((const char *)tf->regs[0]);
		break;
	case SYS_mknod:
		ret = sys_mknod((const char *)tf->regs[0], (int)tf->regs[1], (int)tf->regs[2]);
		break;
	case SYS_link:
		ret = sys_link((const char *)tf->regs[0], (const char *)tf->regs[1]);
		break;
	case SYS_unlink:
		ret = sys_unlink((const char *)tf->regs[0]);
		break;
	case SYS_chdir:
		ret = sys_chdir((const char *)tf->regs[0]);
		break;
	case SYS_pipe:
		ret = sys_pipe((int *)tf->regs[0]);
		break;
	case SYS_stat:
		ret = sys_stat((const char *)tf->regs[0], (struct stat *)tf->regs[1]);
		break;
	case SYS_getcwd:
		ret = sys_getcwd((char *)tf->regs[0], tf->regs[1]);
		break;
	case SYS_lseek:
		ret = sys_lseek((int)tf->regs[0], (long)tf->regs[1], (int)tf->regs[2]);
		break;
	case SYS_rename:
		ret = sys_rename((const char *)tf->regs[0], (const char *)tf->regs[1]);
		break;
	default:
		kprintf("Unknown syscall %lu\n", num);
	}

	tf->regs[0] = ret;
}
