#include "fs.h"
#include "bio.h"
#include "cpu.h"
#include "proc.h"
#include "string.h"

static struct {
	struct inode inode[NINODE];
} icache;

static struct superblock sb;

void fs_readsb(unsigned int dev, struct superblock *sbout) {
	struct buf *bp = bio_read(dev, 1);
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
	bio_release(bp);
}

void fs_init(unsigned int dev) {
	fs_readsb(dev, &sb);
}

struct inode *fs_iget(unsigned int dev, unsigned int inum) {
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

void fs_ilock(struct inode *ip) {
	if (ip == 0 || ip->ref < 1) {
		return;
	}

	unsigned long flags = irq_save();
	while (ip->locked) {
		irq_restore(flags);
		proc_wait(ip);
		flags = irq_save();
	}
	ip->locked = 1;
	irq_restore(flags);

	if (!ip->valid) {
		struct buf *bp = bio_read(ip->dev, IBLOCK(ip->inum, sb));
		if (bp) {
			struct dinode *dip = (struct dinode *)bp->data + ip->inum % IPB;
			ip->type = dip->type;
			ip->major = dip->major;
			ip->minor = dip->minor;
			ip->nlink = dip->nlink;
			ip->size = dip->size;
			for (int i = 0; i < NDIRECT + 2; i++) {
				ip->addrs[i] = dip->addrs[i];
			}
			bio_release(bp);
			ip->valid = 1;
		}
	}
}

void fs_iunlock(struct inode *ip) {
	if (ip == 0 || !ip->locked || ip->ref < 1) {
		return;
	}

	unsigned long flags = irq_save();
	ip->locked = 0;
	proc_wakeup(ip);
	irq_restore(flags);
}

void fs_iput(struct inode *ip) {
	unsigned long flags = irq_save();
	if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
		irq_restore(flags);
		fs_ilock(ip);
		fs_itrunc(ip);
		ip->type = 0;
		fs_iupdate(ip);
		ip->valid = 0;
		fs_iunlock(ip);
		flags = irq_save();
	}
	ip->ref--;
	irq_restore(flags);
}

void fs_iupdate(struct inode *ip) {
	struct buf *bp = bio_read(ip->dev, IBLOCK(ip->inum, sb));
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
	bio_write(bp);
	bio_release(bp);
}

static unsigned int balloc(unsigned int dev) {
	for (unsigned int b = 0; b < sb.size; b += BPB) {
		struct buf *bp = bio_read(dev, BBLOCK(b, sb));
		if (!bp) {
			return 0;
		}
		for (unsigned int bi = 0; bi < BPB && b + bi < sb.size; bi++) {
			unsigned int m = 1 << (bi % 8);
			if ((bp->data[bi / 8] & m) == 0) {
				bp->data[bi / 8] |= m;
				bio_write(bp);
				bio_release(bp);
				struct buf *zbp = bio_read(dev, b + bi);
				if (zbp) {
					memset(zbp->data, 0, BSIZE);
					bio_write(zbp);
					bio_release(zbp);
				}
				return b + bi;
			}
		}
		bio_release(bp);
	}
	return 0;
}

static void bfree(unsigned int dev, unsigned int b) {
	struct buf *bp = bio_read(dev, BBLOCK(b, sb));
	if (!bp) {
		return;
	}
	unsigned int bi = b % BPB;
	unsigned int m = 1 << (bi % 8);
	bp->data[bi / 8] &= ~m;
	bio_write(bp);
	bio_release(bp);
}

unsigned int fs_bmap(struct inode *ip, unsigned int bn) {
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
		struct buf *bp = bio_read(ip->dev, addr);
		if (!bp) {
			return 0;
		}
		unsigned int *a = (unsigned int *)bp->data;
		if ((addr = a[bn]) == 0) {
			addr = balloc(ip->dev);
			if (addr == 0) {
				bio_release(bp);
				return 0;
			}
			a[bn] = addr;
			bio_write(bp);
		}
		bio_release(bp);
		return addr;
	}
	bn -= NINDIRECT;

	if (bn < NDINDIRECT) {
		if ((addr = ip->addrs[NDIRECT + 1]) == 0) {
			addr = balloc(ip->dev);
			if (addr == 0) {
				return 0;
			}
			ip->addrs[NDIRECT + 1] = addr;
		}

		struct buf *bp = bio_read(ip->dev, addr);
		if (!bp) {
			return 0;
		}
		unsigned int *a = (unsigned int *)bp->data;
		unsigned int idx1 = bn / NINDIRECT;
		unsigned int idx2 = bn % NINDIRECT;

		if ((addr = a[idx1]) == 0) {
			addr = balloc(ip->dev);
			if (addr == 0) {
				bio_release(bp);
				return 0;
			}
			a[idx1] = addr;
			bio_write(bp);
		}
		bio_release(bp);

		bp = bio_read(ip->dev, addr);
		if (!bp) {
			return 0;
		}
		a = (unsigned int *)bp->data;
		if ((addr = a[idx2]) == 0) {
			addr = balloc(ip->dev);
			if (addr == 0) {
				bio_release(bp);
				return 0;
			}
			a[idx2] = addr;
			bio_write(bp);
		}
		bio_release(bp);
		return addr;
	}

	return 0;
}

