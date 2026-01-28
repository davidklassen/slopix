#include "ld.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static bool elf_check_header(Elf64_Ehdr *ehdr, const char *filename) {
	if (ehdr->e_ident[0] != ELFMAG0 || ehdr->e_ident[1] != ELFMAG1 ||
	    ehdr->e_ident[2] != ELFMAG2 || ehdr->e_ident[3] != ELFMAG3) {
		error("%s: not an ELF file", filename);
	}

	if (ehdr->e_ident[4] != ELFCLASS64) {
		error("%s: not a 64-bit ELF file", filename);
	}

	if (ehdr->e_ident[5] != ELFDATA2LSB) {
		error("%s: not a little-endian ELF file", filename);
	}

	if (ehdr->e_type != ET_REL) {
		error("%s: not a relocatable object file", filename);
	}

	if (ehdr->e_machine != EM_AARCH64) {
		error("%s: not an AArch64 object file", filename);
	}

	return true;
}

ObjectFile *elf_read(const char *path) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		error("cannot open %s: %s", path, strerror(errno));
	}

	struct stat st;
	if (fstat(fd, &st) < 0) {
		error("cannot stat %s: %s", path, strerror(errno));
	}

	if (st.st_size < (off_t)sizeof(Elf64_Ehdr)) {
		error("%s: file too small for ELF header", path);
	}

	void *data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (data == MAP_FAILED) {
		error("cannot mmap %s: %s", path, strerror(errno));
	}
	close(fd);

	ObjectFile *obj = calloc(1, sizeof(ObjectFile));
	obj->filename = strdup(path);
	obj->data = data;
	obj->size = st.st_size;
	obj->ehdr = (Elf64_Ehdr *)data;

	elf_check_header(obj->ehdr, path);

	obj->shnum = obj->ehdr->e_shnum;
	if (obj->ehdr->e_shoff + obj->shnum * sizeof(Elf64_Shdr) > obj->size) {
		error("%s: section headers extend past end of file", path);
	}
	obj->shdrs = (Elf64_Shdr *)(obj->data + obj->ehdr->e_shoff);

	if (obj->ehdr->e_shstrndx < obj->shnum) {
		Elf64_Shdr *shstrtab_sh = &obj->shdrs[obj->ehdr->e_shstrndx];
		obj->shstrtab = (char *)(obj->data + shstrtab_sh->sh_offset);
	}

	for (int i = 0; i < obj->shnum; i++) {
		Elf64_Shdr *sh = &obj->shdrs[i];
		if (sh->sh_type == SHT_SYMTAB) {
			obj->symtab_shndx = i;
			obj->symtab = (Elf64_Sym *)(obj->data + sh->sh_offset);
			obj->symcount = sh->sh_size / sizeof(Elf64_Sym);

			if (sh->sh_link < (uint32_t)obj->shnum) {
				Elf64_Shdr *strtab_sh = &obj->shdrs[sh->sh_link];
				obj->strtab = (char *)(obj->data + strtab_sh->sh_offset);
			}
			break;
		}
	}

	return obj;
}

void elf_free(ObjectFile *obj) {
	if (obj) {
		munmap(obj->data, obj->size);
		free(obj->filename);
		free(obj);
	}
}

const char *section_name(ObjectFile *obj, int idx) {
	if (idx < 0 || idx >= obj->shnum) {
		return NULL;
	}
	if (!obj->shstrtab) {
		return NULL;
	}
	return obj->shstrtab + obj->shdrs[idx].sh_name;
}

uint8_t *section_data(ObjectFile *obj, int idx) {
	if (idx < 0 || idx >= obj->shnum) {
		return NULL;
	}
	Elf64_Shdr *sh = &obj->shdrs[idx];
	if (sh->sh_type == SHT_NOBITS) {
		return NULL;
	}
	return obj->data + sh->sh_offset;
}

const char *symbol_name(ObjectFile *obj, int idx) {
	if (idx < 0 || idx >= obj->symcount) {
		return NULL;
	}
	if (!obj->strtab) {
		return NULL;
	}
	uint32_t name_idx = obj->symtab[idx].st_name;
	if (name_idx == 0) {
		return NULL;
	}
	return obj->strtab + name_idx;
}
