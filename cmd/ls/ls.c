#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DIRSIZ	  14
#define T_FILE	  1
#define T_DIR	  2
#define T_DEVICE  3
#define T_BDEVICE 4

struct dirent {
	unsigned short inum;
	char name[DIRSIZ];
};

static void ls(const char *path) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf("ls: cannot open %s\n", path);
		return;
	}

	struct stat st;
	if (fstat(fd, &st) < 0) {
		printf("ls: cannot stat %s\n", path);
		close(fd);
		return;
	}

	if (st.st_mode != T_DIR) {
		char type = (st.st_mode == T_FILE) ? '-' : '?';
		printf("%c %d %s\n", type, st.st_size, path);
		close(fd);
		return;
	}

	struct dirent de;
	char fullpath[128];
	char name[DIRSIZ + 1];

	while (read(fd, &de, sizeof(de)) == sizeof(de)) {
		if (de.inum == 0) {
			continue;
		}

		strncpy(name, de.name, DIRSIZ);
		name[DIRSIZ] = '\0';

		if (strcmp(path, "/") == 0) {
			fullpath[0] = '/';
			strcpy(fullpath + 1, name);
		} else {
			strcpy(fullpath, path);
			int len = strlen(fullpath);
			fullpath[len] = '/';
			strcpy(fullpath + len + 1, name);
		}

		struct stat entst;
		if (stat(fullpath, &entst) < 0) {
			printf("ls: cannot stat %s\n", fullpath);
			continue;
		}

		char type;
		switch (entst.st_mode) {
		case T_FILE:
			type = '-';
			break;
		case T_DIR:
			type = 'd';
			break;
		case T_DEVICE:
			type = 'c';
			break;
		case T_BDEVICE:
			type = 'b';
			break;
		default:
			type = '?';
			break;
		}

		printf("%c %d %s\n", type, entst.st_size, name);
	}

	close(fd);
}

int main(int argc, char **argv) {
	if (argc <= 1) {
		ls(".");
	} else {
		for (int i = 1; i < argc; i++) {
			if (argc > 2) {
				printf("%s:\n", argv[i]);
			}
			ls(argv[i]);
		}
	}
	return 0;
}
