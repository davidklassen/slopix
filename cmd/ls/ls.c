#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int digit_count(long long n) {
	if (n == 0)
		return 1;
	int count = 0;
	while (n > 0) {
		count++;
		n /= 10;
	}
	return count;
}

static void ls(const char *path) {
	struct stat st;
	if (stat(path, &st) < 0) {
		printf("ls: cannot stat %s\n", path);
		return;
	}

	if (!S_ISDIR(st.st_mode)) {
		char type = S_ISREG(st.st_mode) ? '-' : '?';
		printf("%c %lld %s\n", type, (long long)st.st_size, path);
		return;
	}

	// First pass: find max size for column width
	DIR *d = opendir(path);
	if (d == 0) {
		printf("ls: cannot open %s\n", path);
		return;
	}

	char fullpath[256];
	struct dirent *ent;
	long long max_size = 0;

	while ((ent = readdir(d)) != 0) {
		if (strcmp(path, "/") == 0)
			snprintf(fullpath, sizeof(fullpath), "/%s", ent->d_name);
		else
			snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);

		struct stat entst;
		if (stat(fullpath, &entst) < 0)
			continue;

		if (entst.st_size > max_size)
			max_size = entst.st_size;
	}

	closedir(d);

	// Second pass: print entries with proper alignment
	d = opendir(path);
	if (d == 0)
		return;

	int width = digit_count(max_size);

	while ((ent = readdir(d)) != 0) {
		if (strcmp(path, "/") == 0)
			snprintf(fullpath, sizeof(fullpath), "/%s", ent->d_name);
		else
			snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);

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

		int padding = width - digit_count(entst.st_size);
		printf("%c ", type);
		while (padding-- > 0)
			printf(" ");
		printf("%lld %s\n", (long long)entst.st_size, ent->d_name);
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
