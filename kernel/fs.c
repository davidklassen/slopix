#include "fs.h"
#include "bio.h"
#include "cpu.h"
#include "proc.h"
#include "kstring.h"

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

struct inode *idup(struct inode *ip) {
	unsigned long flags = irq_save();
	ip->ref++;
	irq_restore(flags);
	return ip;
}

void iunlockput(struct inode *ip) {
	iunlock(ip);
	iput(ip);
}

int readi(struct inode *ip, char *dst, unsigned int off, unsigned int n) {
	if (off > ip->size) {
		return 0;
	}
	if (off + n > ip->size) {
		n = ip->size - off;
	}

	unsigned int tot = 0;
	while (tot < n) {
		unsigned int bn = off / BSIZE;
		unsigned int addr = bmap(ip, bn);
		if (addr == 0) {
			break;
		}

		struct buf *bp = bread(ip->dev, addr);
		if (!bp) {
			break;
		}

		unsigned int boff = off % BSIZE;
		unsigned int m = BSIZE - boff;
		if (m > n - tot) {
			m = n - tot;
		}

		memmove(dst, bp->data + boff, m);
		brelse(bp);

		tot += m;
		off += m;
		dst += m;
	}

	return tot;
}

struct inode *dirlookup(struct inode *dp, char *name, unsigned int *poff) {
	if (dp->type != T_DIR) {
		return 0;
	}

	struct dirent de;
	for (unsigned int off = 0; off < dp->size; off += sizeof(de)) {
		if (readi(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
			break;
		}
		if (de.inum == 0) {
			continue;
		}
		if (strncmp(name, de.name, DIRSIZ) == 0) {
			if (poff) {
				*poff = off;
			}
			return iget(dp->dev, de.inum);
		}
	}
	return 0;
}

static char *skipelem(char *path, char *name) {
	while (*path == '/') {
		path++;
	}
	if (*path == 0) {
		return 0;
	}

	char *s = path;
	while (*path != '/' && *path != 0) {
		path++;
	}

	int len = path - s;
	if (len >= DIRSIZ) {
		memmove(name, s, DIRSIZ);
	} else {
		memmove(name, s, len);
		name[len] = 0;
	}

	while (*path == '/') {
		path++;
	}
	return path;
}

static struct inode *namex(char *path, int nameiparent, char *name) {
	struct inode *ip;

	if (*path == '/') {
		ip = iget(0, ROOTINO);
	} else {
		ip = idup(current->cwd);
	}

	while ((path = skipelem(path, name)) != 0) {
		ilock(ip);
		if (ip->type != T_DIR) {
			iunlockput(ip);
			return 0;
		}
		if (nameiparent && *path == '\0') {
			iunlock(ip);
			return ip;
		}
		struct inode *next = dirlookup(ip, name, 0);
		iunlockput(ip);
		if (next == 0) {
			return 0;
		}
		ip = next;
	}

	if (nameiparent) {
		iput(ip);
		return 0;
	}
	return ip;
}

struct inode *namei(char *path) {
	char name[DIRSIZ];
	return namex(path, 0, name);
}

struct inode *nameiparent(char *path, char *name) {
	return namex(path, 1, name);
}
