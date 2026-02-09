#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

struct fs_dirent {
	uint16_t inum;
	char name[DIRSIZ];
};

#define IPB (BSIZE / sizeof(struct dinode))

static int fsfd;
static struct superblock sb;
static uint32_t freeblock;
static uint32_t freeinode;
static char zeroes[BSIZE];
static uint32_t nfiles;
static uint32_t ndirs;
static uint32_t ndevs;

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
	if (inum >= sb.ninodes) {
		fprintf(stderr, "mkfs: out of inodes (max %d)\n", sb.ninodes);
		exit(1);
	}
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
	uint32_t bpb = BSIZE * 8;

	for (uint32_t b = 0; b < used; b += bpb) {
		memset(buf, 0, BSIZE);
		uint32_t end = b + bpb;
		if (end > used) {
			end = used;
		}
		for (uint32_t i = b; i < end; i++) {
			buf[(i - b) / 8] |= (1 << ((i - b) % 8));
		}
		wsect(sb.bmapstart + b / bpb, buf);
	}
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
		struct fs_dirent de;
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

	struct fs_dirent de;
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

	struct fs_dirent de;
	memset(&de, 0, sizeof(de));
	de.inum = inum;
	strncpy(de.name, name, DIRSIZ);
	iappend(parent_inum, &de, sizeof(de));

	return inum;
}

#define MAX_IGNORE 64
static char *ignore_patterns[MAX_IGNORE];
static int ignore_count = 0;

static void clear_ignore_patterns(void) {
	for (int i = 0; i < ignore_count; i++) {
		free(ignore_patterns[i]);
		ignore_patterns[i] = NULL;
	}
	ignore_count = 0;
}

static void load_mkfsignore(void) {
	clear_ignore_patterns();

	FILE *f = fopen(".mkfsignore", "r");
	if (f == NULL) {
		return;
	}

	char line[256];
	while (fgets(line, sizeof(line), f) != NULL && ignore_count < MAX_IGNORE) {
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
			line[--len] = '\0';
		}
		if (len == 0 || line[0] == '#') {
			continue;
		}
		ignore_patterns[ignore_count++] = strdup(line);
	}
	fclose(f);
}

static int should_ignore(const char *name) {
	size_t namelen = strlen(name);
	for (int i = 0; i < ignore_count; i++) {
		const char *pat = ignore_patterns[i];
		size_t patlen = strlen(pat);
		if (pat[0] == '*' && patlen > 1) {
			const char *suffix = pat + 1;
			size_t suffixlen = patlen - 1;
			if (namelen >= suffixlen &&
			    strcmp(name + namelen - suffixlen, suffix) == 0) {
				return 1;
			}
		} else {
			if (strncmp(name, pat, patlen) == 0) {
				return 1;
			}
		}
	}
	return 0;
}

static uint32_t lookup_in_dir(uint32_t parent, const char *name) {
	struct dinode din;
	rinode(parent, &din);
	if (din.type != T_DIR) {
		return 0;
	}

	struct fs_dirent de;
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
		if (de.inum != 0 && strncmp(de.name, name, DIRSIZ) == 0) {
			return de.inum;
		}
	}
	return 0;
}

static uint32_t lookup_or_create_path(const char *path) {
	if (path[0] == '/') {
		path++;
	}
	if (path[0] == '\0') {
		return ROOTINO;
	}

	uint32_t parent = ROOTINO;
	char pathcopy[512];
	strncpy(pathcopy, path, sizeof(pathcopy) - 1);
	pathcopy[sizeof(pathcopy) - 1] = '\0';

	char *saveptr;
	char *component = strtok_r(pathcopy, "/", &saveptr);
	while (component != NULL) {
		uint32_t found = lookup_in_dir(parent, component);
		if (found == 0) {
			found = create_dir(parent, component);
			ndirs++;
		}
		parent = found;
		component = strtok_r(NULL, "/", &saveptr);
	}
	return parent;
}

static void copy_file_to_image(const char *hostpath, uint32_t parent, const char *name) {
	int fd = open(hostpath, O_RDONLY);
	if (fd < 0) {
		perror(hostpath);
		exit(1);
	}

	uint32_t inum = ialloc(T_FILE);

	struct fs_dirent de;
	memset(&de, 0, sizeof(de));
	de.inum = inum;
	strncpy(de.name, name, DIRSIZ);
	iappend(parent, &de, sizeof(de));

	char fbuf[BSIZE];
	int n;
	while ((n = read(fd, fbuf, sizeof(fbuf))) > 0) {
		iappend(inum, fbuf, n);
	}

	nfiles++;
	close(fd);
}

static void sync_source_dir(const char *hostpath, const char *imgpath) {
	DIR *d = opendir(hostpath);
	if (d == NULL) {
		perror(hostpath);
		exit(1);
	}

	uint32_t parent = lookup_or_create_path(imgpath);

	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
			continue;
		}
		if (should_ignore(ent->d_name)) {
			continue;
		}

		char childhostpath[512], childimgpath[512];
		snprintf(childhostpath, sizeof(childhostpath), "%s/%s", hostpath, ent->d_name);
		snprintf(childimgpath, sizeof(childimgpath), "%s/%s", imgpath, ent->d_name);

		struct stat st;
		if (stat(childhostpath, &st) < 0) {
			perror(childhostpath);
			continue;
		}

		if (S_ISDIR(st.st_mode)) {
			sync_source_dir(childhostpath, childimgpath);
		} else if (S_ISREG(st.st_mode)) {
			if (strlen(ent->d_name) >= DIRSIZ) {
				fprintf(stderr, "mkfs: name too long '%s'\n", ent->d_name);
				continue;
			}
			copy_file_to_image(childhostpath, parent, ent->d_name);
		}
	}
	closedir(d);
}

