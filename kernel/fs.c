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
	if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
		irq_restore(flags);
		ilock(ip);
		itrunc(ip);
		ip->type = 0;
		iupdate(ip);
		ip->valid = 0;
		iunlock(ip);
		flags = irq_save();
	}
	ip->ref--;
	irq_restore(flags);
}

void iupdate(struct inode *ip) {
	struct buf *bp = bread(ip->dev, IBLOCK(ip->inum, sb));
	if (!bp) {
		return;
	}
	struct dinode *dip = (struct dinode *)bp->data + ip->inum % IPB;
	dip->type = ip->type;
	dip->major = ip->major;
	dip->minor = ip->minor;
	dip->nlink = ip->nlink;
	dip->size = ip->size;
	memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
	bwrite(bp);
	brelse(bp);
}

static unsigned int balloc(unsigned int dev) {
	for (unsigned int b = 0; b < sb.size; b += BPB) {
		struct buf *bp = bread(dev, BBLOCK(b, sb));
		if (!bp) {
			return 0;
		}
		for (unsigned int bi = 0; bi < BPB && b + bi < sb.size; bi++) {
			unsigned int m = 1 << (bi % 8);
			if ((bp->data[bi / 8] & m) == 0) {
				bp->data[bi / 8] |= m;
				bwrite(bp);
				brelse(bp);
				struct buf *zbp = bread(dev, b + bi);
				if (zbp) {
					memset(zbp->data, 0, BSIZE);
					bwrite(zbp);
					brelse(zbp);
				}
				return b + bi;
			}
		}
		brelse(bp);
	}
	return 0;
}

static void bfree(unsigned int dev, unsigned int b) {
	struct buf *bp = bread(dev, BBLOCK(b, sb));
	if (!bp) {
		return;
	}
	unsigned int bi = b % BPB;
	unsigned int m = 1 << (bi % 8);
	bp->data[bi / 8] &= ~m;
	bwrite(bp);
	brelse(bp);
}

unsigned int bmap(struct inode *ip, unsigned int bn) {
	unsigned int addr;

	if (bn < NDIRECT) {
		if ((addr = ip->addrs[bn]) == 0) {
			addr = balloc(ip->dev);
			if (addr == 0) {
				return 0;
			}
			ip->addrs[bn] = addr;
		}
		return addr;
	}

	bn -= NDIRECT;
	if (bn < NINDIRECT) {
		if ((addr = ip->addrs[NDIRECT]) == 0) {
			addr = balloc(ip->dev);
			if (addr == 0) {
				return 0;
			}
			ip->addrs[NDIRECT] = addr;
		}
		struct buf *bp = bread(ip->dev, addr);
		if (!bp) {
			return 0;
		}
		unsigned int *a = (unsigned int *)bp->data;
		if ((addr = a[bn]) == 0) {
			addr = balloc(ip->dev);
			if (addr == 0) {
				brelse(bp);
				return 0;
			}
			a[bn] = addr;
			bwrite(bp);
		}
		brelse(bp);
		return addr;
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

int writei(struct inode *ip, const char *src, unsigned int off, unsigned int n) {
	if (off > ip->size || off + n < off) {
		return -1;
	}
	if (off + n > MAXFILE * BSIZE) {
		return -1;
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

		memmove(bp->data + boff, src, m);
		bwrite(bp);
		brelse(bp);

		tot += m;
		off += m;
		src += m;
	}

	if (off > ip->size) {
		ip->size = off;
	}
	iupdate(ip);
	return tot;
}

void itrunc(struct inode *ip) {
	for (int i = 0; i < NDIRECT; i++) {
		if (ip->addrs[i]) {
			bfree(ip->dev, ip->addrs[i]);
			ip->addrs[i] = 0;
		}
	}

	if (ip->addrs[NDIRECT]) {
		struct buf *bp = bread(ip->dev, ip->addrs[NDIRECT]);
		if (bp) {
			unsigned int *a = (unsigned int *)bp->data;
			for (unsigned int j = 0; j < NINDIRECT; j++) {
				if (a[j]) {
					bfree(ip->dev, a[j]);
				}
			}
			brelse(bp);
		}
		bfree(ip->dev, ip->addrs[NDIRECT]);
		ip->addrs[NDIRECT] = 0;
	}

	ip->size = 0;
	iupdate(ip);
}

void stati(struct inode *ip, struct stat *st) {
	st->dev = ip->dev;
	st->ino = ip->inum;
	st->type = ip->type;
	st->nlink = ip->nlink;
	st->size = ip->size;
}

struct inode *ialloc(unsigned int dev, unsigned short type) {
	for (unsigned int inum = 1; inum < sb.ninodes; inum++) {
		struct buf *bp = bread(dev, IBLOCK(inum, sb));
		if (!bp) {
			continue;
		}
		struct dinode *dip = (struct dinode *)bp->data + inum % IPB;
		if (dip->type == 0) {
			memset(dip, 0, sizeof(*dip));
			dip->type = type;
			bwrite(bp);
			brelse(bp);
			return iget(dev, inum);
		}
		brelse(bp);
	}
	return 0;
}

int dirlink(struct inode *dp, char *name, unsigned int inum) {
	if (dp->type != T_DIR) {
		return -1;
	}

	struct inode *ip = dirlookup(dp, name, 0);
	if (ip != 0) {
		iput(ip);
		return -1;
	}

	struct dirent de;
	unsigned int off;
	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (readi(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
			return -1;
		}
		if (de.inum == 0) {
			break;
		}
	}

	strncpy(de.name, name, DIRSIZ);
	de.inum = inum;
	if (writei(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
		return -1;
	}
	return 0;
}

int isdirempty(struct inode *dp) {
	struct dirent de;
	for (unsigned int off = 2 * sizeof(de); off < dp->size; off += sizeof(de)) {
		if (readi(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
			break;
		}
		if (de.inum != 0) {
			return 0;
		}
	}
	return 1;
}

struct inode *create(char *path, unsigned short type, unsigned short major, unsigned short minor) {
	char name[DIRSIZ];
	struct inode *dp = nameiparent(path, name);
	if (dp == 0) {
		return 0;
	}

	ilock(dp);

	struct inode *ip = dirlookup(dp, name, 0);
	if (ip != 0) {
		iunlockput(dp);
		ilock(ip);
		if (type == T_FILE && ip->type == T_FILE) {
			return ip;
		}
		iunlockput(ip);
		return 0;
	}

	ip = ialloc(dp->dev, type);
	if (ip == 0) {
		iunlockput(dp);
		return 0;
	}

	ilock(ip);
	ip->major = major;
	ip->minor = minor;
	ip->nlink = 1;
	iupdate(ip);

	if (type == T_DIR) {
		dp->nlink++;
		iupdate(dp);
		if (dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0) {
			ip->nlink = 0;
			iupdate(ip);
			iunlockput(ip);
			dp->nlink--;
			iupdate(dp);
			iunlockput(dp);
			return 0;
		}
	}

	if (dirlink(dp, name, ip->inum) < 0) {
		if (type == T_DIR) {
			dp->nlink--;
			iupdate(dp);
		}
		ip->nlink = 0;
		iupdate(ip);
		iunlockput(ip);
		iunlockput(dp);
		return 0;
	}

	iunlockput(dp);
	return ip;
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
