#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#define NDIR 4

struct linux_dirent {
	unsigned long d_ino;
	unsigned long d_off;
	unsigned short d_reclen;
	char d_name[];
};

extern int getdents(int fd, void *buf, unsigned int count);

static DIR dir_pool[NDIR];
static int dir_inuse[NDIR];

DIR *opendir(const char *name) {
	int fd = open(name, O_RDONLY);
	if (fd < 0) {
		return 0;
	}

	for (int i = 0; i < NDIR; i++) {
		if (!dir_inuse[i]) {
			dir_inuse[i] = 1;
			dir_pool[i].fd = fd;
			dir_pool[i].pos = 0;
			dir_pool[i].end = 0;
			return &dir_pool[i];
		}
	}

	close(fd);
	return 0;
}

struct dirent *readdir(DIR *dirp) {
	static struct dirent entry;

	if (dirp == 0) {
		return 0;
	}

	while (1) {
		if (dirp->pos >= dirp->end) {
			int n = getdents(dirp->fd, dirp->buf, sizeof(dirp->buf));
			if (n <= 0) {
				return 0;
			}
			dirp->pos = 0;
			dirp->end = n;
		}

		struct linux_dirent *ld = (struct linux_dirent *)(dirp->buf + dirp->pos);
		dirp->pos += ld->d_reclen;

		entry.d_ino = ld->d_ino;
		int i;
		for (i = 0; i < NAME_MAX && ld->d_name[i]; i++) {
			entry.d_name[i] = ld->d_name[i];
		}
		entry.d_name[i] = '\0';

		return &entry;
	}
}

int closedir(DIR *dirp) {
	if (dirp == 0) {
		return -1;
	}

	int fd = dirp->fd;

	for (int i = 0; i < NDIR; i++) {
		if (&dir_pool[i] == dirp) {
			dir_inuse[i] = 0;
			break;
		}
	}

	return close(fd);
}
