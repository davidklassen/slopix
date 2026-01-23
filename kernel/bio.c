#include "bio.h"
#include "cpu.h"
#include "proc.h"
#include "virtio.h"

static struct {
	struct buf buf[NBUF];
	struct buf head;
	int disk_busy;
} bcache;

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

static void disk_wait(void) {
	while (bcache.disk_busy) {
		sleep(&bcache.disk_busy);
	}
	bcache.disk_busy = 1;
}

static void disk_done(void) {
	bcache.disk_busy = 0;
	wakeup(&bcache.disk_busy);
}

void binit(void) {
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

struct buf *bread(unsigned int dev, unsigned int blockno) {
	struct buf *b = bget(dev, blockno);
	if (!b) {
		return 0;
	}

	if (!b->valid) {
		disk_wait();

		unsigned long sector = blockno * SECTORS_PER_BLOCK;
		virtio_disk_read(sector, b->data);
		virtio_disk_read(sector + 1, b->data + 512);
		b->valid = 1;

		disk_done();
	}

	return b;
}

void bwrite(struct buf *b) {
	disk_wait();

	unsigned long sector = b->blockno * SECTORS_PER_BLOCK;
	virtio_disk_write(sector, b->data);
	virtio_disk_write(sector + 1, b->data + 512);

	disk_done();
}

void brelse(struct buf *b) {
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
