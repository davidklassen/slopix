#ifndef BUF_H
#define BUF_H

#define BSIZE		  1024
#define NBUF		  64
#define SECTORS_PER_BLOCK 2

struct buf {
	int valid;
	unsigned int dev;
	unsigned int blockno;
	unsigned int refcnt;
	struct buf *prev;
	struct buf *next;
	unsigned char data[BSIZE];
};

#endif
