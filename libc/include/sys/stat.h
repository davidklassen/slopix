#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <time.h>

typedef long off_t;
typedef unsigned short mode_t;

struct stat {
	unsigned int st_dev;
	unsigned int st_ino;
	unsigned short st_mode;
	unsigned short st_nlink;
	unsigned int st_size;
	time_t st_mtime;
};

int fstat(int fd, struct stat *st);
int stat(const char *path, struct stat *st);

#define S_IFMT   0170000
#define S_IFREG  0100000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFBLK  0060000

#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)

#endif
