#include "elf.h"
#include "pmm.h"
#include "board.h"
#include "string.h"
#include "fs.h"

typedef int (*elf_reader)(void *ctx, char *buf, unsigned long offset, unsigned long len);

static int elf_validate(Elf64_Ehdr *ehdr) {
	unsigned int magic = *(unsigned int *)ehdr->e_ident;
	if (magic != ELF_MAGIC) {
		return -1;
	}
	if (ehdr->e_machine != EM_AARCH64) {
		return -1;
	}
	if (ehdr->e_type != ET_EXEC) {
		return -1;
	}
	return 0;
}

static int elf_load_segments(Elf64_Phdr *phdr, int phnum, pte_t *pagetable, unsigned long *brk, elf_reader reader, void *ctx) {
	unsigned long max_addr = 0;

	for (int i = 0; i < phnum; i++) {
		if (phdr[i].p_type != PT_LOAD) {
			continue;
		}

		unsigned long va = phdr[i].p_vaddr;
		unsigned long filesz = phdr[i].p_filesz;
		unsigned long memsz = phdr[i].p_memsz;
		unsigned long offset = phdr[i].p_offset;
		int write = (phdr[i].p_flags & PF_W) ? 1 : 0;
		int exec = (phdr[i].p_flags & PF_X) ? 1 : 0;

		unsigned long va_start = va & ~(PAGE_SIZE - 1);
		unsigned long va_end = (va + memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

		for (unsigned long page_va = va_start; page_va < va_end;
		     page_va += PAGE_SIZE) {
			paddr_t pa = pmm_alloc();
			if (pa == PMM_INVALID) {
				return -1;
			}

			void *page = PA_TO_VA(pa);

			unsigned long copy_start = (page_va < va) ? va : page_va;
			unsigned long copy_end = (page_va + PAGE_SIZE > va + filesz)
						     ? va + filesz
						     : page_va + PAGE_SIZE;

			if (copy_end > copy_start) {
				unsigned long page_offset = copy_start - page_va;
				unsigned long file_offset =
				    offset + (copy_start - va);
				unsigned long copy_len = copy_end - copy_start;

				if (reader(ctx, (char *)page + page_offset, file_offset, copy_len) !=
				    (int)copy_len) {
					pmm_free(pa);
					return -1;
				}
			}

			if (vmm_map_page(pagetable, page_va, pa, write, exec) <
			    0) {
				pmm_free(pa);
				return -1;
			}
		}

		if (va_end > max_addr) {
			max_addr = va_end;
		}
	}

	*brk = max_addr;
	return 0;
}

static int buf_reader(void *ctx, char *buf, unsigned long offset, unsigned long len) {
	const char *data = (const char *)ctx;
	memcpy(buf, data + offset, len);
	return (int)len;
}

int elf_load(const char *data, unsigned long size, pte_t *pagetable, unsigned long *entry, unsigned long *brk) {
	if (size < sizeof(Elf64_Ehdr)) {
		return -1;
	}

	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;
	if (elf_validate(ehdr) < 0) {
		return -1;
	}

	Elf64_Phdr *phdr = (Elf64_Phdr *)(data + ehdr->e_phoff);
	if (elf_load_segments(phdr, ehdr->e_phnum, pagetable, brk, buf_reader, (void *)data) < 0) {
		return -1;
	}

	*entry = ehdr->e_entry;
	return 0;
}

static int inode_reader(void *ctx, char *buf, unsigned long offset, unsigned long len) {
	struct inode *ip = (struct inode *)ctx;
	return fs_readi(ip, buf, offset, len);
}

int elf_load_from_inode(struct inode *ip, pte_t *pagetable, unsigned long *entry, unsigned long *brk) {
	Elf64_Ehdr ehdr;

	if (fs_readi(ip, (char *)&ehdr, 0, sizeof(ehdr)) != sizeof(ehdr)) {
		return -1;
	}

	if (elf_validate(&ehdr) < 0) {
		return -1;
	}

	if (ehdr.e_phnum > 16) {
		return -1;
	}

	Elf64_Phdr phdr[16];
	unsigned long phoff = ehdr.e_phoff;
	unsigned long phsize = ehdr.e_phnum * sizeof(Elf64_Phdr);

	if (fs_readi(ip, (char *)phdr, phoff, phsize) != (int)phsize) {
		return -1;
	}

	if (elf_load_segments(phdr, ehdr.e_phnum, pagetable, brk, inode_reader, ip) < 0) {
		return -1;
	}

	*entry = ehdr.e_entry;
	return 0;
}
