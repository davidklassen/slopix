#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ROOTINO 1
#define BSIZE	1024

#define NDIRECT	  12
#define NINDIRECT (BSIZE / sizeof(uint32_t))
#define MAXFILE	  (NDIRECT + NINDIRECT)

#define DIRSIZ	14
#define FSMAGIC 0x10203040

#define T_FREE	 0
#define T_FILE	 1
#define T_DIR	 2
#define T_DEVICE 3

struct superblock {
	uint32_t magic;
	uint32_t size;
	uint32_t nblocks;
	uint32_t ninodes;
	uint32_t inodestart;
	uint32_t bmapstart;
};

struct dinode {
	uint16_t type;
	uint16_t major;
	uint16_t minor;
	uint16_t nlink;
	uint32_t size;
	uint32_t addrs[NDIRECT + 1];
};

struct dirent {
	uint16_t inum;
	char name[DIRSIZ];
};

#define IPB (BSIZE / sizeof(struct dinode))

static int fsfd;
static struct superblock sb;
static uint32_t freeblock;
static uint32_t freeinode;
static char zeroes[BSIZE];

static void wsect(uint32_t sec, void *buf) {
	if (lseek(fsfd, sec * BSIZE, SEEK_SET) != sec * BSIZE) {
		perror("lseek");
		exit(1);
	}
	if (write(fsfd, buf, BSIZE) != BSIZE) {
		perror("write");
		exit(1);
	}
}

static void rsect(uint32_t sec, void *buf) {
	if (lseek(fsfd, sec * BSIZE, SEEK_SET) != sec * BSIZE) {
		perror("lseek");
		exit(1);
	}
	if (read(fsfd, buf, BSIZE) != BSIZE) {
		perror("read");
		exit(1);
	}
}

static void winode(uint32_t inum, struct dinode *ip) {
	char buf[BSIZE];
	uint32_t bn = inum / IPB + sb.inodestart;
	rsect(bn, buf);
	struct dinode *dip = ((struct dinode *)buf) + (inum % IPB);
	*dip = *ip;
	wsect(bn, buf);
}

static void rinode(uint32_t inum, struct dinode *ip) {
	char buf[BSIZE];
	uint32_t bn = inum / IPB + sb.inodestart;
	rsect(bn, buf);
	*ip = ((struct dinode *)buf)[inum % IPB];
}

static uint32_t ialloc(uint16_t type) {
	uint32_t inum = freeinode++;
	struct dinode din;

	memset(&din, 0, sizeof(din));
	din.type = type;
	din.nlink = 1;
	din.size = 0;
	winode(inum, &din);

	return inum;
}

static void balloc(uint32_t used) {
	unsigned char buf[BSIZE];

	printf("balloc: marking %d blocks as used\n", used);
	assert(used < BSIZE * 8);

	memset(buf, 0, BSIZE);
	for (uint32_t i = 0; i < used; i++) {
		buf[i / 8] |= (1 << (i % 8));
	}
	wsect(sb.bmapstart, buf);
}

static void iappend(uint32_t inum, void *data, uint32_t n) {
	struct dinode din;
	char *p = (char *)data;
	uint32_t off, fbn, n1;
	char buf[BSIZE];
	uint32_t indirect[NINDIRECT];
	uint32_t x;

	rinode(inum, &din);
	off = din.size;

	while (n > 0) {
		fbn = off / BSIZE;
		assert(fbn < MAXFILE);

		if (fbn < NDIRECT) {
			if (din.addrs[fbn] == 0) {
				din.addrs[fbn] = freeblock++;
			}
			x = din.addrs[fbn];
		} else {
			if (din.addrs[NDIRECT] == 0) {
				din.addrs[NDIRECT] = freeblock++;
				memset(indirect, 0, sizeof(indirect));
				wsect(din.addrs[NDIRECT], indirect);
			}
			rsect(din.addrs[NDIRECT], indirect);
			if (indirect[fbn - NDIRECT] == 0) {
				indirect[fbn - NDIRECT] = freeblock++;
				wsect(din.addrs[NDIRECT], indirect);
			}
			x = indirect[fbn - NDIRECT];
		}

		n1 = BSIZE - (off % BSIZE);
		if (n1 > n) {
			n1 = n;
		}

		rsect(x, buf);
		memcpy(buf + (off % BSIZE), p, n1);
		wsect(x, buf);

		n -= n1;
		off += n1;
		p += n1;
	}

	din.size = off;
	winode(inum, &din);
}