struct inode *fs_idup(struct inode *ip) {
	unsigned long flags = irq_save();
	ip->ref++;
	irq_restore(flags);
	return ip;
}

void fs_iunlockput(struct inode *ip) {
	fs_iunlock(ip);
	fs_iput(ip);
}

int fs_readi(struct inode *ip, char *dst, unsigned int off, unsigned int n) {
	if (off > ip->size) {
		return 0;
	}
	if (off + n > ip->size) {
		n = ip->size - off;
	}

	unsigned int tot = 0;
	while (tot < n) {
		unsigned int bn = off / BSIZE;
		unsigned int addr = fs_bmap(ip, bn);
		if (addr == 0) {
			break;
		}

		struct buf *bp = bio_read(ip->dev, addr);
		if (!bp) {
			break;
		}

		unsigned int boff = off % BSIZE;
		unsigned int m = BSIZE - boff;
		if (m > n - tot) {
			m = n - tot;
		}

		memmove(dst, bp->data + boff, m);
		bio_release(bp);

		tot += m;
		off += m;
		dst += m;
	}

	return tot;
}

int fs_writei(struct inode *ip, const char *src, unsigned int off, unsigned int n) {
	if (off > ip->size || off + n < off) {
		return -1;
	}
	if (off + n > MAXFILE * BSIZE) {
		return -1;
	}

	unsigned int tot = 0;
	while (tot < n) {
		unsigned int bn = off / BSIZE;
		unsigned int addr = fs_bmap(ip, bn);
		if (addr == 0) {
			break;
		}

		struct buf *bp = bio_read(ip->dev, addr);
		if (!bp) {
			break;
		}

		unsigned int boff = off % BSIZE;
		unsigned int m = BSIZE - boff;
		if (m > n - tot) {
			m = n - tot;
		}

		memmove(bp->data + boff, src, m);
		bio_write(bp);
		bio_release(bp);

		tot += m;
		off += m;
		src += m;
	}

	if (off > ip->size) {
		ip->size = off;
	}
	fs_iupdate(ip);
	return tot;
}

void fs_itrunc(struct inode *ip) {
	for (int i = 0; i < NDIRECT; i++) {
		if (ip->addrs[i]) {
			bfree(ip->dev, ip->addrs[i]);
			ip->addrs[i] = 0;
		}
	}

	if (ip->addrs[NDIRECT]) {
		struct buf *bp = bio_read(ip->dev, ip->addrs[NDIRECT]);
		if (bp) {
			unsigned int *a = (unsigned int *)bp->data;
			for (unsigned int j = 0; j < NINDIRECT; j++) {
				if (a[j]) {
					bfree(ip->dev, a[j]);
				}
			}
			bio_release(bp);
		}
		bfree(ip->dev, ip->addrs[NDIRECT]);
		ip->addrs[NDIRECT] = 0;
	}

	if (ip->addrs[NDIRECT + 1]) {
		struct buf *bp = bio_read(ip->dev, ip->addrs[NDIRECT + 1]);
		if (bp) {
			unsigned int *a = (unsigned int *)bp->data;
			for (unsigned int i = 0; i < NINDIRECT; i++) {
				if (a[i]) {
					struct buf *bp2 = bio_read(ip->dev, a[i]);
					if (bp2) {
						unsigned int *a2 = (unsigned int *)bp2->data;
						for (unsigned int j = 0; j < NINDIRECT; j++) {
							if (a2[j]) {
								bfree(ip->dev, a2[j]);
							}
						}
						bio_release(bp2);
					}
					bfree(ip->dev, a[i]);
				}
			}
			bio_release(bp);
		}
		bfree(ip->dev, ip->addrs[NDIRECT + 1]);
		ip->addrs[NDIRECT + 1] = 0;
	}

	ip->size = 0;
	fs_iupdate(ip);
}

