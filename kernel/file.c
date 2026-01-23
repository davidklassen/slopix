#include "file.h"
#include "pipe.h"
#include "cpu.h"
#include "proc.h"

static struct {
	struct file file[NFILE];
} ftable;

static inline unsigned long irq_save(void) {
	unsigned long daif = read_daif();
	disable_irq();
	return daif;
}

static inline void irq_restore(unsigned long daif) {
	if (!(daif & DAIF_IRQ_BIT)) {
		enable_irq();
	}
}

struct file *filealloc(void) {
	unsigned long flags = irq_save();
	for (int i = 0; i < NFILE; i++) {
		struct file *f = &ftable.file[i];
		if (f->ref == 0) {
			f->ref = 1;
			f->type = FD_NONE;
			f->readable = 0;
			f->writable = 0;
			f->ip = 0;
			f->off = 0;
			f->major = 0;
			f->pipe = 0;
			irq_restore(flags);
			return f;
		}
	}
	irq_restore(flags);
	return 0;
}

struct file *filedup(struct file *f) {
	unsigned long flags = irq_save();
	if (f->ref < 1) {
		irq_restore(flags);
		return 0;
	}
	f->ref++;
	irq_restore(flags);
	return f;
}

void fileclose(struct file *f) {
	unsigned long flags = irq_save();
	if (f->ref < 1) {
		irq_restore(flags);
		return;
	}
	f->ref--;
	if (f->ref > 0) {
		irq_restore(flags);
		return;
	}

	struct file ff = *f;
	f->type = FD_NONE;
	f->ip = 0;
	f->pipe = 0;
	irq_restore(flags);

	if (ff.type == FD_PIPE) {
		pipeclose(ff.pipe, ff.writable);
	} else if ((ff.type == FD_INODE || ff.type == FD_DEVICE) && ff.ip) {
		iput(ff.ip);
	}
}

int filestat(struct file *f, struct stat *st) {
	if ((f->type == FD_INODE || f->type == FD_DEVICE) && f->ip) {
		ilock(f->ip);
		stati(f->ip, st);
		iunlock(f->ip);
		return 0;
	}
	return -1;
}

int fileread(struct file *f, char *addr, int n) {
	if (!f->readable) {
		return -1;
	}

	if (f->type == FD_PIPE) {
		return piperead(f->pipe, addr, n);
	}

	if (f->type == FD_DEVICE) {
		if (f->major < 0 || f->major >= NDEV || !devsw[f->major].read) {
			return -1;
		}
		return devsw[f->major].read(addr, n);
	}

	if (f->type == FD_INODE) {
		ilock(f->ip);
		int r = readi(f->ip, addr, f->off, n);
		if (r > 0) {
			f->off += r;
		}
		iunlock(f->ip);
		return r;
	}

	return -1;
}

int filewrite(struct file *f, const char *addr, int n) {
	if (!f->writable) {
		return -1;
	}

	if (f->type == FD_PIPE) {
		return pipewrite(f->pipe, addr, n);
	}

	if (f->type == FD_DEVICE) {
		if (f->major < 0 || f->major >= NDEV || !devsw[f->major].write) {
			return -1;
		}
		return devsw[f->major].write(addr, n);
	}

	if (f->type == FD_INODE) {
		ilock(f->ip);
		int r = writei(f->ip, addr, f->off, n);
		if (r > 0) {
			f->off += r;
		}
		iunlock(f->ip);
		return r;
	}

	return -1;
}

int fdalloc(struct file *f) {
	for (int fd = 0; fd < NOFILE; fd++) {
		if (current->ofile[fd] == 0) {
			current->ofile[fd] = f;
			return fd;
		}
	}
	return -1;
}
