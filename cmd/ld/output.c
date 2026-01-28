#include "ld.h"

#include <fcntl.h>
#include <unistd.h>

static bool write_all(int fd, const void *buf, size_t n) {
	const uint8_t *p = buf;
	while (n > 0) {
		ssize_t w = write(fd, p, n);
		if (w <= 0) {
			return false;
		}
		p += w;
		n -= w;
	}
	return true;
}

static bool write_zeros(int fd, size_t n) {
	static const uint8_t zeros[512] = {0};
	while (n > 0) {
		size_t chunk = n < sizeof(zeros) ? n : sizeof(zeros);
		if (!write_all(fd, zeros, chunk)) {
			return false;
		}
		n -= chunk;
	}
	return true;
}

static uint64_t align_up(uint64_t val, uint64_t align) {
	return (val + align - 1) & ~(align - 1);
}

bool write_executable(const char *path, OutputSection *sections, SymbolTable *global, const char *entry_point) {
	Symbol *start = symbol_lookup(global, entry_point);
	if (!start) {
		fprintf(stderr, "ld: undefined entry point: %s\n", entry_point);
		return false;
	}

	OutputSection *text = &sections[OUT_TEXT];
	OutputSection *rodata = &sections[OUT_RODATA];
	OutputSection *data = &sections[OUT_DATA];
	OutputSection *bss = &sections[OUT_BSS];

	uint64_t ehdr_size = sizeof(Elf64_Ehdr);
	uint64_t phdr_size = sizeof(Elf64_Phdr);
	uint64_t phnum = 2;
	uint64_t headers_end = ehdr_size + phnum * phdr_size;

	uint64_t code_file_offset = PAGE_SIZE;
	uint64_t code_size = text->size + rodata->size;
	uint64_t code_vaddr = text->addr;

	uint64_t data_file_offset = align_up(code_file_offset + code_size, PAGE_SIZE);
	uint64_t data_filesz = data->size;
	uint64_t data_memsz = data->size + bss->size;
	uint64_t data_vaddr = data->addr;

	if (data->size == 0 && bss->size > 0) {
		data_vaddr = bss->addr;
	}

	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0755);
	if (fd < 0) {
		fprintf(stderr, "ld: cannot create %s\n", path);
		return false;
	}

	Elf64_Ehdr ehdr = {
	    .e_ident = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS64, ELFDATA2LSB, EV_CURRENT, 0},
	    .e_type = ET_EXEC,
	    .e_machine = EM_AARCH64,
	    .e_version = EV_CURRENT,
	    .e_entry = start->value,
	    .e_phoff = ehdr_size,
	    .e_shoff = 0,
	    .e_flags = 0,
	    .e_ehsize = ehdr_size,
	    .e_phentsize = phdr_size,
	    .e_phnum = phnum,
	    .e_shentsize = 0,
	    .e_shnum = 0,
	    .e_shstrndx = 0,
	};

	Elf64_Phdr code_phdr = {
	    .p_type = PT_LOAD,
	    .p_flags = PF_R | PF_X,
	    .p_offset = code_file_offset,
	    .p_vaddr = code_vaddr,
	    .p_paddr = code_vaddr,
	    .p_filesz = code_size,
	    .p_memsz = code_size,
	    .p_align = PAGE_SIZE,
	};

	Elf64_Phdr data_phdr = {
	    .p_type = PT_LOAD,
	    .p_flags = PF_R | PF_W,
	    .p_offset = data_file_offset,
	    .p_vaddr = data_vaddr,
	    .p_paddr = data_vaddr,
	    .p_filesz = data_filesz,
	    .p_memsz = data_memsz,
	    .p_align = PAGE_SIZE,
	};

	if (!write_all(fd, &ehdr, ehdr_size)) {
		goto fail;
	}
	if (!write_all(fd, &code_phdr, phdr_size)) {
		goto fail;
	}
	if (!write_all(fd, &data_phdr, phdr_size)) {
		goto fail;
	}

	uint64_t padding = code_file_offset - headers_end;
	if (!write_zeros(fd, padding)) {
		goto fail;
	}

	if (text->size > 0 && !write_all(fd, text->data, text->size)) {
		goto fail;
	}
	if (rodata->size > 0 && !write_all(fd, rodata->data, rodata->size)) {
		goto fail;
	}

	uint64_t current = code_file_offset + code_size;
	padding = data_file_offset - current;
	if (!write_zeros(fd, padding)) {
		goto fail;
	}

	if (data->size > 0 && !write_all(fd, data->data, data->size)) {
		goto fail;
	}

	close(fd);
	return true;

fail:
	fprintf(stderr, "ld: write error: %s\n", path);
	close(fd);
	return false;
}
