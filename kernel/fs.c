#include "fs.h"
#include "bio.h"
#include "cpu.h"
#include "proc.h"

static struct {
	struct inode inode[NINODE];
} icache;

static struct superblock sb;

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

void readsb(unsigned int dev, struct superblock *sbout) {
	struct buf *bp = bread(dev, 1);
	if (!bp) {
		return;
	}
	struct superblock *sbp = (struct superblock *)bp->data;
	sbout->magic = sbp->magic;
	sbout->size = sbp->size;
	sbout->nblocks = sbp->nblocks;
	sbout->ninodes = sbp->ninodes;
	sbout->inodestart = sbp->inodestart;
	sbout->bmapstart = sbp->bmapstart;
	brelse(bp);
}

void fsinit(unsigned int dev) {
	readsb(dev, &sb);
}

struct inode *iget(unsigned int dev, unsigned int inum) {
	unsigned long flags = irq_save();

	struct inode *empty = 0;
	for (int i = 0; i < NINODE; i++) {
		struct inode *ip = &icache.inode[i];
		if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
			ip->ref++;
			irq_restore(flags);
			return ip;
		}
		if (empty == 0 && ip->ref == 0) {
			empty = ip;
		}
	}

	if (empty == 0) {
		irq_restore(flags);
		return 0;
	}

	empty->dev = dev;
	empty->inum = inum;
	empty->ref = 1;
	empty->valid = 0;
	empty->locked = 0;
	irq_restore(flags);
	return empty;
}

void ilock(struct inode *ip) {
	if (ip == 0 || ip->ref < 1) {
		return;
	}

	unsigned long flags = irq_save();
	while (ip->locked) {
		irq_restore(flags);
		sleep(ip);
		flags = irq_save();
	}
	ip->locked = 1;
	irq_restore(flags);

	if (!ip->valid) {
		struct buf *bp = bread(ip->dev, IBLOCK(ip->inum, sb));
		if (bp) {
			struct dinode *dip = (struct dinode *)bp->data + ip->inum % IPB;
			ip->type = dip->type;
			ip->major = dip->major;
			ip->minor = dip->minor;
			ip->nlink = dip->nlink;
			ip->size = dip->size;
			for (int i = 0; i < NDIRECT + 1; i++) {
				ip->addrs[i] = dip->addrs[i];
			}
			brelse(bp);
			ip->valid = 1;
		}
	}
}

void iunlock(struct inode *ip) {
	if (ip == 0 || !ip->locked || ip->ref < 1) {
		return;
	}

	unsigned long flags = irq_save();
	ip->locked = 0;
	wakeup(ip);
	irq_restore(flags);
}

void iput(struct inode *ip) {
	unsigned long flags = irq_save();
	ip->ref--;
	irq_restore(flags);
}

unsigned int bmap(struct inode *ip, unsigned int bn) {
	if (bn < NDIRECT) {
		return ip->addrs[bn];
	}

	bn -= NDIRECT;
	if (bn < NINDIRECT) {
		unsigned int addr = ip->addrs[NDIRECT];
		if (addr == 0) {
			return 0;
		}
		struct buf *bp = bread(ip->dev, addr);
		if (!bp) {
			return 0;
		}
		unsigned int *a = (unsigned int *)bp->data;
		unsigned int result = a[bn];
		brelse(bp);
		return result;
	}

	return 0;
}
