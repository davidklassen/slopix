#include "syscall.h"
#include "errno.h"
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
#include "version.h"
#include "rtc.h"

static long sys_write(int fd, const char *buf, unsigned long len) {
	if (fd < 0 || fd >= NOFILE) {
		return -EBADF;
	}

	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -EBADF;
	}

	if (vmm_validate(current->pagetable, (unsigned long)buf, len, 0) < 0) {
		return -EFAULT;
	}

	return filewrite(f, buf, len);
}

static long sys_exit(int status) {
	proc_cleanup(status);
	return 0;
}

static long sys_read(int fd, char *buf, unsigned long len) {
	if (fd < 0 || fd >= NOFILE) {
		return -EBADF;
	}

	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -EBADF;
	}

	if (vmm_validate(current->pagetable, (unsigned long)buf, len, 1) < 0) {
		return -EFAULT;
	}

	return fileread(f, buf, len);
}

static long sys_sleep(unsigned long ms) {
	unsigned long ticks = ms / 10;
	if (ticks == 0 && ms > 0) {
		ticks = 1;
	}
	if (proc_sleep(ticks) < 0) {
		return -EINTR;
	}
	return 0;
}

static long sys_getpid(void) {
	return current->pid;
}

static long sys_exec(const char *cmdline) {
	// Safely copy command line from user space
	char kcmd[1024];
	if (vmm_copyinstr(current->pagetable, kcmd, (unsigned long)cmdline, 1024) < 0) {
		return -EFAULT;
	}

	// Parse into argv (max 64 args)
	char *argv[64];
	int argc = 0;
	char *p = kcmd;

	while (*p && argc < 64) {
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
		return -EINVAL;
	}

	unsigned long entry_addr = 0;
	unsigned long brk = 0;
	pte_t *new_pt = 0;

	// Check for explicit initramfs: prefix
	if (strncmp(argv[0], "initramfs:", 10) == 0) {
		struct initramfs_entry entry;
		if (initramfs_find(argv[0] + 10, &entry) < 0) {
			return -ENOENT;
		}
		new_pt = vmm_create();
		if (!new_pt) {
			return -ENOMEM;
		}
		if (elf_load(entry.data, entry.size, new_pt, &entry_addr, &brk) < 0) {
			vmm_free(new_pt);
			return -ENOENT;
		}
	} else {
		// All other paths: look on disk
		struct inode *ip = fs_namei(argv[0]);
		if (ip == 0) {
			return -ENOENT;
		}
		fs_ilock(ip);
		if (ip->type != T_FILE) {
			fs_iunlockput(ip);
			return -EACCES;
		}
		new_pt = vmm_create();
		if (!new_pt) {
			fs_iunlockput(ip);
			return -ENOMEM;
		}
		if (elf_load_from_inode(ip, new_pt, &entry_addr, &brk) < 0) {
			fs_iunlockput(ip);
			vmm_free(new_pt);
			return -ENOENT;
		}
		fs_iunlockput(ip);
	}

	paddr_t stack_pa = 0;
	for (int i = 0; i < USER_STACK_PAGES; i++) {
		paddr_t pa = pmm_alloc();
		if (pa == PMM_INVALID) {
			vmm_free(new_pt);
			return -ENOMEM;
		}
		if (vmm_map_page(new_pt, USER_STACK - (i + 1) * PAGE_SIZE, pa, 1, 0) < 0) {
			pmm_free(pa);
			vmm_free(new_pt);
			return -ENOMEM;
		}
		if (i == 0) {
			stack_pa = pa;
		}
	}

	// Set up argc/argv on stack
	// Layout (high to low): strings, argv[argc]=NULL, argv[0..argc-1], sp
	char *kstack_top = (char *)PA_TO_VA(stack_pa) + PAGE_SIZE;
	unsigned long ustack_top = USER_STACK;

	// Copy strings to stack (from top down)
	unsigned long ustr[64];
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

	// Extract basename and store in current->name
	char *basename = argv[0];
	for (char *q = argv[0]; *q; q++) {
		if (*q == '/') {
			basename = q + 1;
		}
	}
	int k;
	for (k = 0; k < 15 && basename[k]; k++) {
		current->name[k] = basename[k];
	}
	current->name[k] = '\0';

	return argc;
}