int fs_itrunc_to(struct inode *ip, unsigned int len) {
	if (len > ip->size) {
		return -1;
	}
	if (len == ip->size) {
		return 0;
	}
	if (len == 0) {
		fs_itrunc(ip);
		return 0;
	}

	unsigned int last_block = (len - 1) / BSIZE;
	unsigned int first_free = last_block + 1;

	unsigned int partial = len % BSIZE;
	if (partial != 0 && last_block < NDIRECT && ip->addrs[last_block]) {
		struct buf *bp = bio_read(ip->dev, ip->addrs[last_block]);
		if (bp) {
			memset(bp->data + partial, 0, BSIZE - partial);
			bio_write(bp);
			bio_release(bp);
		}
	}

	for (unsigned int i = first_free; i < NDIRECT; i++) {
		if (ip->addrs[i]) {
			bfree(ip->dev, ip->addrs[i]);
			ip->addrs[i] = 0;
		}
	}

	if (first_free <= NDIRECT + NINDIRECT - 1 && ip->addrs[NDIRECT]) {
		struct buf *bp = bio_read(ip->dev, ip->addrs[NDIRECT]);
		if (bp) {
			unsigned int *a = (unsigned int *)bp->data;
			unsigned int start = (first_free > NDIRECT) ? first_free - NDIRECT : 0;
			int modified = 0;
			for (unsigned int j = start; j < NINDIRECT; j++) {
				if (a[j]) {
					bfree(ip->dev, a[j]);
					a[j] = 0;
					modified = 1;
				}
			}
			if (modified && start > 0) {
				bio_write(bp);
			}
			bio_release(bp);
			if (start == 0) {
				bfree(ip->dev, ip->addrs[NDIRECT]);
				ip->addrs[NDIRECT] = 0;
			}
		}
	}

	if (first_free <= NDIRECT + NINDIRECT + NDINDIRECT - 1 && ip->addrs[NDIRECT + 1]) {
		struct buf *bp = bio_read(ip->dev, ip->addrs[NDIRECT + 1]);
		if (bp) {
			unsigned int *a = (unsigned int *)bp->data;
			unsigned int dind_start = (first_free > NDIRECT + NINDIRECT) ? first_free - NDIRECT - NINDIRECT : 0;
			unsigned int start_idx1 = dind_start / NINDIRECT;
			unsigned int start_idx2 = dind_start % NINDIRECT;
			int all_freed = 1;
			for (unsigned int i = 0; i < NINDIRECT; i++) {
				if (a[i] == 0) {
					continue;
				}
				if (i < start_idx1) {
					all_freed = 0;
					continue;
				}
				struct buf *bp2 = bio_read(ip->dev, a[i]);
				if (bp2) {
					unsigned int *a2 = (unsigned int *)bp2->data;
					unsigned int start2 = (i == start_idx1) ? start_idx2 : 0;
					int inner_all_freed = 1;
					for (unsigned int j = 0; j < NINDIRECT; j++) {
						if (a2[j] == 0) {
							continue;
						}
						if (j < start2) {
							inner_all_freed = 0;
							continue;
						}
						bfree(ip->dev, a2[j]);
						a2[j] = 0;
					}
					if (inner_all_freed) {
						bio_release(bp2);
						bfree(ip->dev, a[i]);
						a[i] = 0;
					} else {
						bio_write(bp2);
						bio_release(bp2);
						all_freed = 0;
					}
				}
			}
			bio_release(bp);
			if (all_freed) {
				bfree(ip->dev, ip->addrs[NDIRECT + 1]);
				ip->addrs[NDIRECT + 1] = 0;
			}
		}
	}

	ip->size = len;
	fs_iupdate(ip);
	return 0;
}

void fs_stati(struct inode *ip, struct stat *st) {
	st->st_dev = ip->dev;
	st->st_ino = ip->inum;
	st->st_mode = ip->type;
	st->st_nlink = ip->nlink;
	st->st_size = ip->size;
}

