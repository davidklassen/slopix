#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ROOTINO 1
#define BSIZE	1024

#define NDIRECT	   11
#define NINDIRECT  (BSIZE / sizeof(uint32_t))
#define NDINDIRECT (NINDIRECT * NINDIRECT)
#define MAXFILE	   (NDIRECT + NINDIRECT + NDINDIRECT)

#define DIRSIZ	62
#define FSMAGIC 0x10203040

#define T_FREE	  0
#define T_FILE	  1
#define T_DIR	  2
#define T_DEVICE  3
#define T_BDEVICE 4

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
	uint32_t addrs[NDIRECT + 2];
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
	uint32_t dindirect[NINDIRECT];
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
		} else if (fbn < NDIRECT + NINDIRECT) {
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
		} else {
			uint32_t bn2 = fbn - NDIRECT - NINDIRECT;
			uint32_t idx1 = bn2 / NINDIRECT;
			uint32_t idx2 = bn2 % NINDIRECT;

			if (din.addrs[NDIRECT + 1] == 0) {
				din.addrs[NDIRECT + 1] = freeblock++;
				memset(dindirect, 0, sizeof(dindirect));
				wsect(din.addrs[NDIRECT + 1], dindirect);
			}
			rsect(din.addrs[NDIRECT + 1], dindirect);

			if (dindirect[idx1] == 0) {
				dindirect[idx1] = freeblock++;
				wsect(din.addrs[NDIRECT + 1], dindirect);
				memset(indirect, 0, sizeof(indirect));
				wsect(dindirect[idx1], indirect);
			}
			rsect(dindirect[idx1], indirect);

			if (indirect[idx2] == 0) {
				indirect[idx2] = freeblock++;
				wsect(dindirect[idx1], indirect);
			}
			x = indirect[idx2];
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

static uint32_t lookup_dir(const char *path) {
	if (path[0] == '/') {
		path++;
	}
	if (path[0] == '\0') {
		return ROOTINO;
	}

	uint32_t parent = ROOTINO;
	char component[DIRSIZ + 1];

	while (*path) {
		int i = 0;
		while (*path && *path != '/' && i < DIRSIZ) {
			component[i++] = *path++;
		}
		component[i] = '\0';
		if (*path == '/') {
			path++;
		}

		struct dinode din;
		rinode(parent, &din);
		if (din.type != T_DIR) {
			return 0;
		}

		int found = 0;
		struct dirent de;
		for (uint32_t off = 0; off < din.size; off += sizeof(de)) {
			char buf[BSIZE];
			uint32_t bn = off / BSIZE;
			uint32_t boff = off % BSIZE;
			if (bn < NDIRECT) {
				if (din.addrs[bn] == 0) {
					break;
				}
				rsect(din.addrs[bn], buf);
			} else {
				uint32_t indirect[NINDIRECT];
				if (din.addrs[NDIRECT] == 0) {
					break;
				}
				rsect(din.addrs[NDIRECT], indirect);
				if (indirect[bn - NDIRECT] == 0) {
					break;
				}
				rsect(indirect[bn - NDIRECT], buf);
			}
			memcpy(&de, buf + boff, sizeof(de));
			if (de.inum != 0 && strncmp(de.name, component, DIRSIZ) == 0) {
				parent = de.inum;
				found = 1;
				break;
			}
		}
		if (!found) {
			return 0;
		}
	}
	return parent;
}

static uint32_t create_dir(uint32_t parent_inum, const char *name) {
	uint32_t inum = ialloc(T_DIR);

	struct dirent de;
	memset(&de, 0, sizeof(de));

	de.inum = inum;
	strncpy(de.name, ".", DIRSIZ);
	iappend(inum, &de, sizeof(de));

	de.inum = parent_inum;
	strncpy(de.name, "..", DIRSIZ);
	iappend(inum, &de, sizeof(de));

	de.inum = inum;
	strncpy(de.name, name, DIRSIZ);
	iappend(parent_inum, &de, sizeof(de));

	struct dinode pdin;
	rinode(parent_inum, &pdin);
	pdin.nlink++;
	winode(parent_inum, &pdin);

	return inum;
}

static uint32_t create_device(uint32_t parent_inum, const char *name, uint16_t type, uint16_t major, uint16_t minor) {
	uint32_t inum = freeinode++;

	struct dinode din;
	memset(&din, 0, sizeof(din));
	din.type = type;
	din.major = major;
	din.minor = minor;
	din.nlink = 1;
	din.size = 0;
	winode(inum, &din);

	struct dirent de;
	memset(&de, 0, sizeof(de));
	de.inum = inum;
	strncpy(de.name, name, DIRSIZ);
	iappend(parent_inum, &de, sizeof(de));

	return inum;
}

static void usage(const char *prog) {
	fprintf(stderr, "Usage: %s <image> [-s blocks] [-i inodes] [spec ...]\n", prog);
	fprintf(stderr, "  -s blocks   Total filesystem size in blocks (default: 1024)\n");
	fprintf(stderr, "  -i inodes   Number of inodes (default: 200)\n");
	fprintf(stderr, "\nFile specifications:\n");
	fprintf(stderr, "  hostfile:/imgpath        Copy host file to image path\n");
	fprintf(stderr, "  :dir:/path               Create directory\n");
	fprintf(stderr, "  :cdev:/path:major:minor  Create character device\n");
	fprintf(stderr, "  :bdev:/path:major:minor  Create block device\n");
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

		if (strncmp(arg, ":dir:", 5) == 0) {
			char *path = arg + 5;
			if (path[0] == '/') {
				path++;
			}
			char *slash = strrchr(path, '/');
			char *name;
			uint32_t parent;
			if (slash) {
				*slash = '\0';
				name = slash + 1;
				parent = lookup_dir(path);
				if (parent == 0) {
					fprintf(stderr, "mkfs: parent directory '%s' not found\n", path);
					exit(1);
				}
			} else {
				name = path;
				parent = rootino;
			}
			if (strlen(name) == 0 || strlen(name) >= DIRSIZ) {
				fprintf(stderr, "mkfs: invalid directory name '%s'\n", name);
				exit(1);
			}
			uint32_t inum = create_dir(parent, name);
			printf("mkfs: created directory '/%s%s%s' (inode %d)\n",
			       slash ? arg + 5 : "",
			       slash ? "/" : "",
			       name,
			       inum);
			continue;
		}

		if (strncmp(arg, ":cdev:", 6) == 0 || strncmp(arg, ":bdev:", 6) == 0) {
			int is_bdev = (arg[1] == 'b');
			char *rest = arg + 6;
			char *colon1 = strchr(rest, ':');
			if (!colon1) {
				fprintf(stderr, "mkfs: invalid device spec '%s'\n", arg);
				exit(1);
			}
			*colon1 = '\0';
			char *colon2 = strchr(colon1 + 1, ':');
			if (!colon2) {
				fprintf(stderr, "mkfs: invalid device spec '%s'\n", arg);
				exit(1);
			}
			*colon2 = '\0';
			char *path = rest;
			int major = atoi(colon1 + 1);
			int minor = atoi(colon2 + 1);

			if (path[0] == '/') {
				path++;
			}
			char *slash = strrchr(path, '/');
			char *name;
			uint32_t parent;
			if (slash) {
				*slash = '\0';
				name = slash + 1;
				parent = lookup_dir(path);
				if (parent == 0) {
					fprintf(stderr, "mkfs: parent directory '%s' not found\n", path);
					exit(1);
				}
			} else {
				name = path;
				parent = rootino;
			}
			if (strlen(name) == 0 || strlen(name) >= DIRSIZ) {
				fprintf(stderr, "mkfs: invalid device name '%s'\n", name);
				exit(1);
			}
			uint16_t type = is_bdev ? T_BDEVICE : T_DEVICE;
			uint32_t inum = create_device(parent, name, type, major, minor);
			printf("mkfs: created %s device '%s' (%d,%d) (inode %d)\n",
			       is_bdev ? "block" : "char",
			       name,
			       major,
			       minor,
			       inum);
			continue;
		}

		char *colon = strchr(arg, ':');
		if (colon == NULL) {
			fprintf(stderr, "mkfs: invalid file spec '%s'\n", arg);
			exit(1);
		}

		*colon = '\0';
		char *hostpath = arg;
		char *imgpath = colon + 1;

		if (imgpath[0] == '/') {
			imgpath++;
		}

		char *slash = strrchr(imgpath, '/');
		char *name;
		uint32_t parent;
		if (slash) {
			*slash = '\0';
			name = slash + 1;
			parent = lookup_dir(imgpath);
			if (parent == 0) {
				fprintf(stderr, "mkfs: parent directory '%s' not found\n", imgpath);
				exit(1);
			}
		} else {
			name = imgpath;
			parent = rootino;
		}

		if (strlen(name) == 0 || strlen(name) >= DIRSIZ) {
			fprintf(stderr, "mkfs: invalid image path '%s'\n", name);
			exit(1);
		}

		int fd = open(hostpath, O_RDONLY);
		if (fd < 0) {
			perror(hostpath);
			exit(1);
		}

		uint32_t inum = ialloc(T_FILE);

		de.inum = inum;
		strncpy(de.name, name, DIRSIZ);
		iappend(parent, &de, sizeof(de));

		char fbuf[BSIZE];
		int n;
		while ((n = read(fd, fbuf, sizeof(fbuf))) > 0) {
			iappend(inum, fbuf, n);
		}

		printf("mkfs: added '%s' as '%s' (inode %d)\n", hostpath, name, inum);
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
