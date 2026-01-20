#include "initramfs.h"

extern char _initramfs_start[];
extern char _initramfs_end[];

static int streq(const char *a, const char *b, unsigned int len) {
	for (unsigned int i = 0; i < len; i++) {
		if (a[i] != b[i]) {
			return 0;
		}
	}
	return 1;
}

static unsigned int strlen(const char *s) {
	unsigned int n = 0;
	while (s[n]) {
		n++;
	}
	return n;
}

static unsigned int align4(unsigned int n) {
	return (n + 3) & ~3;
}

int initramfs_find(const char *name, struct initramfs_entry *entry) {
	if (&_initramfs_start[0] == &_initramfs_end[0]) {
		return -1;
	}

	struct initramfs_header *hdr = (struct initramfs_header *)_initramfs_start;
	if (hdr->magic != INITRAMFS_MAGIC) {
		return -1;
	}

	unsigned int name_len = strlen(name);
	const char *ptr = _initramfs_start + sizeof(struct initramfs_header);

	for (unsigned int i = 0; i < hdr->count; i++) {
		struct initramfs_file *file = (struct initramfs_file *)ptr;
		const char *file_name = ptr + sizeof(struct initramfs_file);
		const char *file_data = file_name + align4(file->name_len);

		if (file->name_len == name_len && streq(name, file_name, name_len)) {
			entry->name = file_name;
			entry->data = file_data;
			entry->size = file->data_len;
			return 0;
		}

		ptr = file_data + align4(file->data_len);
	}

	return -1;
}