struct inode *fs_ialloc(unsigned int dev, unsigned short type) {
	for (unsigned int inum = 1; inum < sb.ninodes; inum++) {
		struct buf *bp = bio_read(dev, IBLOCK(inum, sb));
		if (!bp) {
			continue;
		}
		struct dinode *dip = (struct dinode *)bp->data + inum % IPB;
		if (dip->type == 0) {
			memset(dip, 0, sizeof(*dip));
			dip->type = type;
			bio_write(bp);
			bio_release(bp);
			return fs_iget(dev, inum);
		}
		bio_release(bp);
	}
	return 0;
}

int fs_dirlink(struct inode *dp, char *name, unsigned int inum) {
	if (dp->type != T_DIR) {
		return -1;
	}

	struct inode *ip = fs_dirlookup(dp, name, 0);
	if (ip != 0) {
		fs_iput(ip);
		return -1;
	}

	struct dirent de;
	unsigned int off;
	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (fs_readi(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
			return -1;
		}
		if (de.inum == 0) {
			break;
		}
	}

	strncpy(de.name, name, DIRSIZ);
	de.inum = inum;
	if (fs_writei(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
		return -1;
	}
	return 0;
}

int fs_isdirempty(struct inode *dp) {
	struct dirent de;
	for (unsigned int off = 2 * sizeof(de); off < dp->size; off += sizeof(de)) {
		if (fs_readi(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
			break;
		}
		if (de.inum != 0) {
			return 0;
		}
	}
	return 1;
}

struct inode *fs_create(char *path, unsigned short type, unsigned short major, unsigned short minor) {
	char name[DIRSIZ];
	struct inode *dp = fs_nameiparent(path, name);
	if (dp == 0) {
		return 0;
	}

	fs_ilock(dp);

	struct inode *ip = fs_dirlookup(dp, name, 0);
	if (ip != 0) {
		fs_iunlockput(dp);
		fs_ilock(ip);
		if (type == T_FILE && ip->type == T_FILE) {
			return ip;
		}
		fs_iunlockput(ip);
		return 0;
	}

	ip = fs_ialloc(dp->dev, type);
	if (ip == 0) {
		fs_iunlockput(dp);
		return 0;
	}

	fs_ilock(ip);
	ip->major = major;
	ip->minor = minor;
	ip->nlink = 1;
	fs_iupdate(ip);

	if (type == T_DIR) {
		dp->nlink++;
		fs_iupdate(dp);
		if (fs_dirlink(ip, ".", ip->inum) < 0 || fs_dirlink(ip, "..", dp->inum) < 0) {
			ip->nlink = 0;
			fs_iupdate(ip);
			fs_iunlockput(ip);
			dp->nlink--;
			fs_iupdate(dp);
			fs_iunlockput(dp);
			return 0;
		}
	}

	if (fs_dirlink(dp, name, ip->inum) < 0) {
		if (type == T_DIR) {
			dp->nlink--;
			fs_iupdate(dp);
		}
		ip->nlink = 0;
		fs_iupdate(ip);
		fs_iunlockput(ip);
		fs_iunlockput(dp);
		return 0;
	}

	fs_iunlockput(dp);
	return ip;
}

struct inode *fs_dirlookup(struct inode *dp, char *name, unsigned int *poff) {
	if (dp->type != T_DIR) {
		return 0;
	}

	struct dirent de;
	for (unsigned int off = 0; off < dp->size; off += sizeof(de)) {
		if (fs_readi(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
			break;
		}
		if (de.inum == 0) {
			continue;
		}
		if (strncmp(name, de.name, DIRSIZ) == 0) {
			if (poff) {
				*poff = off;
			}
			return fs_iget(dp->dev, de.inum);
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
		ip = fs_iget(0, ROOTINO);
	} else {
		ip = fs_idup(current->cwd);
	}

	while ((path = skipelem(path, name)) != 0) {
		fs_ilock(ip);
		if (ip->type != T_DIR) {
			fs_iunlockput(ip);
			return 0;
		}
		if (nameiparent && *path == '\0') {
			fs_iunlock(ip);
			return ip;
		}
		struct inode *next = fs_dirlookup(ip, name, 0);
		fs_iunlockput(ip);
		if (next == 0) {
			return 0;
		}
		ip = next;
	}

	if (nameiparent) {
		fs_iput(ip);
		return 0;
	}
	return ip;
}

struct inode *fs_namei(char *path) {
	char name[DIRSIZ];
	return namex(path, 0, name);
}

struct inode *fs_nameiparent(char *path, char *name) {
	return namex(path, 1, name);
}
