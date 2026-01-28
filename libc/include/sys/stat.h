#ifndef _SYS_STAT_H
#define _SYS_STAT_H

typedef long off_t;

struct stat {
	unsigned int st_dev;
	unsigned int st_ino;
	unsigned short st_mode;
	unsigned short st_nlink;
	unsigned int st_size;
};

int fstat(int fd, struct stat *st);
int stat(const char *path, struct stat *st);

#endif