static long sys_fork(void) {
	pte_t *child_pt = vmm_copy(current->pagetable);
	if (child_pt == 0) {
		return -ENOMEM;
	}

	struct proc *child = proc_alloc();
	if (child == 0) {
		vmm_free(child_pt);
		return -ENOMEM;
	}

	child->pagetable = child_pt;
	child->parent = current;
	child->pgid = current->pgid;

	// Copy trap frame to child's kernel stack
	char *sp = child->kstack + KSTACK_SIZE;
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
		child->cwd = fs_idup(current->cwd);
	}

	// Copy file descriptors
	for (int fd = 0; fd < NOFILE; fd++) {
		if (current->ofile[fd]) {
			child->ofile[fd] = filedup(current->ofile[fd]);
		}
	}

	child->state = RUNNABLE;

	// Parent returns child's pid
	return child->pid;
}

static long encode_wait_status(struct proc *p) {
	int exit_code = p->exit_status;
	int child_pid = p->pid;

	// Free process memory immediately to avoid exhaustion
	if (p->pagetable) {
		vmm_free(p->pagetable);
		p->pagetable = 0;
	}
	if (p->kstack) {
		pmm_free_contiguous(VA_TO_PA(p->kstack), KSTACK_PAGES);
		p->kstack = 0;
	}

	p->state = UNUSED;
	if (exit_code < 0) {
		return (child_pid << 16) | ((-exit_code) & 0x7f);
	}
	return (child_pid << 16) | (exit_code << 8);
}

static long sys_wait(void) {
	for (;;) {
		int has_children = 0;
		for (int i = 0; i < NPROC; i++) {
			struct proc *p = &procs[i];
			if (p->parent == current && p->state != UNUSED) {
				has_children = 1;
				if (p->state == ZOMBIE) {
					return encode_wait_status(p);
				}
			}
		}
		if (!has_children) {
			return -ESRCH;
		}
		if (proc_wait(current) < 0) {
			return -EINTR;
		}
	}
}

static long sys_waitpid(int pid, int options) {
	int wnohang = options & 1;
	int wuntraced = options & 2;

	for (;;) {
		struct proc *stopped = 0;
		int has_children = 0;

		for (int i = 0; i < NPROC; i++) {
			struct proc *p = &procs[i];
			if (p->state == UNUSED || p->parent != current) {
				continue;
			}
			if (pid > 0 && p->pid != pid) {
				continue;
			}
			has_children = 1;

			if (p->state == ZOMBIE) {
				return encode_wait_status(p);
			}
			if (wuntraced && p->state == STOPPED) {
				stopped = p;
			}
		}

		if (!has_children) {
			return -ESRCH;
		}

		if (stopped) {
			int sig = stopped->stop_signal ? stopped->stop_signal : SIGTSTP;
			return (stopped->pid << 16) | (sig << 8) | 0x7f;
		}

		if (wnohang) {
			return 0;
		}

		if (proc_wait(current) < 0) {
			return -EINTR;
		}
	}
}

static long sys_poll(int fd, long timeout_ms) {
	if (fd < 0 || fd >= NOFILE) {
		return -EBADF;
	}

	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -EBADF;
	}

	// For now, only console device supports polling
	if (f->type != FD_DEVICE || f->major != CONSOLE) {
		return -ENOTTY;
	}

	unsigned long ticks = timeout_ms / 10;
	if (ticks == 0 && timeout_ms > 0) {
		ticks = 1;
	}

	int r = uart_poll_timeout(ticks);
	if (r < 0) {
		return r;
	}
	return r;
}

static long sys_poweroff(void) {
	psci_system_off();
	return 0;
}

static long sys_reboot(void) {
	psci_system_reset();
	return 0;
}

