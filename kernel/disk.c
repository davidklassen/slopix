#include "disk.h"
#include "file.h"
#include "bio.h"
#include "string.h"

struct bdevsw bdevsw[NBDEV];

static int disk_read(unsigned int blockno, char *buf) {
	struct buf *b = bread(0, blockno);
	if (!b) {
		return -1;
	}
	memmove(buf, b->data, BSIZE);
	brelse(b);
	return 0;
}

static int disk_write(unsigned int blockno, const char *buf) {
	struct buf *b = bread(0, blockno);
	if (!b) {
		return -1;
	}
	memmove(b->data, buf, BSIZE);
	int ret = bwrite(b);
	brelse(b);
	return ret;
}

void disk_init(void) {
	bdevsw[DISK].read = disk_read;
	bdevsw[DISK].write = disk_write;
}
