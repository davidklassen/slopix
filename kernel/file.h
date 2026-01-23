#ifndef FILE_H
#define FILE_H

#include "fs.h"

struct pipe;

#define NFILE  100
#define NOFILE 16

#define FD_NONE	   0
#define FD_PIPE	   1
#define FD_INODE   2
#define FD_DEVICE  3
#define FD_BDEVICE 4

#define NDEV	10
#define CONSOLE 1
#define NULLDEV 2

struct devsw {
	int (*read)(char *, int);
	int (*write)(const char *, int);
};

extern struct devsw devsw[];

#define NBDEV 4
#define DISK  1

struct bdevsw {
	int (*read)(unsigned int blockno, char *buf);
	int (*write)(unsigned int blockno, const char *buf);
};

extern struct bdevsw bdevsw[];

struct file {
	int type;
	int ref;
	int readable;
	int writable;
	int append;
	struct inode *ip;
	unsigned int off;
	short major;
	struct pipe *pipe;
};

struct file *filealloc(void);
struct file *filedup(struct file *f);
void fileclose(struct file *f);
int filestat(struct file *f, struct stat *st);
int fileread(struct file *f, char *addr, int n);
int filewrite(struct file *f, const char *addr, int n);
int fdalloc(struct file *f);

#endif
