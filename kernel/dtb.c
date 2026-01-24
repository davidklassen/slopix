#include "dtb.h"
#include "string.h"

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE   0x00000002
#define FDT_PROP       0x00000003
#define FDT_NOP	       0x00000004
#define FDT_END	       0x00000009

static const char *bootargs;
static unsigned long initrd_start;
static unsigned long initrd_end;

static unsigned int be32_to_cpu(unsigned int be) {
	return ((be & 0xff) << 24) | ((be & 0xff00) << 8) | ((be & 0xff0000) >> 8) |
	       ((be & 0xff000000) >> 24);
}

static unsigned int align4(unsigned int val) {
	return (val + 3) & ~3U;
}

void dtb_init(void *dtb_addr) {
	bootargs = 0;
	initrd_start = 0;
	initrd_end = 0;

	if (dtb_addr == 0) {
		return;
	}

	unsigned int *hdr = (unsigned int *)dtb_addr;
	unsigned int magic = be32_to_cpu(hdr[0]);
	if (magic != FDT_MAGIC) {
		return;
	}

	unsigned int off_dt_struct = be32_to_cpu(hdr[2]);
	unsigned int off_dt_strings = be32_to_cpu(hdr[3]);

	char *strings = (char *)dtb_addr + off_dt_strings;
	unsigned int *p = (unsigned int *)((char *)dtb_addr + off_dt_struct);

	int in_chosen = 0;

	while (1) {
		unsigned int token = be32_to_cpu(*p++);

		if (token == FDT_BEGIN_NODE) {
			char *name = (char *)p;
			unsigned int namelen = strlen(name);
			p = (unsigned int *)((char *)p + align4(namelen + 1));

			if (strcmp(name, "chosen") == 0) {
				in_chosen = 1;
			}
		} else if (token == FDT_END_NODE) {
			in_chosen = 0;
		} else if (token == FDT_PROP) {
			unsigned int len = be32_to_cpu(*p++);
			unsigned int nameoff = be32_to_cpu(*p++);
			char *propname = strings + nameoff;
			char *propdata = (char *)p;

			if (in_chosen && strcmp(propname, "bootargs") == 0) {
				bootargs = propdata;
			}
			if (in_chosen && strcmp(propname, "linux,initrd-start") == 0) {
				if (len == 8) {
					unsigned int hi = be32_to_cpu(*(unsigned int *)propdata);
					unsigned int lo = be32_to_cpu(*(unsigned int *)(propdata + 4));
					initrd_start = ((unsigned long)hi << 32) | lo;
				} else {
					initrd_start = be32_to_cpu(*(unsigned int *)propdata);
				}
			}
			if (in_chosen && strcmp(propname, "linux,initrd-end") == 0) {
				if (len == 8) {
					unsigned int hi = be32_to_cpu(*(unsigned int *)propdata);
					unsigned int lo = be32_to_cpu(*(unsigned int *)(propdata + 4));
					initrd_end = ((unsigned long)hi << 32) | lo;
				} else {
					initrd_end = be32_to_cpu(*(unsigned int *)propdata);
				}
			}

			p = (unsigned int *)(propdata + align4(len));
		} else if (token == FDT_NOP) {
			continue;
		} else if (token == FDT_END) {
			break;
		} else {
			break;
		}
	}
}

const char *dtb_get_bootargs(void) {
	return bootargs;
}

unsigned long dtb_get_initrd_start(void) {
	return initrd_start;
}

unsigned long dtb_get_initrd_end(void) {
	return initrd_end;
}
