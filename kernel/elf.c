#include "elf.h"
#include "pmm.h"
#include "board.h"

static void memcpy(void *dst, const void *src, unsigned long n) {
	char *d = dst;
	const char *s = src;
	while (n--) {
		*d++ = *s++;
	}
}

static void memset(void *dst, int c, unsigned long n) {
	char *d = dst;
	while (n--) {
		*d++ = c;
	}
}

int elf_load(const char *data, unsigned long size, pte_t *pagetable, unsigned long *entry, unsigned long *brk) {
	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;

	if (size < sizeof(Elf64_Ehdr)) {
		return -1;
	}

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

	Elf64_Phdr *phdr = (Elf64_Phdr *)(data + ehdr->e_phoff);
	unsigned long max_addr = 0;

	for (int i = 0; i < ehdr->e_phnum; i++) {
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
			if (pa == 0) {
				return -1;
			}

			void *page = PA_TO_VA(pa);
			memset(page, 0, PAGE_SIZE);

			unsigned long copy_start = (page_va < va) ? va : page_va;
			unsigned long copy_end = (page_va + PAGE_SIZE > va + filesz)
						     ? va + filesz
						     : page_va + PAGE_SIZE;

			if (copy_end > copy_start) {
				unsigned long page_offset = copy_start - page_va;
				unsigned long file_offset =
				    offset + (copy_start - va);
				unsigned long copy_len = copy_end - copy_start;
				memcpy((char *)page + page_offset,
				       data + file_offset,
				       copy_len);
			}

			if (vmm_map_page(pagetable, page_va, pa, write, exec) <
			    0) {
				return -1;
			}
		}

		if (va_end > max_addr) {
			max_addr = va_end;
		}
	}

	*entry = ehdr->e_entry;
	*brk = max_addr;
	return 0;
}
