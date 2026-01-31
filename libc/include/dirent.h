#ifndef _DIRENT_H
#define _DIRENT_H

#define NAME_MAX 255

struct dirent {
	unsigned long d_ino;
	char d_name[NAME_MAX + 1];
};

typedef struct {
	int fd;
	char buf[1024];
	int pos;
	int end;
} DIR;

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);

#endif
