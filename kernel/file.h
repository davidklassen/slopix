#ifndef FILE_H
#define FILE_H

#include "fs.h"

#define NFILE  100
#define NOFILE 16

#define FD_NONE	  0
#define FD_PIPE	  1
#define FD_INODE  2
#define FD_DEVICE 3

struct file {
	int type;
	int ref;
	int readable;
	int writable;
	struct inode *ip;
	unsigned int off;
};

struct file *filealloc(void);
struct file *filedup(struct file *f);
void fileclose(struct file *f);
int filestat(struct file *f, struct stat *st);
int fileread(struct file *f, char *addr, int n);
int fdalloc(struct file *f);

#endif