static long sys_sbrk(long n) {
	unsigned long old_sz = current->sz;
	unsigned long new_sz = old_sz + n;

	if (n < 0 && new_sz > old_sz) {
		return -EINVAL;
	}

	if (n > 0) {
		unsigned long old_end = (old_sz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		unsigned long new_end = (new_sz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

		for (unsigned long va = old_end; va < new_end; va += PAGE_SIZE) {
			paddr_t pa = pmm_alloc();
			if (pa == PMM_INVALID) {
				return -ENOMEM;
			}
			if (vmm_map_page(current->pagetable, va, pa, 1, 0) < 0) {
				pmm_free(pa);
				return -ENOMEM;
			}
		}
	}

	current->sz = new_sz;
	return old_sz;
}

static long sys_open(const char *path, int flags) {
	char kpath[128];
	if (vmm_copyinstr(current->pagetable, kpath, (unsigned long)path, 128) < 0) {
		return -EFAULT;
	}

	struct inode *ip;
	if (flags & O_CREAT) {
		ip = fs_namei(kpath);
		if (ip == 0) {
			ip = fs_create(kpath, T_FILE, 0, 0);
			if (ip == 0) {
				return -ENOSPC;
			}
		} else {
			if (flags & O_EXCL) {
				fs_iput(ip);
				return -EEXIST;
			}
			fs_ilock(ip);
		}
	} else {
		ip = fs_namei(kpath);
		if (ip == 0) {
			return -ENOENT;
		}
		fs_ilock(ip);
	}

	struct file *f = filealloc();
	if (f == 0) {
		fs_iunlockput(ip);
		return -ENOMEM;
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
		fs_itrunc(ip);
	}

	fs_iunlock(ip);

	int fd = fdalloc(f);
	if (fd < 0) {
		fileclose(f);
		return -ENOMEM;
	}

	return fd;
}

static long sys_close(int fd) {
	if (fd < 0 || fd >= NOFILE) {
		return -EBADF;
	}
	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -EBADF;
	}
	current->ofile[fd] = 0;
	fileclose(f);
	return 0;
}

static long sys_fstat(int fd, struct stat *st) {
	if (fd < 0 || fd >= NOFILE) {
		return -EBADF;
	}
	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -EBADF;
	}
	if (vmm_validate(current->pagetable, (unsigned long)st, sizeof(struct stat), 1) < 0) {
		return -EFAULT;
	}
	return filestat(f, st);
}

static long sys_dup(int fd) {
	if (fd < 0 || fd >= NOFILE) {
		return -EBADF;
	}
	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -EBADF;
	}
	int newfd = fdalloc(f);
	if (newfd < 0) {
		return -ENOMEM;
	}
	filedup(f);
	return newfd;
}

static long sys_mkdir(const char *path) {
	char kpath[128];
	if (vmm_copyinstr(current->pagetable, kpath, (unsigned long)path, 128) < 0) {
		return -EFAULT;
	}

	struct inode *ip = fs_create(kpath, T_DIR, 0, 0);
	if (ip == 0) {
		return -EEXIST;
	}
	fs_iunlockput(ip);
	return 0;
}

static long sys_mknod(const char *path, int major, int minor) {
	char kpath[128];
	if (vmm_copyinstr(current->pagetable, kpath, (unsigned long)path, 128) < 0) {
		return -EFAULT;
	}

	struct inode *ip = fs_create(kpath, T_DEVICE, major, minor);
	if (ip == 0) {
		return -EEXIST;
	}
	fs_iunlockput(ip);
	return 0;
}

static long sys_link(const char *old, const char *new) {
	char kold[128], knew[128], name[DIRSIZ];

	if (vmm_copyinstr(current->pagetable, kold, (unsigned long)old, 128) < 0) {
		return -EFAULT;
	}
	if (vmm_copyinstr(current->pagetable, knew, (unsigned long)new, 128) < 0) {
		return -EFAULT;
	}

	struct inode *ip = fs_namei(kold);
	if (ip == 0) {
		return -ENOENT;
	}

	fs_ilock(ip);
	if (ip->type == T_DIR) {
		fs_iunlockput(ip);
		return -EPERM;
	}
	ip->nlink++;
	fs_iupdate(ip);
	fs_iunlock(ip);

	struct inode *dp = fs_nameiparent(knew, name);
	if (dp == 0) {
		goto bad;
	}

	fs_ilock(dp);
	if (dp->dev != ip->dev || fs_dirlink(dp, name, ip->inum) < 0) {
		fs_iunlockput(dp);
		goto bad;
	}
	fs_iunlockput(dp);
	fs_iput(ip);
	return 0;

bad:
	fs_ilock(ip);
	ip->nlink--;
	fs_iupdate(ip);
	fs_iunlockput(ip);
	return -ENOENT;
}

