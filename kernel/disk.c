#include "disk.h"
#include "file.h"
#include "bio.h"
#include "string.h"

struct bdevsw bdevsw[NBDEV];

static int disk_read(unsigned int blockno, char *buf) {
	struct buf *b = bio_read(0, blockno);
	if (!b) {
		return -1;
	}
	memmove(buf, b->data, BSIZE);
	bio_release(b);
	return 0;
}

static int disk_write(unsigned int blockno, const char *buf) {
	struct buf *b = bio_read(0, blockno);
	if (!b) {
		return -1;
	}
	memmove(b->data, buf, BSIZE);
	int ret = bio_write(b);
	bio_release(b);
	return ret;
}

void disk_init(void) {
	bdevsw[DISK].read = disk_read;
	bdevsw[DISK].write = disk_write;
}
