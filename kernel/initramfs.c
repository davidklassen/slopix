#include "initramfs.h"
#include "dtb.h"
#include "board.h"
#include "string.h"

static const char *initramfs_data;
static unsigned long initramfs_size;

static unsigned int align4(unsigned int n) {
	return (n + 3) & ~3;
}

void initramfs_init(void) {
	unsigned long pa_start = dtb_get_initrd_start();
	unsigned long pa_end = dtb_get_initrd_end();

	if (pa_start == 0 || pa_end == 0 || pa_end <= pa_start) {
		initramfs_data = 0;
		initramfs_size = 0;
		return;
	}

	initramfs_data = (const char *)PA_TO_VA(pa_start);
	initramfs_size = pa_end - pa_start;
}

int initramfs_find(const char *name, struct initramfs_entry *entry) {
	if (initramfs_data == 0 || initramfs_size == 0) {
		return -1;
	}

	struct initramfs_header *hdr = (struct initramfs_header *)initramfs_data;
	if (hdr->magic != INITRAMFS_MAGIC) {
		return -1;
	}

	unsigned int name_len = strlen(name);
	const char *ptr = initramfs_data + sizeof(struct initramfs_header);

	for (unsigned int i = 0; i < hdr->count; i++) {
		struct initramfs_file *file = (struct initramfs_file *)ptr;
		const char *file_name = ptr + sizeof(struct initramfs_file);
		const char *file_data = file_name + align4(file->name_len);

		if (file->name_len == name_len && strncmp(name, file_name, name_len) == 0) {
			entry->name = file_name;
			entry->data = file_data;
			entry->size = file->data_len;
			return 0;
		}

		ptr = file_data + align4(file->data_len);
	}

	return -1;
}
