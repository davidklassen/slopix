#ifndef PIPE_H
#define PIPE_H

#define PIPESIZE 512

struct file;

struct pipe {
	char data[PIPESIZE];
	unsigned int nread;
	unsigned int nwrite;
	int readopen;
	int writeopen;
};

int pipealloc(struct file **f0, struct file **f1);
void pipeclose(struct pipe *pi, int writable);
int piperead(struct pipe *pi, char *addr, int n);
int pipewrite(struct pipe *pi, const char *addr, int n);

#endif
