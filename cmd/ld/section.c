#include "ld.h"

void output_section_init(OutputSection *sec, const char *name, uint32_t type, uint64_t flags) {
	sec->name = name;
	sec->addr = 0;
	sec->offset = 0;
	sec->size = 0;
	sec->alignment = SECTION_ALIGN;
	sec->type = type;
	sec->flags = flags;
	sec->data = NULL;
	sec->piece_count = 0;
	sec->piece_capacity = 0;
	sec->pieces = NULL;
}

int categorize_section(const char *name, uint64_t flags) {
	if (!(flags & SHF_ALLOC)) {
		return -1;
	}
	if (strncmp(name, ".text", 5) == 0) {
		return OUT_TEXT;
	}
	if (strncmp(name, ".rodata", 7) == 0) {
		return OUT_RODATA;
	}
	if (strncmp(name, ".data", 5) == 0) {
		return OUT_DATA;
	}
	if (strncmp(name, ".bss", 4) == 0) {
		return OUT_BSS;
	}
	return -1;
}

static uint64_t align_up(uint64_t val, uint64_t align) {
	if (align == 0) {
		return val;
	}
	return (val + align - 1) & ~(align - 1);
}

static void add_piece(OutputSection *sec, ObjectFile *file, int input_shndx, uint64_t output_offset, uint64_t size) {
	if (sec->piece_count >= sec->piece_capacity) {
		int new_cap = sec->piece_capacity == 0 ? 16 : sec->piece_capacity * 2;
		sec->pieces = realloc(sec->pieces, new_cap * sizeof(SectionPiece));
		sec->piece_capacity = new_cap;
	}
	SectionPiece *p = &sec->pieces[sec->piece_count++];
	p->file = file;
	p->input_shndx = input_shndx;
	p->output_offset = output_offset;
	p->size = size;
}

void merge_sections(ObjectFile **objects, int count, OutputSection *sections) {
	for (int i = 0; i < count; i++) {
		ObjectFile *obj = objects[i];

		for (int j = 1; j < obj->shnum; j++) {
			Elf64_Shdr *sh = &obj->shdrs[j];
			const char *name = section_name(obj, j);
			if (!name) {
				continue;
			}

			int cat = categorize_section(name, sh->sh_flags);
			if (cat < 0) {
				continue;
			}

			OutputSection *out = &sections[cat];

			uint64_t align = sh->sh_addralign;
			if (align == 0) {
				align = 1;
			}
			if (align > out->alignment) {
				out->alignment = align;
			}

			uint64_t offset = align_up(out->size, align);

			add_piece(out, obj, j, offset, sh->sh_size);

			if (sh->sh_type != SHT_NOBITS && sh->sh_size > 0) {
				uint64_t new_size = offset + sh->sh_size;
				out->data = realloc(out->data, new_size);
				memset(out->data + out->size, 0, offset - out->size);
				memcpy(out->data + offset, section_data(obj, j), sh->sh_size);
			}

			out->size = offset + sh->sh_size;
		}
	}
}

void assign_addresses(OutputSection *sections) {
	uint64_t addr = TEXT_BASE;

	for (int i = 1; i < OUT_COUNT; i++) {
		OutputSection *sec = &sections[i];
		if (sec->size == 0) {
			continue;
		}

		addr = align_up(addr, sec->alignment);
		sec->addr = addr;
		addr += sec->size;
	}
}

SectionPiece *find_piece(OutputSection *sections, ObjectFile *file, int input_shndx) {
	for (int i = 1; i < OUT_COUNT; i++) {
		OutputSection *sec = &sections[i];
		for (int j = 0; j < sec->piece_count; j++) {
			SectionPiece *p = &sec->pieces[j];
			if (p->file == file && p->input_shndx == input_shndx) {
				return p;
			}
		}
	}
	return NULL;
}

static int find_section_for_piece(OutputSection *sections, SectionPiece *piece) {
	for (int i = 1; i < OUT_COUNT; i++) {
		OutputSection *sec = &sections[i];
		for (int j = 0; j < sec->piece_count; j++) {
			if (&sec->pieces[j] == piece) {
				return i;
			}
		}
	}
	return 0;
}

void update_symbol_values(SymbolTable *global, OutputSection *sections) {
	for (int i = 0; i < SYMTAB_BUCKETS; i++) {
		for (Symbol *sym = global->buckets[i]; sym; sym = sym->next) {
			if (sym->shndx == SHN_ABS) {
				continue;
			}

			SectionPiece *piece = find_piece(sections, sym->file, sym->shndx);
			if (!piece) {
				continue;
			}

			int out_idx = find_section_for_piece(sections, piece);
			OutputSection *sec = &sections[out_idx];

			sym->value = sec->addr + piece->output_offset + sym->value;
			sym->output_shndx = out_idx;
		}
	}
}

uint64_t resolve_local_symbol(ObjectFile *obj, int sym_idx, OutputSection *sections) {
	if (sym_idx < 0 || sym_idx >= obj->symcount) {
		return 0;
	}

	Elf64_Sym *sym = &obj->symtab[sym_idx];

	if (sym->st_shndx == SHN_UNDEF) {
		return 0;
	}
	if (sym->st_shndx == SHN_ABS) {
		return sym->st_value;
	}

	SectionPiece *piece = find_piece(sections, obj, sym->st_shndx);
	if (!piece) {
		return sym->st_value;
	}

	int out_idx = find_section_for_piece(sections, piece);
	OutputSection *sec = &sections[out_idx];

	return sec->addr + piece->output_offset + sym->st_value;
}

static const char *section_type_str(uint32_t type) {
	switch (type) {
	case SHT_PROGBITS:
		return "PROGBITS";
	case SHT_NOBITS:
		return "NOBITS";
	default:
		return "?";
	}
}

void dump_output_sections(OutputSection *sections) {
	printf("Output sections:\n");
	printf("%-10s %-12s %-12s %-10s %-10s %s\n",
	       "Name",
	       "Address",
	       "Size",
	       "Align",
	       "Type",
	       "Pieces");

	for (int i = 1; i < OUT_COUNT; i++) {
		OutputSection *sec = &sections[i];
		if (sec->size == 0 && sec->piece_count == 0) {
			continue;
		}

		printf("%-10s 0x%010llx 0x%010llx %-10llu %-10s %d\n",
		       sec->name,
		       (unsigned long long)sec->addr,
		       (unsigned long long)sec->size,
		       (unsigned long long)sec->alignment,
		       section_type_str(sec->type),
		       sec->piece_count);

		for (int j = 0; j < sec->piece_count; j++) {
			SectionPiece *p = &sec->pieces[j];
			printf("  [%d] %s:%d @ 0x%llx size 0x%llx\n",
			       j,
			       p->file->filename,
			       p->input_shndx,
			       (unsigned long long)p->output_offset,
			       (unsigned long long)p->size);
		}
	}
}