static long sys_unlink(const char *path) {
	char kpath[128], name[DIRSIZ];
	if (vmm_copyinstr(current->pagetable, kpath, (unsigned long)path, 128) < 0) {
		return -EFAULT;
	}

	struct inode *dp = fs_nameiparent(kpath, name);
	if (dp == 0) {
		return -ENOENT;
	}

	fs_ilock(dp);

	if (name[0] == '.' && name[1] == '\0') {
		fs_iunlockput(dp);
		return -EINVAL;
	}
	if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
		fs_iunlockput(dp);
		return -EINVAL;
	}

	unsigned int off;
	struct inode *ip = fs_dirlookup(dp, name, &off);
	if (ip == 0) {
		fs_iunlockput(dp);
		return -ENOENT;
	}

	fs_ilock(ip);

	if (ip->type == T_DIR && !fs_isdirempty(ip)) {
		fs_iunlockput(ip);
		fs_iunlockput(dp);
		return -ENOTEMPTY;
	}

	struct dirent de;
	memset(&de, 0, sizeof(de));
	if (fs_writei(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
		fs_iunlockput(ip);
		fs_iunlockput(dp);
		return -EIO;
	}

	if (ip->type == T_DIR) {
		dp->nlink--;
		fs_iupdate(dp);
	}
	fs_iunlockput(dp);

	ip->nlink--;
	fs_iupdate(ip);
	fs_iunlockput(ip);

	return 0;
}

static long sys_chdir(const char *path) {
	char kpath[128];
	if (vmm_copyinstr(current->pagetable, kpath, (unsigned long)path, 128) < 0) {
		return -EFAULT;
	}

	struct inode *ip = fs_namei(kpath);
	if (ip == 0) {
		return -ENOENT;
	}

	fs_ilock(ip);
	if (ip->type != T_DIR) {
		fs_iunlockput(ip);
		return -ENOTDIR;
	}
	fs_iunlock(ip);

	fs_iput(current->cwd);
	current->cwd = ip;
	return 0;
}

static long sys_pipe(int *fdarray) {
	if (vmm_validate(current->pagetable, (unsigned long)fdarray, 8, 1) < 0) {
		return -EFAULT;
	}

	struct file *rf, *wf;
	if (pipealloc(&rf, &wf) < 0) {
		return -ENOMEM;
	}

	int fd0 = fdalloc(rf);
	if (fd0 < 0) {
		fileclose(rf);
		fileclose(wf);
		return -ENOMEM;
	}

	int fd1 = fdalloc(wf);
	if (fd1 < 0) {
		current->ofile[fd0] = 0;
		fileclose(rf);
		fileclose(wf);
		return -ENOMEM;
	}

	fdarray[0] = fd0;
	fdarray[1] = fd1;
	return 0;
}

static long sys_stat(const char *path, struct stat *st) {
	char kpath[128];
	if (vmm_copyinstr(current->pagetable, kpath, (unsigned long)path, 128) < 0) {
		return -EFAULT;
	}
	if (vmm_validate(current->pagetable, (unsigned long)st, sizeof(struct stat), 1) < 0) {
		return -EFAULT;
	}

	struct inode *ip = fs_namei(kpath);
	if (ip == 0) {
		return -ENOENT;
	}

	fs_ilock(ip);
	fs_stati(ip, st);
	fs_iunlockput(ip);
	return 0;
}

