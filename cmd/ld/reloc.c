#include "ld.h"

const char *reloc_type_name(int type) {
	switch (type) {
	case R_AARCH64_NONE:
		return "R_AARCH64_NONE";
	case R_AARCH64_ABS64:
		return "R_AARCH64_ABS64";
	case R_AARCH64_ADR_PREL_PG_HI21:
		return "R_AARCH64_ADR_PREL_PG_HI21";
	case R_AARCH64_ADD_ABS_LO12_NC:
		return "R_AARCH64_ADD_ABS_LO12_NC";
	case R_AARCH64_LDST8_ABS_LO12_NC:
		return "R_AARCH64_LDST8_ABS_LO12_NC";
	case R_AARCH64_JUMP26:
		return "R_AARCH64_JUMP26";
	case R_AARCH64_CALL26:
		return "R_AARCH64_CALL26";
	case R_AARCH64_LDST16_ABS_LO12_NC:
		return "R_AARCH64_LDST16_ABS_LO12_NC";
	case R_AARCH64_LDST32_ABS_LO12_NC:
		return "R_AARCH64_LDST32_ABS_LO12_NC";
	case R_AARCH64_LDST64_ABS_LO12_NC:
		return "R_AARCH64_LDST64_ABS_LO12_NC";
	default:
		return "R_AARCH64_UNKNOWN";
	}
}

static Elf64_Rela *get_rela_for_section(ObjectFile *obj, int sec_idx, int *count) {
	*count = 0;
	for (int i = 0; i < obj->shnum; i++) {
		Elf64_Shdr *sh = &obj->shdrs[i];
		if (sh->sh_type == SHT_RELA && (int)sh->sh_info == sec_idx) {
			*count = sh->sh_size / sizeof(Elf64_Rela);
			return (Elf64_Rela *)(obj->data + sh->sh_offset);
		}
	}
	return NULL;
}

static uint64_t resolve_reloc_symbol(ObjectFile *obj, int sym_idx, SymbolTable *global, OutputSection *sections) {
	if (sym_idx < 0 || sym_idx >= obj->symcount) {
		return 0;
	}

	Elf64_Sym *sym = &obj->symtab[sym_idx];
	uint8_t binding = ELF64_ST_BIND(sym->st_info);
	uint8_t type = ELF64_ST_TYPE(sym->st_info);

	if (binding == STB_LOCAL || type == STT_SECTION) {
		return resolve_local_symbol(obj, sym_idx, sections);
	}

	const char *name = symbol_name(obj, sym_idx);
	if (!name || name[0] == '\0') {
		return resolve_local_symbol(obj, sym_idx, sections);
	}

	Symbol *gsym = symbol_lookup(global, name);
	if (gsym) {
		return gsym->value;
	}

	return resolve_local_symbol(obj, sym_idx, sections);
}

static int find_output_section_idx(OutputSection *sections, ObjectFile *file, int input_shndx) {
	for (int i = 1; i < OUT_COUNT; i++) {
		OutputSection *sec = &sections[i];
		for (int j = 0; j < sec->piece_count; j++) {
			SectionPiece *p = &sec->pieces[j];
			if (p->file == file && p->input_shndx == input_shndx) {
				return i;
			}
		}
	}
	return -1;
}

