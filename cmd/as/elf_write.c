#include "as.h"

enum {
	SEC_NULL = 0,
	SEC_TEXT,
	SEC_DATA,
	SEC_BSS,
	SEC_SYMTAB,
	SEC_STRTAB,
	SEC_SHSTRTAB,
	SEC_RELA_TEXT,
	SEC_RELA_DATA,
	SEC_COUNT
};

static void write_bytes(FILE *fp, const void *data, size_t size) {
	fwrite(data, 1, size, fp);
}

static void write_padding(FILE *fp, size_t count) {
	for (size_t i = 0; i < count; i++) {
		fputc(0, fp);
	}
}

static size_t align_up(size_t val, size_t align) {
	return (val + align - 1) & ~(align - 1);
}

void elf_write(const char *filename) {
	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		error("cannot open output file: %s", filename);
	}

	StringTable strtab;
	StringTable shstrtab;
	strtab_init(&strtab);
	strtab_init(&shstrtab);

	const char *tname = text_section_name ? text_section_name : ".text";
	char rela_text_name[64];
	snprintf(rela_text_name, sizeof(rela_text_name), ".rela%s", tname);

	uint32_t sh_name_text = strtab_add(&shstrtab, tname);
	uint32_t sh_name_data = strtab_add(&shstrtab, ".data");
	uint32_t sh_name_bss = strtab_add(&shstrtab, ".bss");
	uint32_t sh_name_symtab = strtab_add(&shstrtab, ".symtab");
	uint32_t sh_name_strtab = strtab_add(&shstrtab, ".strtab");
	uint32_t sh_name_shstrtab = strtab_add(&shstrtab, ".shstrtab");
	uint32_t sh_name_rela_text = strtab_add(&shstrtab, rela_text_name);
	uint32_t sh_name_rela_data = strtab_add(&shstrtab, ".rela.data");

	int nsyms = symtab_count();
	int nlocals = 0;
	for (int i = 0; i < nsyms; i++) {
		Symbol *sym = symtab_get(i);
		// Undefined symbols must be global for linker resolution
		if (sym->binding == STB_LOCAL && sym->defined) {
			nlocals++;
		}
	}

	int total_elf_syms = 1 + 3 + nsyms;
	int first_global = 1 + 3 + nlocals;

	Elf64_Sym *elf_syms = calloc(total_elf_syms, sizeof(Elf64_Sym));
	int *sym_to_elf = calloc(nsyms, sizeof(int));

	int elf_idx = 1;

	uint32_t text_sym_name = strtab_add(&strtab, "$x");
	elf_syms[elf_idx].st_name = text_sym_name;
	elf_syms[elf_idx].st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
	elf_syms[elf_idx].st_shndx = SEC_TEXT;
	elf_idx++;

	uint32_t data_sym_name = strtab_add(&strtab, "$d");
	elf_syms[elf_idx].st_name = data_sym_name;
	elf_syms[elf_idx].st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
	elf_syms[elf_idx].st_shndx = SEC_DATA;
	elf_idx++;

	uint32_t bss_sym_name = strtab_add(&strtab, ".bss");
	elf_syms[elf_idx].st_name = bss_sym_name;
	elf_syms[elf_idx].st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
	elf_syms[elf_idx].st_shndx = SEC_BSS;
	elf_idx++;

	for (int i = 0; i < nsyms; i++) {
		Symbol *sym = symtab_get(i);
		// Skip if not a defined local symbol
		if (sym->binding != STB_LOCAL || !sym->defined) {
			continue;
		}
		uint32_t name_off = strtab_add(&strtab, sym->name);
		elf_syms[elf_idx].st_name = name_off;
		elf_syms[elf_idx].st_info = ELF64_ST_INFO(STB_LOCAL, sym->type);
		elf_syms[elf_idx].st_value = sym->value;
		elf_syms[elf_idx].st_size = sym->size;
		if (sym->section == SECTION_TEXT) {
			elf_syms[elf_idx].st_shndx = SEC_TEXT;
		} else if (sym->section == SECTION_DATA) {
			elf_syms[elf_idx].st_shndx = SEC_DATA;
		} else if (sym->section == SECTION_BSS) {
			elf_syms[elf_idx].st_shndx = SEC_BSS;
		} else {
			elf_syms[elf_idx].st_shndx = SHN_UNDEF;
		}
		sym_to_elf[i] = elf_idx;
		elf_idx++;
	}

	// Emit global symbols and undefined symbols (undefined must be global)
	for (int i = 0; i < nsyms; i++) {
		Symbol *sym = symtab_get(i);
		// Include global symbols and undefined local symbols
		if (sym->binding == STB_LOCAL && sym->defined) {
			continue;
		}
		uint32_t name_off = strtab_add(&strtab, sym->name);
		elf_syms[elf_idx].st_name = name_off;
		elf_syms[elf_idx].st_info = ELF64_ST_INFO(STB_GLOBAL, sym->type);
		elf_syms[elf_idx].st_value = sym->value;
		elf_syms[elf_idx].st_size = sym->size;
		if (sym->section == SECTION_TEXT) {
			elf_syms[elf_idx].st_shndx = SEC_TEXT;
		} else if (sym->section == SECTION_DATA) {
			elf_syms[elf_idx].st_shndx = SEC_DATA;
		} else if (sym->section == SECTION_BSS) {
			elf_syms[elf_idx].st_shndx = SEC_BSS;
		} else {
			elf_syms[elf_idx].st_shndx = SHN_UNDEF;
		}
		sym_to_elf[i] = elf_idx;
		elf_idx++;
	}

	int text_reloc_count = reloc_count(SECTION_TEXT);
	int data_reloc_count = reloc_count(SECTION_DATA);

	Elf64_Rela *text_relas = NULL;
	Elf64_Rela *data_relas = NULL;

	if (text_reloc_count > 0) {
		text_relas = calloc(text_reloc_count, sizeof(Elf64_Rela));
		int idx = text_reloc_count - 1;
		for (Reloc *r = reloc_get_list(SECTION_TEXT); r; r = r->next) {
			text_relas[idx].r_offset = r->offset;
			int elf_sym = (r->symbol_idx >= 0 && r->symbol_idx < nsyms)
					  ? sym_to_elf[r->symbol_idx]
					  : 0;
			text_relas[idx].r_info = ELF64_R_INFO(elf_sym, r->type);
			text_relas[idx].r_addend = r->addend;
			idx--;
		}
	}

	if (data_reloc_count > 0) {
		data_relas = calloc(data_reloc_count, sizeof(Elf64_Rela));
		int idx = data_reloc_count - 1;
		for (Reloc *r = reloc_get_list(SECTION_DATA); r; r = r->next) {
			data_relas[idx].r_offset = r->offset;
			int elf_sym = (r->symbol_idx >= 0 && r->symbol_idx < nsyms)
					  ? sym_to_elf[r->symbol_idx]
					  : 0;
			data_relas[idx].r_info = ELF64_R_INFO(elf_sym, r->type);
			data_relas[idx].r_addend = r->addend;
			idx--;
		}
	}

	size_t ehdr_size = sizeof(Elf64_Ehdr);
	size_t text_off = ehdr_size;
	size_t text_size = text_section.size;

	size_t data_off = align_up(text_off + text_size, 8);
	size_t data_size = data_section.size;

	size_t symtab_off = align_up(data_off + data_size, 8);
	size_t symtab_size = total_elf_syms * sizeof(Elf64_Sym);

	size_t strtab_off = symtab_off + symtab_size;
	size_t strtab_size = strtab.size;

	size_t shstrtab_off = strtab_off + strtab_size;
	size_t shstrtab_size = shstrtab.size;

	size_t rela_text_off = align_up(shstrtab_off + shstrtab_size, 8);
	size_t rela_text_size = text_reloc_count * sizeof(Elf64_Rela);

	size_t rela_data_off = rela_text_off + rela_text_size;
	size_t rela_data_size = data_reloc_count * sizeof(Elf64_Rela);

	size_t shdr_off = align_up(rela_data_off + rela_data_size, 8);

	Elf64_Ehdr ehdr = {0};
	ehdr.e_ident[0] = ELFMAG0;
	ehdr.e_ident[1] = ELFMAG1;
	ehdr.e_ident[2] = ELFMAG2;
	ehdr.e_ident[3] = ELFMAG3;
	ehdr.e_ident[4] = ELFCLASS64;
	ehdr.e_ident[5] = ELFDATA2LSB;
	ehdr.e_ident[6] = EV_CURRENT;
	ehdr.e_type = ET_REL;
	ehdr.e_machine = EM_AARCH64;
	ehdr.e_version = EV_CURRENT;
	ehdr.e_ehsize = sizeof(Elf64_Ehdr);
	ehdr.e_shoff = shdr_off;
	ehdr.e_shentsize = sizeof(Elf64_Shdr);
	ehdr.e_shnum = SEC_COUNT;
	ehdr.e_shstrndx = SEC_SHSTRTAB;

	Elf64_Shdr shdrs[SEC_COUNT] = {0};

	shdrs[SEC_TEXT].sh_name = sh_name_text;
	shdrs[SEC_TEXT].sh_type = SHT_PROGBITS;
	shdrs[SEC_TEXT].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
	shdrs[SEC_TEXT].sh_offset = text_off;
	shdrs[SEC_TEXT].sh_size = text_size;
	shdrs[SEC_TEXT].sh_addralign = 4;

	shdrs[SEC_DATA].sh_name = sh_name_data;
	shdrs[SEC_DATA].sh_type = SHT_PROGBITS;
	shdrs[SEC_DATA].sh_flags = SHF_ALLOC | SHF_WRITE;
	shdrs[SEC_DATA].sh_offset = data_off;
	shdrs[SEC_DATA].sh_size = data_size;
	shdrs[SEC_DATA].sh_addralign = 8;

	shdrs[SEC_BSS].sh_name = sh_name_bss;
	shdrs[SEC_BSS].sh_type = SHT_NOBITS;
	shdrs[SEC_BSS].sh_flags = SHF_ALLOC | SHF_WRITE;
	shdrs[SEC_BSS].sh_offset = data_off + data_size;
	shdrs[SEC_BSS].sh_size = bss_size;
	shdrs[SEC_BSS].sh_addralign = 8;

	shdrs[SEC_SYMTAB].sh_name = sh_name_symtab;
	shdrs[SEC_SYMTAB].sh_type = SHT_SYMTAB;
	shdrs[SEC_SYMTAB].sh_offset = symtab_off;
	shdrs[SEC_SYMTAB].sh_size = symtab_size;
	shdrs[SEC_SYMTAB].sh_link = SEC_STRTAB;
	shdrs[SEC_SYMTAB].sh_info = first_global;
	shdrs[SEC_SYMTAB].sh_addralign = 8;
	shdrs[SEC_SYMTAB].sh_entsize = sizeof(Elf64_Sym);

	shdrs[SEC_STRTAB].sh_name = sh_name_strtab;
	shdrs[SEC_STRTAB].sh_type = SHT_STRTAB;
	shdrs[SEC_STRTAB].sh_offset = strtab_off;
	shdrs[SEC_STRTAB].sh_size = strtab_size;
	shdrs[SEC_STRTAB].sh_addralign = 1;

	shdrs[SEC_SHSTRTAB].sh_name = sh_name_shstrtab;
	shdrs[SEC_SHSTRTAB].sh_type = SHT_STRTAB;
	shdrs[SEC_SHSTRTAB].sh_offset = shstrtab_off;
	shdrs[SEC_SHSTRTAB].sh_size = shstrtab_size;
	shdrs[SEC_SHSTRTAB].sh_addralign = 1;

	shdrs[SEC_RELA_TEXT].sh_name = sh_name_rela_text;
	shdrs[SEC_RELA_TEXT].sh_type = SHT_RELA;
	shdrs[SEC_RELA_TEXT].sh_flags = SHF_INFO_LINK;
	shdrs[SEC_RELA_TEXT].sh_offset = rela_text_off;
	shdrs[SEC_RELA_TEXT].sh_size = rela_text_size;
	shdrs[SEC_RELA_TEXT].sh_link = SEC_SYMTAB;
	shdrs[SEC_RELA_TEXT].sh_info = SEC_TEXT;
	shdrs[SEC_RELA_TEXT].sh_addralign = 8;
	shdrs[SEC_RELA_TEXT].sh_entsize = sizeof(Elf64_Rela);

	shdrs[SEC_RELA_DATA].sh_name = sh_name_rela_data;
	shdrs[SEC_RELA_DATA].sh_type = SHT_RELA;
	shdrs[SEC_RELA_DATA].sh_flags = SHF_INFO_LINK;
	shdrs[SEC_RELA_DATA].sh_offset = rela_data_off;
	shdrs[SEC_RELA_DATA].sh_size = rela_data_size;
	shdrs[SEC_RELA_DATA].sh_link = SEC_SYMTAB;
	shdrs[SEC_RELA_DATA].sh_info = SEC_DATA;
	shdrs[SEC_RELA_DATA].sh_addralign = 8;
	shdrs[SEC_RELA_DATA].sh_entsize = sizeof(Elf64_Rela);

	write_bytes(fp, &ehdr, sizeof(ehdr));

	if (text_size > 0) {
		write_bytes(fp, text_section.data, text_size);
	}

	size_t cur = text_off + text_size;
	if (cur < data_off) {
		write_padding(fp, data_off - cur);
	}

	if (data_size > 0) {
		write_bytes(fp, data_section.data, data_size);
	}

	cur = data_off + data_size;
	if (cur < symtab_off) {
		write_padding(fp, symtab_off - cur);
	}

	write_bytes(fp, elf_syms, symtab_size);
	write_bytes(fp, strtab.data, strtab_size);
	write_bytes(fp, shstrtab.data, shstrtab_size);

	cur = shstrtab_off + shstrtab_size;
	if (cur < rela_text_off) {
		write_padding(fp, rela_text_off - cur);
	}

	if (text_reloc_count > 0) {
		write_bytes(fp, text_relas, rela_text_size);
	}

	if (data_reloc_count > 0) {
		write_bytes(fp, data_relas, rela_data_size);
	}

	cur = rela_data_off + rela_data_size;
	if (cur < shdr_off) {
		write_padding(fp, shdr_off - cur);
	}

	write_bytes(fp, shdrs, sizeof(shdrs));

	fclose(fp);

	free(elf_syms);
	free(sym_to_elf);
	free(text_relas);
	free(data_relas);
}