static long sys_getcwd(char *buf, unsigned long size) {
	if (size < 2) {
		return -EINVAL;
	}
	if (vmm_validate(current->pagetable, (unsigned long)buf, size, 1) < 0) {
		return -EFAULT;
	}

	char names[16][DIRSIZ];
	int depth = 0;

	struct inode *ip = fs_idup(current->cwd);

	while (ip->inum != ROOTINO && depth < 16) {
		fs_ilock(ip);
		struct inode *parent = fs_dirlookup(ip, "..", 0);
		if (parent == 0) {
			fs_iunlockput(ip);
			return -EIO;
		}
		fs_iunlock(ip);

		fs_ilock(parent);
		struct dirent de;
		int found = 0;
		for (unsigned int off = 0; off < parent->size; off += sizeof(de)) {
			if (fs_readi(parent, (char *)&de, off, sizeof(de)) != sizeof(de)) {
				break;
			}
			if (de.inum == ip->inum && strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0) {
				strncpy(names[depth], de.name, DIRSIZ);
				found = 1;
				break;
			}
		}
		fs_iunlock(parent);
		fs_iput(ip);
		if (!found) {
			fs_iput(parent);
			return -EIO;
		}
		ip = parent;
		depth++;
	}
	fs_iput(ip);

	unsigned long pos = 0;
	if (depth == 0) {
		buf[pos++] = '/';
	} else {
		for (int i = depth - 1; i >= 0; i--) {
			buf[pos++] = '/';
			for (int j = 0; j < DIRSIZ && names[i][j]; j++) {
				if (pos + 1 >= size) {
					return -EINVAL;
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
		return -EBADF;
	}
	struct file *f = current->ofile[fd];
	if (f == 0 || (f->type != FD_INODE && f->type != FD_BDEVICE)) {
		return -EBADF;
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
		fs_ilock(f->ip);
		newoff = (long)f->ip->size + offset;
		fs_iunlock(f->ip);
		break;
	default:
		return -EINVAL;
	}

	if (newoff < 0) {
		return -EINVAL;
	}
	f->off = (unsigned int)newoff;
	return newoff;
}

// mmap flags (minimal subset)
#define MAP_FAILED    ((void *)-1)
#define PROT_READ     0x1
#define PROT_WRITE    0x2
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FIXED     0x10

static long sys_mmap(unsigned long addr, unsigned long len, int prot, int flags) {
	if (len == 0) {
		return -EINVAL;
	}

	// Only support anonymous private mappings for now
	if (!(flags & MAP_ANONYMOUS) || !(flags & MAP_PRIVATE)) {
		return -EINVAL;
	}

	// Round len up to page size
	len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	// Find a free virtual address range if not MAP_FIXED
	unsigned long va;
	if (flags & MAP_FIXED) {
		if (addr & (PAGE_SIZE - 1)) {
			return -EINVAL;
		}
		va = addr;
	} else {
		// Start after current program break, page-aligned
		va = (current->sz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	}

	// Check bounds
	if (va + len > USER_STACK - PAGE_SIZE || va + len < va) {
		return -ENOMEM;
	}

	int write = (prot & PROT_WRITE) ? 1 : 0;

	// Allocate and map pages
	for (unsigned long page_va = va; page_va < va + len; page_va += PAGE_SIZE) {
		paddr_t pa = pmm_alloc();
		if (pa == PMM_INVALID) {
			// Unmap what we've mapped so far
			for (unsigned long unmap_va = va; unmap_va < page_va; unmap_va += PAGE_SIZE) {
				paddr_t unmap_pa;
				if (vmm_unmap_page(current->pagetable, unmap_va, &unmap_pa) == 0) {
					pmm_free(unmap_pa);
				}
			}
			return -ENOMEM;
		}

		// Zero the page
		char *page = (char *)PA_TO_VA(pa);
		for (unsigned long i = 0; i < PAGE_SIZE; i++) {
			page[i] = 0;
		}

		if (vmm_map_page(current->pagetable, page_va, pa, write, 0) < 0) {
			pmm_free(pa);
			// Unmap what we've mapped so far
			for (unsigned long unmap_va = va; unmap_va < page_va; unmap_va += PAGE_SIZE) {
				paddr_t unmap_pa;
				if (vmm_unmap_page(current->pagetable, unmap_va, &unmap_pa) == 0) {
					pmm_free(unmap_pa);
				}
			}
			return -ENOMEM;
		}
	}

	// Update process size if we extended past current break
	if (va + len > current->sz) {
		current->sz = va + len;
	}

	return (long)va;
}

static long sys_munmap(unsigned long addr, unsigned long len) {
	if (addr & (PAGE_SIZE - 1)) {
		return -EINVAL;
	}
	if (len == 0) {
		return -EINVAL;
	}

	len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	for (unsigned long va = addr; va < addr + len; va += PAGE_SIZE) {
		paddr_t pa;
		if (vmm_unmap_page(current->pagetable, va, &pa) == 0) {
			pmm_free(pa);
		}
	}

	return 0;
}

static long sys_kill(int pid, int sig) {
	if (pid > 0) {
		return proc_signal(pid, sig);
	}
	if (pid < -1) {
		return proc_signal_pgrp(-pid, sig);
	}
	return -EINVAL;
}

static long sys_getprocs(struct procinfo *buf, int max) {
	if (max <= 0) {
		return -EINVAL;
	}
	if (vmm_validate(current->pagetable, (unsigned long)buf, max * sizeof(struct procinfo), 1) < 0) {
		return -EFAULT;
	}

	int count = 0;
	for (int i = 0; i < NPROC && count < max; i++) {
		struct proc *p = &procs[i];
		if (p->state == UNUSED) {
			continue;
		}

		buf[count].pid = p->pid;
		buf[count].ppid = p->parent ? p->parent->pid : 0;
		buf[count].state = p->state;
		int j;
		for (j = 0; j < 15 && p->name[j]; j++) {
			buf[count].name[j] = p->name[j];
		}
		buf[count].name[j] = '\0';
		count++;
	}
	return count;
}

static long sys_getppid(void) {
	return current->parent ? current->parent->pid : 0;
}

static long sys_setpgid(int pid, int pgid) {
	return proc_setpgid(pid, pgid);
}

static long sys_getpgid(int pid) {
	return proc_getpgid(pid);
}

static long sys_tcsetpgrp(int fd, int pgid) {
	(void)fd;
	if (pgid < 0) {
		return -EINVAL;
	}
	extern void console_set_fg_pgid(int pgid);
	console_set_fg_pgid(pgid);
	return 0;
}

static long sys_tcgetpgrp(int fd) {
	(void)fd;
	extern int console_get_fg_pgid(void);
	return console_get_fg_pgid();
}

static long sys_tcsetraw(int fd, int raw) {
	if (fd < 0 || fd > 2) {
		return -EBADF;
	}
	extern void console_set_raw(int raw);
	console_set_raw(raw);
	return 0;
}

static long sys_tcgetraw(int fd) {
	(void)fd;
	extern int console_get_raw(void);
	return console_get_raw();
}

static long sys_ftruncate(int fd, long length) {
	if (fd < 0 || fd >= NOFILE) {
		return -EBADF;
	}
	struct file *f = current->ofile[fd];
	if (f == 0) {
		return -EBADF;
	}
	if (f->type != FD_INODE || f->ip == 0) {
		return -EINVAL;
	}
	if (!f->writable) {
		return -EACCES;
	}
	if (length < 0) {
		return -EINVAL;
	}

	fs_ilock(f->ip);
	int ret = fs_itrunc_to(f->ip, (unsigned int)length);
	fs_iunlock(f->ip);
	return ret;
}

struct linux_dirent {
	unsigned long d_ino;
	unsigned long d_off;
	unsigned short d_reclen;
	char d_name[];
};

static long sys_getdents(int fd, char *buf, unsigned int count) {
	if (fd < 0 || fd >= NOFILE) {
		return -EBADF;
	}
	struct file *f = current->ofile[fd];
	if (f == 0 || f->type != FD_INODE) {
		return -EBADF;
	}
	if (vmm_validate(current->pagetable, (unsigned long)buf, count, 1) < 0) {
		return -EFAULT;
	}

	fs_ilock(f->ip);
	if (f->ip->type != T_DIR) {
		fs_iunlock(f->ip);
		return -ENOTDIR;
	}

	unsigned int bpos = 0;
	struct dirent de;
	while (f->off < f->ip->size && bpos < count) {
		if (fs_readi(f->ip, (char *)&de, f->off, sizeof(de)) != sizeof(de)) {
			break;
		}
		f->off += sizeof(de);

		if (de.inum == 0) {
			continue;
		}

		int namelen = 0;
		while (namelen < DIRSIZ && de.name[namelen]) {
			namelen++;
		}
		unsigned short reclen =
		    (sizeof(unsigned long) * 2 + sizeof(unsigned short) + namelen + 1 + 7) & ~7;

		if (bpos + reclen > count) {
			f->off -= sizeof(de);
			break;
		}

		struct linux_dirent *ld = (struct linux_dirent *)(buf + bpos);
		ld->d_ino = de.inum;
		ld->d_off = f->off;
		ld->d_reclen = reclen;
		for (int i = 0; i < namelen; i++) {
			ld->d_name[i] = de.name[i];
		}
		ld->d_name[namelen] = '\0';
		bpos += reclen;
	}

	fs_iunlock(f->ip);
	return bpos;
}

static struct sleeplock rename_lock = SLEEPLOCK_INIT("rename");

static long sys_rename(const char *oldpath, const char *newpath) {
	char kold[128], knew[128], oldname[DIRSIZ], newname[DIRSIZ];
	if (vmm_copyinstr(current->pagetable, kold, (unsigned long)oldpath, 128) <
	    0) {
		return -EFAULT;
	}
	if (vmm_copyinstr(current->pagetable, knew, (unsigned long)newpath, 128) <
	    0) {
		return -EFAULT;
	}

	sleep_lock(&rename_lock);

	struct inode *ip = fs_namei(kold);
	if (ip == 0) {
		sleep_unlock(&rename_lock);
		return -ENOENT;
	}

	int is_dir = 0;
	fs_ilock(ip);
	is_dir = (ip->type == T_DIR);
	ip->nlink++;
	fs_iupdate(ip);
	fs_iunlock(ip);

	struct inode *new_dp = fs_nameiparent(knew, newname);
	if (new_dp == 0) {
		goto fail;
	}

	// Cycle detection for directories: walk from new_dp to root,
	// ensure we don't hit ip (would create unreachable cycle)
	if (is_dir) {
		struct inode *check = fs_idup(new_dp);
		while (check->inum != ROOTINO) {
			if (check->inum == ip->inum) {
				fs_iput(check);
				fs_iput(new_dp);
				goto fail;
			}
			fs_ilock(check);
			struct inode *parent = fs_dirlookup(check, "..", 0);
			fs_iunlockput(check);
			if (parent == 0) {
				fs_iput(new_dp);
				goto fail;
			}
			check = parent;
		}
		fs_iput(check);
	}

	unsigned int new_parent_inum = new_dp->inum;
	fs_ilock(new_dp);

	unsigned int off;
	struct inode *existing = fs_dirlookup(new_dp, newname, &off);
	if (existing) {
		fs_ilock(existing);
		if (existing->type == T_DIR) {
			fs_iunlockput(existing);
			fs_iunlockput(new_dp);
			goto fail;
		}
		struct dirent de;
		memset(&de, 0, sizeof(de));
		fs_writei(new_dp, (char *)&de, off, sizeof(de));
		existing->nlink--;
		fs_iupdate(existing);
		fs_iunlockput(existing);
	}

	if (new_dp->dev != ip->dev || fs_dirlink(new_dp, newname, ip->inum) < 0) {
		fs_iunlockput(new_dp);
		goto fail;
	}

	// For directories: increment new parent's nlink (new ".." reference)
	if (is_dir) {
		new_dp->nlink++;
		fs_iupdate(new_dp);
	}
	fs_iunlockput(new_dp);

	// Unlink from old location
	struct inode *old_dp = fs_nameiparent(kold, oldname);
	if (old_dp == 0) {
		fs_iput(ip);
		sleep_unlock(&rename_lock);
		return -ENOENT;
	}
	fs_ilock(old_dp);
	struct inode *check = fs_dirlookup(old_dp, oldname, &off);
	if (check == 0 || check->inum != ip->inum) {
		if (check) {
			fs_iput(check);
		}
		fs_iunlockput(old_dp);
		fs_iput(ip);
		sleep_unlock(&rename_lock);
		return -ENOENT;
	}
	fs_iput(check);
	struct dirent de;
	memset(&de, 0, sizeof(de));
	fs_writei(old_dp, (char *)&de, off, sizeof(de));

	// For directories: decrement old parent's nlink (removed ".." reference)
	if (is_dir) {
		old_dp->nlink--;
		fs_iupdate(old_dp);
	}
	fs_iunlockput(old_dp);

	// Update ".." inside moved directory to point to new parent
	if (is_dir) {
		fs_ilock(ip);
		struct dirent dotdot;
		for (unsigned int o = 0; o < ip->size; o += sizeof(dotdot)) {
			if (fs_readi(ip, (char *)&dotdot, o, sizeof(dotdot)) !=
			    sizeof(dotdot)) {
				break;
			}
			if (strcmp(dotdot.name, "..") == 0) {
				dotdot.inum = new_parent_inum;
				fs_writei(ip, (char *)&dotdot, o, sizeof(dotdot));
				break;
			}
		}
		fs_iunlock(ip);
	}

	fs_ilock(ip);
	ip->nlink--;
	fs_iupdate(ip);
	fs_iunlockput(ip);
	sleep_unlock(&rename_lock);
	return 0;

fail:
	fs_ilock(ip);
	ip->nlink--;
	fs_iupdate(ip);
	fs_iunlockput(ip);
	sleep_unlock(&rename_lock);
	return -EINVAL;
}

static long sys_time(void) {
	return rtc_read();
}

static long sys_uname(struct utsname *buf) {
	if (vmm_validate(current->pagetable, (unsigned long)buf, sizeof(struct utsname), 1) < 0) {
		return -EFAULT;
	}
	memset(buf, 0, sizeof(struct utsname));
	strncpy(buf->sysname, "Slopix", 32);
	strncpy(buf->nodename, "slopix", 32);
	strncpy(buf->release, SLOPIX_VERSION, 32);
	strncpy(buf->version, SLOPIX_BUILD_DATE, 32);
	strncpy(buf->machine, "aarch64", 32);
	return 0;
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
	case SYS_mmap:
		ret = sys_mmap(tf->regs[0], tf->regs[1], (int)tf->regs[2], (int)tf->regs[3]);
		break;
	case SYS_munmap:
		ret = sys_munmap(tf->regs[0], tf->regs[1]);
		break;
	case SYS_kill:
		ret = sys_kill((int)tf->regs[0], (int)tf->regs[1]);
		break;
	case SYS_getprocs:
		ret = sys_getprocs((struct procinfo *)tf->regs[0], (int)tf->regs[1]);
		break;
	case SYS_getppid:
		ret = sys_getppid();
		break;
	case SYS_waitpid:
		ret = sys_waitpid((int)tf->regs[0], (int)tf->regs[1]);
		break;
	case SYS_setpgid:
		ret = sys_setpgid((int)tf->regs[0], (int)tf->regs[1]);
		break;
	case SYS_getpgid:
		ret = sys_getpgid((int)tf->regs[0]);
		break;
	case SYS_tcsetpgrp:
		ret = sys_tcsetpgrp((int)tf->regs[0], (int)tf->regs[1]);
		break;
	case SYS_tcgetpgrp:
		ret = sys_tcgetpgrp((int)tf->regs[0]);
		break;
	case SYS_tcsetraw:
		ret = sys_tcsetraw((int)tf->regs[0], (int)tf->regs[1]);
		break;
	case SYS_tcgetraw:
		ret = sys_tcgetraw((int)tf->regs[0]);
		break;
	case SYS_ftruncate:
		ret = sys_ftruncate((int)tf->regs[0], (long)tf->regs[1]);
		break;
	case SYS_getdents:
		ret = sys_getdents((int)tf->regs[0], (char *)tf->regs[1], (unsigned int)tf->regs[2]);
		break;
	case SYS_reboot:
		ret = sys_reboot();
		break;
	case SYS_uname:
		ret = sys_uname((struct utsname *)tf->regs[0]);
		break;
	case SYS_time:
		ret = sys_time();
		break;
	default:
		kprintf("Unknown syscall %lu\n", num);
	}

	tf->regs[0] = ret;
}