static bool apply_one_reloc(uint8_t *target, uint64_t place_addr, uint64_t sym_value, int type, int64_t addend, const char *filename) {
	uint64_t s = sym_value;
	int64_t a = addend;
	uint64_t p = place_addr;

	switch (type) {
	case R_AARCH64_NONE:
		break;

	case R_AARCH64_ABS64: {
		uint64_t val = s + a;
		memcpy(target, &val, 8);
		break;
	}

	case R_AARCH64_CALL26:
	case R_AARCH64_JUMP26: {
		int64_t offset = (int64_t)(s + a) - (int64_t)p;
		if (offset < -(1 << 27) || offset >= (1 << 27)) {
			error("%s: %s offset out of range: %lld",
			      filename,
			      reloc_type_name(type),
			      (long long)offset);
			return false;
		}
		if (offset & 3) {
			error("%s: %s offset not aligned: 0x%llx",
			      filename,
			      reloc_type_name(type),
			      (unsigned long long)offset);
			return false;
		}
		uint32_t insn;
		memcpy(&insn, target, 4);
		uint32_t imm26 = (offset >> 2) & 0x03ffffff;
		insn = (insn & 0xfc000000) | imm26;
		memcpy(target, &insn, 4);
		break;
	}

	case R_AARCH64_ADR_PREL_PG_HI21: {
		uint64_t page_s = (s + a) & ~0xfffULL;
		uint64_t page_p = p & ~0xfffULL;
		int64_t page_offset = (int64_t)page_s - (int64_t)page_p;
		if (page_offset < -(1LL << 32) || page_offset >= (1LL << 32)) {
			error("%s: %s page offset out of range: %lld",
			      filename,
			      reloc_type_name(type),
			      (long long)page_offset);
			return false;
		}
		uint32_t insn;
		memcpy(&insn, target, 4);
		int64_t imm = page_offset >> 12;
		uint32_t immlo = (imm & 3) << 29;
		uint32_t immhi = ((imm >> 2) & 0x7ffff) << 5;
		insn = (insn & 0x9f00001f) | immlo | immhi;
		memcpy(target, &insn, 4);
		break;
	}

	case R_AARCH64_ADD_ABS_LO12_NC:
	case R_AARCH64_LDST8_ABS_LO12_NC: {
		uint32_t insn;
		memcpy(&insn, target, 4);
		uint32_t imm12 = ((s + a) & 0xfff) << 10;
		insn = (insn & 0xffc003ff) | imm12;
		memcpy(target, &insn, 4);
		break;
	}

	case R_AARCH64_LDST16_ABS_LO12_NC: {
		uint64_t val = (s + a) & 0xfff;
		if (val & 1) {
			error("%s: %s address not 2-byte aligned: 0x%llx",
			      filename,
			      reloc_type_name(type),
			      (unsigned long long)(s + a));
			return false;
		}
		uint32_t insn;
		memcpy(&insn, target, 4);
		uint32_t imm12 = (val >> 1) << 10;
		insn = (insn & 0xffc003ff) | imm12;
		memcpy(target, &insn, 4);
		break;
	}

	case R_AARCH64_LDST32_ABS_LO12_NC: {
		uint64_t val = (s + a) & 0xfff;
		if (val & 3) {
			error("%s: %s address not 4-byte aligned: 0x%llx",
			      filename,
			      reloc_type_name(type),
			      (unsigned long long)(s + a));
			return false;
		}
		uint32_t insn;
		memcpy(&insn, target, 4);
		uint32_t imm12 = (val >> 2) << 10;
		insn = (insn & 0xffc003ff) | imm12;
		memcpy(target, &insn, 4);
		break;
	}

	case R_AARCH64_LDST64_ABS_LO12_NC: {
		uint64_t val = (s + a) & 0xfff;
		if (val & 7) {
			error("%s: %s address not 8-byte aligned: 0x%llx",
			      filename,
			      reloc_type_name(type),
			      (unsigned long long)(s + a));
			return false;
		}
		uint32_t insn;
		memcpy(&insn, target, 4);
		uint32_t imm12 = (val >> 3) << 10;
		insn = (insn & 0xffc003ff) | imm12;
		memcpy(target, &insn, 4);
		break;
	}

	default:
		error("%s: unsupported relocation type %d (%s)",
		      filename,
		      type,
		      reloc_type_name(type));
		return false;
	}

	return true;
}

bool apply_relocations(ObjectFile **objects, int count, SymbolTable *global, OutputSection *sections) {
	for (int i = 0; i < count; i++) {
		ObjectFile *obj = objects[i];

		for (int j = 1; j < obj->shnum; j++) {
			Elf64_Shdr *sh = &obj->shdrs[j];

			if (!(sh->sh_flags & SHF_ALLOC)) {
				continue;
			}

			int rela_count;
			Elf64_Rela *relas = get_rela_for_section(obj, j, &rela_count);
			if (!relas || rela_count == 0) {
				continue;
			}

			SectionPiece *piece = find_piece(sections, obj, j);
			if (!piece) {
				continue;
			}

			int out_idx = find_output_section_idx(sections, obj, j);
			if (out_idx < 0) {
				continue;
			}
			OutputSection *out = &sections[out_idx];

			for (int k = 0; k < rela_count; k++) {
				Elf64_Rela *rela = &relas[k];

				uint64_t output_offset = piece->output_offset + rela->r_offset;
				uint64_t place_addr = out->addr + output_offset;
				uint8_t *target = out->data + output_offset;

				int sym_idx = ELF64_R_SYM(rela->r_info);
				int rtype = ELF64_R_TYPE(rela->r_info);

				uint64_t sym_value = resolve_reloc_symbol(obj, sym_idx, global, sections);

				if (!apply_one_reloc(target, place_addr, sym_value, rtype, rela->r_addend, obj->filename)) {
					return false;
				}
			}
		}
	}

	return true;
}