static void usage(const char *prog) {
	fprintf(stderr, "Usage: %s <image> [-s blocks] [-i inodes] [spec ...] [-m source:target]\n", prog);
	fprintf(stderr, "  -s blocks           Total filesystem size in blocks (default: 1024)\n");
	fprintf(stderr, "  -i inodes           Number of inodes (default: 200)\n");
	fprintf(stderr, "  -m source:target    Recursively copy source directory to target path\n");
	fprintf(stderr, "\nFile specifications:\n");
	fprintf(stderr, "  hostfile:/imgpath        Copy host file to image path\n");
	fprintf(stderr, "  :dir:/path               Create directory\n");
	fprintf(stderr, "  :cdev:/path:major:minor  Create character device\n");
	fprintf(stderr, "  :bdev:/path:major:minor  Create block device\n");
	fprintf(stderr, "\nExamples:\n");
	fprintf(stderr, "  -m .build/out:/          Copy .build/out/* to /\n");
	fprintf(stderr, "  -m .:src                 Copy source tree to /src/\n");
	exit(1);
}

#define MAX_MAPS 32
struct dir_map {
	char *src;
	char *dst;
};

int main(int argc, char **argv) {
	uint32_t size = 1024;
	uint32_t ninodes = 200;
	char *imgfile = NULL;
	struct dir_map dir_maps[MAX_MAPS];
	int dir_map_count = 0;
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
		} else if (strcmp(argv[i], "-m") == 0) {
			if (i + 1 >= argc) {
				usage(argv[0]);
			}
			char *arg = argv[++i];
			char *colon = strchr(arg, ':');
			if (colon == NULL) {
				fprintf(stderr, "mkfs: -m requires source:target format\n");
				exit(1);
			}
			if (dir_map_count >= MAX_MAPS) {
				fprintf(stderr, "mkfs: too many -m mappings (max %d)\n", MAX_MAPS);
				exit(1);
			}
			*colon = '\0';
			dir_maps[dir_map_count].src = arg;
			dir_maps[dir_map_count].dst = colon + 1;
			dir_map_count++;
		} else if (strchr(argv[i], ':') != NULL) {
			break;
		} else {
			usage(argv[0]);
		}
	}

	int file_start = i;

	assert(BSIZE % sizeof(struct dinode) == 0);
	assert(BSIZE % sizeof(struct fs_dirent) == 0);

	fsfd = open(imgfile, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fsfd < 0) {
		perror(imgfile);
		exit(1);
	}

	uint32_t ninodeblocks = (ninodes + IPB - 1) / IPB;
	uint32_t nbitmap = (size + BSIZE * 8 - 1) / (BSIZE * 8);
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

	struct fs_dirent de;
	memset(&de, 0, sizeof(de));

	de.inum = rootino;
	strncpy(de.name, ".", DIRSIZ);
	iappend(rootino, &de, sizeof(de));

	de.inum = rootino;
	strncpy(de.name, "..", DIRSIZ);
	iappend(rootino, &de, sizeof(de));

	for (int fi = file_start; fi < argc; fi++) {
		char *arg = argv[fi];

		if (strcmp(arg, "-m") == 0) {
			if (fi + 1 >= argc) {
				usage(argv[0]);
			}
			char *maparg = argv[++fi];
			char *colon = strchr(maparg, ':');
			if (colon == NULL) {
				fprintf(stderr, "mkfs: -m requires source:target format\n");
				exit(1);
			}
			if (dir_map_count >= MAX_MAPS) {
				fprintf(stderr, "mkfs: too many -m mappings (max %d)\n", MAX_MAPS);
				exit(1);
			}
			*colon = '\0';
			dir_maps[dir_map_count].src = maparg;
			dir_maps[dir_map_count].dst = colon + 1;
			dir_map_count++;
			continue;
		}

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
			create_dir(parent, name);
			ndirs++;
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
			create_device(parent, name, type, major, minor);
			ndevs++;
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

		nfiles++;
		close(fd);
	}

	load_mkfsignore();
	for (int mi = 0; mi < dir_map_count; mi++) {
		char imgpath[512];
		if (dir_maps[mi].dst[0] == '/') {
			snprintf(imgpath, sizeof(imgpath), "%s", dir_maps[mi].dst);
		} else {
			snprintf(imgpath, sizeof(imgpath), "/%s", dir_maps[mi].dst);
		}
		sync_source_dir(dir_maps[mi].src, imgpath);
	}

	struct dinode din;
	rinode(rootino, &din);
	uint32_t off = din.size;
	off = ((off / BSIZE) + 1) * BSIZE;
	din.size = off;
	winode(rootino, &din);

	balloc(freeblock);

	close(fsfd);
	printf("mkfs: wrote %d files, %d dirs, %d devices (%d/%d inodes, %d/%d blocks used)\n",
	       nfiles,
	       ndirs,
	       ndevs,
	       freeinode - 1,
	       sb.ninodes,
	       freeblock,
	       sb.size);

	return 0;
}
