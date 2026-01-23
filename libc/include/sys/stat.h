#ifndef _SYS_STAT_H
#define _SYS_STAT_H

struct stat {
	unsigned int dev;
	unsigned int ino;
	unsigned short type;
	unsigned short nlink;
	unsigned int size;
};

int fstat(int fd, struct stat *st);
int stat(const char *path, struct stat *st);

#endif
