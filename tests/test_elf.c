#ifdef RUN_TESTS

#include "test.h"
#include "elf.h"
#include "vmm.h"
#include "pmm.h"

// Minimal synthetic ELF: header + 1 program header + 8 bytes of code
// Total: 64 + 56 + 8 = 128 bytes
static const unsigned char minimal_elf[] = {
    // ELF header (64 bytes)
    0x7f,
    0x45,
    0x4c,
    0x46, // e_ident[0-3]: magic
    0x02, // e_ident[4]: class (64-bit)
    0x01, // e_ident[5]: data (little endian)
    0x01, // e_ident[6]: version
    0x00, // e_ident[7]: OS/ABI
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // e_ident[8-15]: padding
    0x02,
    0x00, // e_type: ET_EXEC
    0xb7,
    0x00, // e_machine: EM_AARCH64 (183)
    0x01,
    0x00,
    0x00,
    0x00, // e_version: 1
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // e_entry: 0x10000
    0x40,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // e_phoff: 64
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // e_shoff: 0
    0x00,
    0x00,
    0x00,
    0x00, // e_flags: 0
    0x40,
    0x00, // e_ehsize: 64
    0x38,
    0x00, // e_phentsize: 56
    0x01,
    0x00, // e_phnum: 1
    0x00,
    0x00, // e_shentsize: 0
    0x00,
    0x00, // e_shnum: 0
    0x00,
    0x00, // e_shstrndx: 0

    // Program header (56 bytes)
    0x01,
    0x00,
    0x00,
    0x00, // p_type: PT_LOAD
    0x05,
    0x00,
    0x00,
    0x00, // p_flags: PF_R | PF_X
    0x78,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // p_offset: 120
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // p_vaddr: 0x10000
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // p_paddr: 0x10000
    0x08,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // p_filesz: 8
    0x08,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // p_memsz: 8
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // p_align: 0x1000

    // Code (8 bytes): two NOP instructions
    0x1f,
    0x20,
    0x03,
    0xd5, // nop
    0x1f,
    0x20,
    0x03,
    0xd5, // nop
};

TEST(elf_load_valid_elf) {
	pte_t *pt = vmm_create();
	unsigned long entry = 0;

	int ret = elf_load((const char *)minimal_elf, sizeof(minimal_elf), pt, &entry);

	ASSERT_EQ(ret, 0, "elf_load should succeed");
	ASSERT_EQ(entry, 0x10000, "entry point should be 0x10000");

	vmm_free(pt);
	return 0;
}

TEST(elf_load_invalid_magic) {
	unsigned char bad_elf[] = {0x00, 0x00, 0x00, 0x00};
	pte_t *pt = vmm_create();
	unsigned long entry = 0;

	int ret = elf_load((const char *)bad_elf, sizeof(bad_elf), pt, &entry);

	ASSERT_EQ(ret, -1, "should reject invalid magic");

	vmm_free(pt);
	return 0;
}

TEST(elf_load_wrong_machine) {
	unsigned char wrong_machine[128];
	for (unsigned long i = 0; i < sizeof(wrong_machine); i++) {
		wrong_machine[i] = minimal_elf[i];
	}
	// Change e_machine to x86_64 (0x3E)
	wrong_machine[18] = 0x3e;
	wrong_machine[19] = 0x00;

	pte_t *pt = vmm_create();
	unsigned long entry = 0;

	int ret = elf_load((const char *)wrong_machine, sizeof(wrong_machine), pt, &entry);

	ASSERT_EQ(ret, -1, "should reject wrong machine type");

	vmm_free(pt);
	return 0;
}

TEST(elf_load_truncated) {
	pte_t *pt = vmm_create();
	unsigned long entry = 0;

	// Only pass 32 bytes (less than ELF header size)
	int ret = elf_load((const char *)minimal_elf, 32, pt, &entry);

	ASSERT_EQ(ret, -1, "should reject truncated ELF");

	vmm_free(pt);
	return 0;
}

TEST(elf_load_maps_page) {
	pte_t *pt = vmm_create();
	unsigned long entry = 0;

	elf_load((const char *)minimal_elf, sizeof(minimal_elf), pt, &entry);

	// Walk page table to verify 0x10000 is mapped
	pte_t *l0 = pt;
	int l0_idx = (0x10000UL >> 39) & 0x1FF;
	pte_t *l1 = (pte_t *)PA_TO_VA(l0[l0_idx] & ~0xFFFUL);
	int l1_idx = (0x10000UL >> 30) & 0x1FF;
	pte_t *l2 = (pte_t *)PA_TO_VA(l1[l1_idx] & ~0xFFFUL);
	int l2_idx = (0x10000UL >> 21) & 0x1FF;
	pte_t *l3 = (pte_t *)PA_TO_VA(l2[l2_idx] & ~0xFFFUL);
	int l3_idx = (0x10000UL >> 12) & 0x1FF;

	ASSERT_NE(l3[l3_idx], 0, "page should be mapped");

	vmm_free(pt);
	return 0;
}

TEST_SUITE(elf) {
	RUN_TEST(elf_load_valid_elf);
	RUN_TEST(elf_load_invalid_magic);
	RUN_TEST(elf_load_wrong_machine);
	RUN_TEST(elf_load_truncated);
	RUN_TEST(elf_load_maps_page);
}

#endif
