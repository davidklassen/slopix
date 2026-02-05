#include "pipe.h"
#include "errno.h"
#include "file.h"
#include "pmm.h"
#include "board.h"
#include "proc.h"
#include "cpu.h"

int pipealloc(struct file **f0, struct file **f1) {
	struct pipe *pi = 0;
	*f0 = *f1 = 0;

	if ((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0) {
		goto bad;
	}

	paddr_t pa = pmm_alloc();
	if (pa == PMM_INVALID) {
		goto bad;
	}
	pi = (struct pipe *)PA_TO_VA(pa);

	pi->readopen = 1;
	pi->writeopen = 1;
	pi->nread = 0;
	pi->nwrite = 0;

	(*f0)->type = FD_PIPE;
	(*f0)->readable = 1;
	(*f0)->writable = 0;
	(*f0)->pipe = pi;

	(*f1)->type = FD_PIPE;
	(*f1)->readable = 0;
	(*f1)->writable = 1;
	(*f1)->pipe = pi;

	return 0;

bad:
	if (pi) {
		pmm_free(VA_TO_PA(pi));
	}
	if (*f0) {
		fileclose(*f0);
	}
	if (*f1) {
		fileclose(*f1);
	}
	return -1;
}

void pipeclose(struct pipe *pi, int writable) {
	unsigned long flags = irq_save();

	if (writable) {
		pi->writeopen = 0;
		proc_wakeup(&pi->nread);
	} else {
		pi->readopen = 0;
		proc_wakeup(&pi->nwrite);
	}

	if (pi->readopen == 0 && pi->writeopen == 0) {
		irq_restore(flags);
		pmm_free(VA_TO_PA(pi));
		return;
	}

	irq_restore(flags);
}

int piperead(struct pipe *pi, char *addr, int n) {
	int i;
	unsigned long flags = irq_save();

	while (pi->nread == pi->nwrite && pi->writeopen) {
		irq_restore(flags);
		if (proc_wait(&pi->nread) < 0) {
			return -EINTR;
		}
		flags = irq_save();
	}

	for (i = 0; i < n; i++) {
		if (pi->nread == pi->nwrite) {
			break;
		}
		addr[i] = pi->data[pi->nread % PIPESIZE];
		pi->nread++;
	}

	proc_wakeup(&pi->nwrite);
	irq_restore(flags);

	return i;
}

int pipewrite(struct pipe *pi, const char *addr, int n) {
	int i = 0;
	unsigned long flags = irq_save();

	while (i < n) {
		if (pi->readopen == 0) {
			irq_restore(flags);
			return -1;
		}

		if (pi->nwrite == pi->nread + PIPESIZE) {
			proc_wakeup(&pi->nread);
			irq_restore(flags);
			if (proc_wait(&pi->nwrite) < 0) {
				return i > 0 ? i : -EINTR;
			}
			flags = irq_save();
			continue;
		}

		pi->data[pi->nwrite % PIPESIZE] = addr[i];
		pi->nwrite++;
		i++;
	}

	proc_wakeup(&pi->nread);
	irq_restore(flags);

	return i;
}
