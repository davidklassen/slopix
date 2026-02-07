#include "test.h"
#include "vmm.h"
#include "pmm.h"
#include "string.h"

#define TEST_VA 0x200000

TEST(copyinstr_basic) {
	pte_t *pt = vmm_create();
	ASSERT_NOT_NULL(pt, "vmm_create");

	paddr_t pa = pmm_alloc();
	ASSERT(pa != PMM_INVALID, "pmm_alloc");

	int r = vmm_map_page(pt, TEST_VA, pa, 0, 0);
	ASSERT_EQ(r, 0, "vmm_map_page");

	char *src = (char *)PA_TO_VA(pa);
	memcpy(src, "hello", 6);

	char dst[64];
	int len = vmm_copyinstr(pt, dst, TEST_VA, sizeof(dst));
	ASSERT_EQ(len, 5, "length");
	ASSERT_STREQ(dst, "hello");

	vmm_free(pt);
	return 0;
}

TEST(copyinstr_cross_page) {
	pte_t *pt = vmm_create();
	ASSERT_NOT_NULL(pt, "vmm_create");

	paddr_t pa1 = pmm_alloc();
	ASSERT(pa1 != PMM_INVALID, "pmm_alloc pa1");

	paddr_t pa2 = pmm_alloc();
	ASSERT(pa2 != PMM_INVALID, "pmm_alloc pa2");

	int r = vmm_map_page(pt, TEST_VA, pa1, 0, 0);
	ASSERT_EQ(r, 0, "map page 1");

	r = vmm_map_page(pt, TEST_VA + PAGE_SIZE, pa2, 0, 0);
	ASSERT_EQ(r, 0, "map page 2");

	// Place "AB" spanning the page boundary: 'A' at last byte of page 1,
	// 'B' and '\0' at first bytes of page 2
	unsigned long off = PAGE_SIZE - 1;
	char *p1 = (char *)PA_TO_VA(pa1);
	char *p2 = (char *)PA_TO_VA(pa2);
	p1[off] = 'A';
	p2[0] = 'B';
	p2[1] = '\0';

	char dst[64];
	int len = vmm_copyinstr(pt, dst, TEST_VA + off, sizeof(dst));
	ASSERT_EQ(len, 2, "length");
	ASSERT_STREQ(dst, "AB");

	vmm_free(pt);
	return 0;
}

TEST(copyinstr_max_truncates) {
	pte_t *pt = vmm_create();
	ASSERT_NOT_NULL(pt, "vmm_create");

	paddr_t pa = pmm_alloc();
	ASSERT(pa != PMM_INVALID, "pmm_alloc");

	int r = vmm_map_page(pt, TEST_VA, pa, 0, 0);
	ASSERT_EQ(r, 0, "vmm_map_page");

	char *src = (char *)PA_TO_VA(pa);
	memcpy(src, "long string", 12);

	char dst[64];
	int len = vmm_copyinstr(pt, dst, TEST_VA, 5);
	ASSERT_EQ(len, 4, "truncated length");
	ASSERT_EQ(dst[4], '\0', "null terminated");
	ASSERT_STREQ(dst, "long");

	vmm_free(pt);
	return 0;
}

TEST(copyinstr_unmapped_fails) {
	pte_t *pt = vmm_create();
	ASSERT_NOT_NULL(pt, "vmm_create");

	char dst[64];
	int len = vmm_copyinstr(pt, dst, TEST_VA, sizeof(dst));
	ASSERT_EQ(len, -1, "unmapped returns -1");

	vmm_free(pt);
	return 0;
}

TEST_SUITE(copyinstr) {
	RUN_TEST(copyinstr_basic);
	RUN_TEST(copyinstr_cross_page);
	RUN_TEST(copyinstr_max_truncates);
	RUN_TEST(copyinstr_unmapped_fails);
}
