#ifdef RUN_TESTS

#include "test.h"
#include "elf.h"
#include "pmm.h"
#include "vmm.h"
#include "string.h"

// Minimal valid AArch64 ELF64: header (64B) + 1 PT_LOAD phdr (56B) + 4B data
// Segment: vaddr=0x1000, filesz=4, memsz=4, offset=120 (64+56), flags=R|X
static const unsigned char test_elf[] = {
    // ELF header (64 bytes)
    0x7f,
    'E',
    'L',
    'F', // e_ident[0..3]: magic
    2,	 // e_ident[4]: ELFCLASS64
    1,	 // e_ident[5]: ELFDATA2LSB
    1,	 // e_ident[6]: EV_CURRENT
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // e_ident[7..15]: padding
    2,
    0, // e_type: ET_EXEC
    0xb7,
    0, // e_machine: EM_AARCH64 (183)
    1,
    0,
    0,
    0, // e_version: EV_CURRENT
    0x00,
    0x10,
    0,
    0,
    0,
    0,
    0,
    0, // e_entry: 0x1000
    0x40,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // e_phoff: 64 (right after header)
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // e_shoff: 0
    0,
    0,
    0,
    0, // e_flags: 0
    0x40,
    0, // e_ehsize: 64
    0x38,
    0, // e_phentsize: 56
    1,
    0, // e_phnum: 1
    0,
    0, // e_shentsize: 0
    0,
    0, // e_shnum: 0
    0,
    0, // e_shstrndx: 0

    // Program header (56 bytes)
    1,
    0,
    0,
    0, // p_type: PT_LOAD
    5,
    0,
    0,
    0, // p_flags: PF_R | PF_X
    0x78,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // p_offset: 120 (64+56)
    0x00,
    0x10,
    0,
    0,
    0,
    0,
    0,
    0, // p_vaddr: 0x1000
    0x00,
    0x10,
    0,
    0,
    0,
    0,
    0,
    0, // p_paddr: 0x1000
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // p_filesz: 4
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // p_memsz: 4
    0,
    0x10,
    0,
    0,
    0,
    0,
    0,
    0, // p_align: 0x1000

    // Segment data (4 bytes)
    0xDE,
    0xAD,
    0xBE,
    0xEF,
};

TEST(elf_load_basic) {
	pte_t *pt = vmm_create();
	ASSERT_NOT_NULL(pt, "vmm_create");

	unsigned long entry, brk;
	int r = elf_load((const char *)test_elf, sizeof(test_elf), pt, &entry, &brk);
	ASSERT_EQ(r, 0, "elf_load");
	ASSERT_EQ(entry, 0x1000, "entry");
	ASSERT_EQ(brk, 0x2000, "brk page-aligned");

	// Verify mapped data by walking the page table
	paddr_t pa;
	int ur = vmm_unmap_page(pt, 0x1000, &pa);
	ASSERT_EQ(ur, 0, "unmap");

	unsigned char *mapped = (unsigned char *)PA_TO_VA(pa);
	ASSERT_EQ(mapped[0], 0xDE, "byte 0");
	ASSERT_EQ(mapped[1], 0xAD, "byte 1");
	ASSERT_EQ(mapped[2], 0xBE, "byte 2");
	ASSERT_EQ(mapped[3], 0xEF, "byte 3");

	pmm_free(pa);
	vmm_free(pt);
	return 0;
}

TEST(elf_load_bad_magic) {
	unsigned char buf[sizeof(test_elf)];
	memcpy(buf, test_elf, sizeof(test_elf));
	buf[0] = 0x00;

	pte_t *pt = vmm_create();
	ASSERT_NOT_NULL(pt, "vmm_create");

	unsigned long entry, brk;
	int r = elf_load((const char *)buf, sizeof(buf), pt, &entry, &brk);
	ASSERT_EQ(r, -1, "bad magic");

	vmm_free(pt);
	return 0;
}

TEST(elf_load_bad_machine) {
	unsigned char buf[sizeof(test_elf)];
	memcpy(buf, test_elf, sizeof(test_elf));
	buf[18] = 0;
	buf[19] = 0;

	pte_t *pt = vmm_create();
	ASSERT_NOT_NULL(pt, "vmm_create");

	unsigned long entry, brk;
	int r = elf_load((const char *)buf, sizeof(buf), pt, &entry, &brk);
	ASSERT_EQ(r, -1, "bad machine");

	vmm_free(pt);
	return 0;
}

TEST(elf_load_too_small) {
	pte_t *pt = vmm_create();
	ASSERT_NOT_NULL(pt, "vmm_create");

	unsigned long entry, brk;
	int r = elf_load((const char *)test_elf, 32, pt, &entry, &brk);
	ASSERT_EQ(r, -1, "too small");

	vmm_free(pt);
	return 0;
}

TEST(elf_load_no_leak) {
	unsigned long before = pmm_free_count();

	pte_t *pt = vmm_create();
	ASSERT_NOT_NULL(pt, "vmm_create");

	unsigned long entry, brk;
	int r = elf_load((const char *)test_elf, sizeof(test_elf), pt, &entry, &brk);
	ASSERT_EQ(r, 0, "elf_load");

	vmm_free(pt);

	unsigned long after = pmm_free_count();
	ASSERT_EQ(after, before, "no pages leaked");

	return 0;
}

TEST_SUITE(elf) {
	RUN_TEST(elf_load_basic);
	RUN_TEST(elf_load_bad_magic);
	RUN_TEST(elf_load_bad_machine);
	RUN_TEST(elf_load_too_small);
	RUN_TEST(elf_load_no_leak);
}

#endif
