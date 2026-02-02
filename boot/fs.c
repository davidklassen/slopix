#define BSIZE	  1024
#define ROOTINO	  1
#define FSMAGIC	  0x10203040
#define NDIRECT	  11
#define NINDIRECT (BSIZE / sizeof(unsigned int))
#define DIRSIZ	  62
#define IPB	  16
#define T_FILE	  1
#define T_DIR	  2

struct superblock {
	unsigned int magic;
	unsigned int size;
	unsigned int nblocks;
	unsigned int ninodes;
	unsigned int inodestart;
	unsigned int bmapstart;
};

struct dinode {
	unsigned short type;
	unsigned short major;
	unsigned short minor;
	unsigned short nlink;
	unsigned int size;
	unsigned int addrs[NDIRECT + 2];
};

struct dirent {
	unsigned short inum;
	char name[DIRSIZ];
};

#define BLOCK_BUF_PA	0x40073000UL
#define INDIRECT_BUF_PA 0x40073400UL
#define SB_BUF_PA	0x40073800UL

static unsigned char *block_buf = (unsigned char *)BLOCK_BUF_PA;
static unsigned char *indirect_buf = (unsigned char *)INDIRECT_BUF_PA;
static struct superblock *sb = (struct superblock *)SB_BUF_PA;

void uart_puts(const char *s);
void uart_puthex(unsigned int val);
int virtio_read(unsigned long sector, void *buf);

static int read_block(unsigned int blockno, void *buf) {
	unsigned long sector = blockno * 2;
	unsigned char *p = buf;

	if (virtio_read(sector, p) < 0)
		return -1;
	if (virtio_read(sector + 1, p + 512) < 0)
		return -1;

	return 0;
}

int fs_init(void) {
	if (read_block(1, sb) < 0) {
		uart_puts("fs: read superblock failed\n");
		return -1;
	}

	if (sb->magic != FSMAGIC) {
		uart_puts("fs: bad magic ");
		uart_puthex(sb->magic);
		uart_puts("\n");
		return -1;
	}

	return 0;
}

static int read_inode(unsigned int inum, struct dinode *dip) {
	unsigned int block = inum / IPB + sb->inodestart;
	unsigned int offset = (inum % IPB) * sizeof(struct dinode);

	if (read_block(block, block_buf) < 0)
		return -1;

	struct dinode *src = (struct dinode *)(block_buf + offset);
	*dip = *src;
	return 0;
}

static unsigned int bmap(struct dinode *dip, unsigned int bn) {
	if (bn < NDIRECT) {
		return dip->addrs[bn];
	}

	bn -= NDIRECT;

	if (bn < NINDIRECT) {
		if (read_block(dip->addrs[NDIRECT], indirect_buf) < 0)
			return 0;
		unsigned int *addrs = (unsigned int *)indirect_buf;
		return addrs[bn];
	}

	bn -= NINDIRECT;

	unsigned int dindirect_block = dip->addrs[NDIRECT + 1];
	if (read_block(dindirect_block, indirect_buf) < 0)
		return 0;

	unsigned int *l1 = (unsigned int *)indirect_buf;
	unsigned int l1_idx = bn / NINDIRECT;
	unsigned int l2_idx = bn % NINDIRECT;

	if (read_block(l1[l1_idx], indirect_buf) < 0)
		return 0;

	unsigned int *l2 = (unsigned int *)indirect_buf;
	return l2[l2_idx];
}

static int streq(const char *a, const char *b) {
	while (*a && *b) {
		if (*a != *b)
			return 0;
		a++;
		b++;
	}
	return *a == *b;
}

static const char *skipelem(const char *path, char *name) {
	while (*path == '/')
		path++;

	if (*path == 0)
		return 0;

	const char *start = path;
	while (*path != '/' && *path != 0)
		path++;

	int len = path - start;
	if (len >= DIRSIZ)
		len = DIRSIZ - 1;

	for (int i = 0; i < len; i++)
		name[i] = start[i];
	name[len] = 0;

	while (*path == '/')
		path++;

	return path;
}

static unsigned int dirlookup(struct dinode *dir, const char *name) {
	struct dirent *de;
	unsigned int nblocks = dir->size / BSIZE;
	unsigned int remainder = dir->size % BSIZE;

	for (unsigned int i = 0; i <= nblocks; i++) {
		unsigned int blockno = bmap(dir, i);
		if (blockno == 0)
			continue;

		if (read_block(blockno, block_buf) < 0)
			return 0;

		unsigned int bytes = (i < nblocks) ? BSIZE : remainder;
		unsigned int nentries = bytes / sizeof(struct dirent);

		de = (struct dirent *)block_buf;
		for (unsigned int j = 0; j < nentries; j++) {
			if (de[j].inum == 0)
				continue;
			if (streq(de[j].name, name))
				return de[j].inum;
		}
	}

	return 0;
}

int fs_read_file(const char *path, void *buf, unsigned int max_size) {
	if (path[0] != '/') {
		uart_puts("fs: path must be absolute\n");
		return -1;
	}

	struct dinode inode;
	char name[DIRSIZ];

	if (read_inode(ROOTINO, &inode) < 0)
		return -1;

	const char *p = path;
	while ((p = skipelem(p, name)) != 0) {
		if (inode.type != T_DIR) {
			uart_puts("fs: not a directory\n");
			return -1;
		}

		unsigned int inum = dirlookup(&inode, name);
		if (inum == 0) {
			uart_puts("fs: ");
			uart_puts(name);
			uart_puts(" not found\n");
			return -1;
		}

		if (read_inode(inum, &inode) < 0)
			return -1;
	}

	if (inode.type != T_FILE) {
		uart_puts("fs: not a file\n");
		return -1;
	}

	unsigned int size = inode.size;
	if (size > max_size) {
		uart_puts("fs: file too large\n");
		return -1;
	}

	unsigned char *dst = buf;
	unsigned int off = 0;

	while (off < size) {
		unsigned int bn = off / BSIZE;
		unsigned int blockno = bmap(&inode, bn);
		if (blockno == 0)
			return -1;

		if (read_block(blockno, block_buf) < 0)
			return -1;

		unsigned int block_off = off % BSIZE;
		unsigned int chunk = BSIZE - block_off;
		if (off + chunk > size)
			chunk = size - off;

		for (unsigned int i = 0; i < chunk; i++)
			dst[off + i] = block_buf[block_off + i];

		off += chunk;
	}

	return size;
}
