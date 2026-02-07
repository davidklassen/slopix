#include "test.h"
#include "dtb.h"
#include "string.h"

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE   0x00000002
#define FDT_PROP       0x00000003
#define FDT_NOP	       0x00000004
#define FDT_END	       0x00000009

#define FDT_BUF_SIZE	256
#define FDT_STRINGS_OFF 192

struct fdt_builder {
	unsigned char buf[FDT_BUF_SIZE];
	unsigned int spos;
	unsigned int slen;
};

static unsigned int cpu_to_be32(unsigned int v) {
	return ((v & 0xff) << 24) | ((v & 0xff00) << 8) |
	       ((v & 0xff0000) >> 8) | ((v & 0xff000000) >> 24);
}

static void fdt_init(struct fdt_builder *f) {
	memset(f->buf, 0, FDT_BUF_SIZE);
	f->spos = 40;
	f->slen = 0;
	unsigned int *hdr = (unsigned int *)f->buf;
	hdr[0] = cpu_to_be32(FDT_MAGIC);
}

static void fdt_put32(struct fdt_builder *f, unsigned int val) {
	*(unsigned int *)(f->buf + f->spos) = cpu_to_be32(val);
	f->spos += 4;
}

static void fdt_begin_node(struct fdt_builder *f, const char *name) {
	fdt_put32(f, FDT_BEGIN_NODE);
	unsigned int len = strlen(name) + 1;
	memcpy(f->buf + f->spos, name, len);
	f->spos += (len + 3) & ~3U;
}

static void fdt_end_node(struct fdt_builder *f) {
	fdt_put32(f, FDT_END_NODE);
}

static unsigned int fdt_add_string(struct fdt_builder *f, const char *s) {
	unsigned int off = f->slen;
	unsigned int len = strlen(s) + 1;
	memcpy(f->buf + FDT_STRINGS_OFF + f->slen, s, len);
	f->slen += len;
	return off;
}

static void fdt_prop_str(struct fdt_builder *f, const char *name, const char *val) {
	unsigned int nameoff = fdt_add_string(f, name);
	unsigned int len = strlen(val) + 1;
	fdt_put32(f, FDT_PROP);
	fdt_put32(f, len);
	fdt_put32(f, nameoff);
	memcpy(f->buf + f->spos, val, len);
	f->spos += (len + 3) & ~3U;
}

static void fdt_prop_u32(struct fdt_builder *f, const char *name, unsigned int val) {
	unsigned int nameoff = fdt_add_string(f, name);
	fdt_put32(f, FDT_PROP);
	fdt_put32(f, 4);
	fdt_put32(f, nameoff);
	fdt_put32(f, val);
}

static void fdt_prop_u64(struct fdt_builder *f, const char *name, unsigned long val) {
	unsigned int nameoff = fdt_add_string(f, name);
	fdt_put32(f, FDT_PROP);
	fdt_put32(f, 8);
	fdt_put32(f, nameoff);
	fdt_put32(f, (unsigned int)(val >> 32));
	fdt_put32(f, (unsigned int)(val & 0xFFFFFFFF));
}

static void *fdt_finish(struct fdt_builder *f) {
	fdt_put32(f, FDT_END);
	unsigned int *hdr = (unsigned int *)f->buf;
	hdr[2] = cpu_to_be32(40);
	hdr[3] = cpu_to_be32(FDT_STRINGS_OFF);
	return f->buf;
}

static void dtb_restore(void) {
	extern unsigned long _dtb_address;
	dtb_init((void *)_dtb_address);
}

TEST(dtb_null_safe) {
	dtb_init(0);
	ASSERT_NULL(dtb_get_bootargs(), "bootargs should be NULL after null init");
	ASSERT_EQ(dtb_get_initrd_start(), 0, "initrd_start should be 0");
	ASSERT_EQ(dtb_get_initrd_end(), 0, "initrd_end should be 0");
	dtb_restore();
	return 0;
}

TEST(dtb_invalid_magic) {
	unsigned int buf[16];
	memset(buf, 0, sizeof(buf));
	buf[0] = cpu_to_be32(0xdeadbeef);
	dtb_init(buf);
	ASSERT_NULL(dtb_get_bootargs(), "bootargs should be NULL for bad magic");
	ASSERT_EQ(dtb_get_initrd_start(), 0, "initrd_start should be 0");
	ASSERT_EQ(dtb_get_initrd_end(), 0, "initrd_end should be 0");
	dtb_restore();
	return 0;
}

TEST(dtb_bootargs) {
	struct fdt_builder b;
	fdt_init(&b);
	fdt_begin_node(&b, "");
	fdt_begin_node(&b, "chosen");
	fdt_prop_str(&b, "bootargs", "console=ttyS0 root=/dev/vda");
	fdt_end_node(&b);
	fdt_end_node(&b);
	void *dtb = fdt_finish(&b);

	dtb_init(dtb);
	ASSERT_STREQ(dtb_get_bootargs(), "console=ttyS0 root=/dev/vda");
	dtb_restore();
	return 0;
}

TEST(dtb_no_chosen) {
	struct fdt_builder b;
	fdt_init(&b);
	fdt_begin_node(&b, "");
	fdt_begin_node(&b, "memory");
	fdt_end_node(&b);
	fdt_end_node(&b);
	void *dtb = fdt_finish(&b);

	dtb_init(dtb);
	ASSERT_NULL(dtb_get_bootargs(), "bootargs should be NULL without chosen");
	dtb_restore();
	return 0;
}

TEST(dtb_initrd_32bit) {
	struct fdt_builder b;
	fdt_init(&b);
	fdt_begin_node(&b, "");
	fdt_begin_node(&b, "chosen");
	fdt_prop_u32(&b, "linux,initrd-start", 0x48000000);
	fdt_prop_u32(&b, "linux,initrd-end", 0x48100000);
	fdt_end_node(&b);
	fdt_end_node(&b);
	void *dtb = fdt_finish(&b);

	dtb_init(dtb);
	ASSERT_EQ(dtb_get_initrd_start(), 0x48000000, "initrd start 32-bit");
	ASSERT_EQ(dtb_get_initrd_end(), 0x48100000, "initrd end 32-bit");
	dtb_restore();
	return 0;
}

TEST(dtb_initrd_64bit) {
	struct fdt_builder b;
	fdt_init(&b);
	fdt_begin_node(&b, "");
	fdt_begin_node(&b, "chosen");
	fdt_prop_u64(&b, "linux,initrd-start", 0x0000004800000000UL);
	fdt_prop_u64(&b, "linux,initrd-end", 0x0000004800100000UL);
	fdt_end_node(&b);
	fdt_end_node(&b);
	void *dtb = fdt_finish(&b);

	dtb_init(dtb);
	ASSERT_EQ(dtb_get_initrd_start(), 0x0000004800000000UL, "initrd start 64-bit");
	ASSERT_EQ(dtb_get_initrd_end(), 0x0000004800100000UL, "initrd end 64-bit");
	dtb_restore();
	return 0;
}

TEST_SUITE(dtb) {
	RUN_TEST(dtb_null_safe);
	RUN_TEST(dtb_invalid_magic);
	RUN_TEST(dtb_bootargs);
	RUN_TEST(dtb_no_chosen);
	RUN_TEST(dtb_initrd_32bit);
	RUN_TEST(dtb_initrd_64bit);
}
