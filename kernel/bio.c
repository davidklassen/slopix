#include "bio.h"
#include "cpu.h"
#include "proc.h"
#include "virtio.h"

static struct {
	struct buf buf[NBUF];
	struct buf head;
	int disk_busy;
} bcache;

static void disk_wait(void) {
	unsigned long flags = irq_save();
	while (bcache.disk_busy) {
		irq_restore(flags);
		proc_wait_nointr(&bcache.disk_busy);
		flags = irq_save();
	}
	bcache.disk_busy = 1;
	irq_restore(flags);
}

static void disk_done(void) {
	unsigned long flags = irq_save();
	bcache.disk_busy = 0;
	proc_wakeup(&bcache.disk_busy);
	irq_restore(flags);
}

void bio_init(void) {
	bcache.head.prev = &bcache.head;
	bcache.head.next = &bcache.head;

	for (int i = 0; i < NBUF; i++) {
		struct buf *b = &bcache.buf[i];
		b->valid = 0;
		b->dev = 0;
		b->blockno = 0;
		b->refcnt = 0;
		b->next = bcache.head.next;
		b->prev = &bcache.head;
		bcache.head.next->prev = b;
		bcache.head.next = b;
	}

	bcache.disk_busy = 0;
}

static struct buf *bget(unsigned int dev, unsigned int blockno) {
	unsigned long flags = irq_save();

	for (struct buf *b = bcache.head.next; b != &bcache.head; b = b->next) {
		if (b->dev == dev && b->blockno == blockno) {
			b->refcnt++;
			irq_restore(flags);
			return b;
		}
	}

	for (struct buf *b = bcache.head.prev; b != &bcache.head; b = b->prev) {
		if (b->refcnt == 0) {
			b->dev = dev;
			b->blockno = blockno;
			b->valid = 0;
			b->refcnt = 1;
			irq_restore(flags);
			return b;
		}
	}

	irq_restore(flags);
	return 0;
}

struct buf *bio_read(unsigned int dev, unsigned int blockno) {
	struct buf *b = bget(dev, blockno);
	if (!b) {
		return 0;
	}

	if (!b->valid) {
		disk_wait();

		unsigned long sector = blockno * SECTORS_PER_BLOCK;
		int err1 = virtio_disk_read(sector, b->data);
		int err2 = (err1 == 0) ? virtio_disk_read(sector + 1, b->data + 512) : err1;

		disk_done();

		if (err1 < 0 || err2 < 0) {
			bio_release(b);
			return 0;
		}
		b->valid = 1;
	}

	return b;
}

int bio_write(struct buf *b) {
	disk_wait();

	unsigned long sector = b->blockno * SECTORS_PER_BLOCK;
	int err1 = virtio_disk_write(sector, b->data);
	int err2 = (err1 == 0) ? virtio_disk_write(sector + 1, b->data + 512) : err1;

	disk_done();

	return (err1 < 0 || err2 < 0) ? -1 : 0;
}

void bio_release(struct buf *b) {
	unsigned long flags = irq_save();

	b->refcnt--;
	if (b->refcnt == 0) {
		b->next->prev = b->prev;
		b->prev->next = b->next;
		b->next = bcache.head.next;
		b->prev = &bcache.head;
		bcache.head.next->prev = b;
		bcache.head.next = b;
	}

	irq_restore(flags);
}
