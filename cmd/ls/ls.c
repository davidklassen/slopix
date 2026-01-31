#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void ls(const char *path) {
	struct stat st;
	if (stat(path, &st) < 0) {
		printf("ls: cannot stat %s\n", path);
		return;
	}

	if (!S_ISDIR(st.st_mode)) {
		char type = S_ISREG(st.st_mode) ? '-' : '?';
		printf("%c %d %s\n", type, st.st_size, path);
		return;
	}

	DIR *d = opendir(path);
	if (d == 0) {
		printf("ls: cannot open %s\n", path);
		return;
	}

	char fullpath[256];
	struct dirent *ent;
	while ((ent = readdir(d)) != 0) {
		if (strcmp(path, "/") == 0) {
			fullpath[0] = '/';
			strcpy(fullpath + 1, ent->d_name);
		} else {
			strcpy(fullpath, path);
			int len = strlen(fullpath);
			fullpath[len] = '/';
			strcpy(fullpath + len + 1, ent->d_name);
		}

		struct stat entst;
		if (stat(fullpath, &entst) < 0) {
			printf("ls: cannot stat %s\n", fullpath);
			continue;
		}

		char type;
		if (S_ISREG(entst.st_mode)) {
			type = '-';
		} else if (S_ISDIR(entst.st_mode)) {
			type = 'd';
		} else if (S_ISCHR(entst.st_mode)) {
			type = 'c';
		} else if (S_ISBLK(entst.st_mode)) {
			type = 'b';
		} else {
			type = '?';
		}

		printf("%c %d %s\n", type, entst.st_size, ent->d_name);
	}

	closedir(d);
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