static void usage(const char *prog) {
	fprintf(stderr, "Usage: %s <image> [-s blocks] [-i inodes] [hostfile:/imgpath ...]\n", prog);
	fprintf(stderr, "  -s blocks   Total filesystem size in blocks (default: 1024)\n");
	fprintf(stderr, "  -i inodes   Number of inodes (default: 200)\n");
	exit(1);
}

int main(int argc, char **argv) {
	uint32_t size = 1024;
	uint32_t ninodes = 200;
	char *imgfile = NULL;
	int i;

	if (argc < 2) {
		usage(argv[0]);
	}

	imgfile = argv[1];

	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "-s") == 0) {
			if (i + 1 >= argc) {
				usage(argv[0]);
			}
			size = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-i") == 0) {
			if (i + 1 >= argc) {
				usage(argv[0]);
			}
			ninodes = atoi(argv[++i]);
		} else if (strchr(argv[i], ':') != NULL) {
			break;
		} else {
			usage(argv[0]);
		}
	}

	int file_start = i;

	assert(BSIZE % sizeof(struct dinode) == 0);
	assert(BSIZE % sizeof(struct dirent) == 0);

	fsfd = open(imgfile, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fsfd < 0) {
		perror(imgfile);
		exit(1);
	}

	uint32_t ninodeblocks = (ninodes + IPB - 1) / IPB;
	uint32_t nbitmap = 1;
	uint32_t nmeta = 2 + ninodeblocks + nbitmap;
	uint32_t nblocks = size - nmeta;

	sb.magic = FSMAGIC;
	sb.size = size;
	sb.nblocks = nblocks;
	sb.ninodes = ninodes;
	sb.inodestart = 2;
	sb.bmapstart = 2 + ninodeblocks;

	printf("mkfs: size=%d blocks=%d inodes=%d\n", sb.size, sb.nblocks, sb.ninodes);
	printf("mkfs: inodestart=%d bmapstart=%d\n", sb.inodestart, sb.bmapstart);

	freeblock = nmeta;
	freeinode = 1;

	for (uint32_t i = 0; i < size; i++) {
		wsect(i, zeroes);
	}

	char buf[BSIZE];
	memset(buf, 0, BSIZE);
	memcpy(buf, &sb, sizeof(sb));
	wsect(1, buf);

	uint32_t rootino = ialloc(T_DIR);
	assert(rootino == ROOTINO);

	struct dirent de;
	memset(&de, 0, sizeof(de));

	de.inum = rootino;
	strncpy(de.name, ".", DIRSIZ);
	iappend(rootino, &de, sizeof(de));

	de.inum = rootino;
	strncpy(de.name, "..", DIRSIZ);
	iappend(rootino, &de, sizeof(de));

	for (int fi = file_start; fi < argc; fi++) {
		char *arg = argv[fi];
		char *colon = strchr(arg, ':');
		if (colon == NULL) {
			fprintf(stderr, "mkfs: invalid file spec '%s' (expected hostfile:/imgpath)\n", arg);
			exit(1);
		}

		*colon = '\0';
		char *hostpath = arg;
		char *imgpath = colon + 1;

		if (imgpath[0] == '/') {
			imgpath++;
		}

		if (strlen(imgpath) == 0 || strlen(imgpath) >= DIRSIZ) {
			fprintf(stderr, "mkfs: invalid image path '%s'\n", imgpath);
			exit(1);
		}

		int fd = open(hostpath, O_RDONLY);
		if (fd < 0) {
			perror(hostpath);
			exit(1);
		}

		uint32_t inum = ialloc(T_FILE);

		de.inum = inum;
		strncpy(de.name, imgpath, DIRSIZ);
		iappend(rootino, &de, sizeof(de));

		char fbuf[BSIZE];
		int n;
		while ((n = read(fd, fbuf, sizeof(fbuf))) > 0) {
			iappend(inum, fbuf, n);
		}

		printf("mkfs: added '%s' as '/%s' (inode %d)\n", hostpath, imgpath, inum);
		close(fd);
	}

	struct dinode din;
	rinode(rootino, &din);
	uint32_t off = din.size;
	off = ((off / BSIZE) + 1) * BSIZE;
	din.size = off;
	winode(rootino, &din);

	balloc(freeblock);

	close(fsfd);
	printf("mkfs: done\n");

	return 0;
}
