#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct entry {
	char name[NAME_MAX + 1];
	char type;
	long long size;
};

static int cmp_entry(const void *a, const void *b) {
	return strcmp(((const struct entry *)a)->name,
		      ((const struct entry *)b)->name);
}

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

	DIR *d = opendir(path);
	if (d == 0) {
		printf("ls: cannot open %s\n", path);
		return;
	}

	size_t cap = 16;
	size_t count = 0;
	struct entry *entries = malloc(cap * sizeof(struct entry));
	if (entries == 0) {
		closedir(d);
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

		if (count == cap) {
			cap *= 2;
			struct entry *tmp =
			    realloc(entries, cap * sizeof(struct entry));
			if (tmp == 0) {
				break;
			}
			entries = tmp;
		}

		strncpy(entries[count].name, ent->d_name, NAME_MAX);
		entries[count].name[NAME_MAX] = '\0';

		if (S_ISREG(entst.st_mode))
			entries[count].type = '-';
		else if (S_ISDIR(entst.st_mode))
			entries[count].type = 'd';
		else if (S_ISCHR(entst.st_mode))
			entries[count].type = 'c';
		else if (S_ISBLK(entst.st_mode))
			entries[count].type = 'b';
		else
			entries[count].type = '?';

		entries[count].size = (long long)entst.st_size;
		if (entries[count].size > max_size)
			max_size = entries[count].size;

		count++;
	}

	closedir(d);

	qsort(entries, count, sizeof(struct entry), cmp_entry);

	int width = digit_count(max_size);
	for (size_t i = 0; i < count; i++) {
		int padding = width - digit_count(entries[i].size);
		printf("%c ", entries[i].type);
		while (padding-- > 0)
			printf(" ");
		printf("%lld %s\n", entries[i].size, entries[i].name);
	}

	free(entries);
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
