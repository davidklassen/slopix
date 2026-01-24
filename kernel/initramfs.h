#ifndef INITRAMFS_H
#define INITRAMFS_H

#define INITRAMFS_MAGIC 0x4D415253

struct initramfs_header {
	unsigned int magic;
	unsigned int count;
};

struct initramfs_file {
	unsigned int name_len;
	unsigned int data_len;
};

struct initramfs_entry {
	const char *name;
	const char *data;
	unsigned int size;
};

void initramfs_init(void);
int initramfs_find(const char *name, struct initramfs_entry *entry);

#endif
