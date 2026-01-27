#include "as.h"
#include <strings.h>

static int current_section;
static uint64_t lc_text;
static uint64_t lc_data;
static uint64_t lc_bss;
static uint64_t literal_pool_base;

static uint64_t *current_lc(void) {
	switch (current_section) {
	case SECTION_TEXT:
		return &lc_text;
	case SECTION_DATA:
		return &lc_data;
	case SECTION_BSS:
		return &lc_bss;
	default:
		return NULL;
	}
}

static void align_lc(int power) {
	uint64_t *lc = current_lc();
	if (!lc) {
		return;
	}
	uint64_t alignment = 1ULL << power;
	*lc = (*lc + alignment - 1) & ~(alignment - 1);
}

static void advance_lc(uint64_t bytes) {
	uint64_t *lc = current_lc();
	if (lc) {
		*lc += bytes;
	}
}

static Token *skip_to_newline(Token *tok) {
	while (tok->kind != TOK_NEWLINE && tok->kind != TOK_EOF) {
		tok = tok->next;
	}
	return tok;
}

static bool is_ldr_literal(Token *tok) {
	if (tok->kind != TOK_IDENT) {
		return false;
	}
	if (strcasecmp(tok->str, "ldr") != 0) {
		return false;
	}
	Token *t = tok->next;
	if (t->kind != TOK_REGISTER) {
		return false;
	}
	t = t->next;
	if (t->kind != TOK_COMMA) {
		return false;
	}
	t = t->next;
	return t->kind == TOK_EQUALS;
}

static Token *expect_ident(Token *tok) {
	if (tok->kind != TOK_IDENT) {
		error_tok(tok, "expected identifier");
	}
	return tok;
}

static void handle_directive(Token *tok) {
	if (!tok->str || tok->str[0] != '.') {
		return;
	}

	const char *dir = tok->str;

	if (strcmp(dir, ".text") == 0) {
		current_section = SECTION_TEXT;
		return;
	}
	if (strcmp(dir, ".data") == 0) {
		current_section = SECTION_DATA;
		return;
	}
	if (strcmp(dir, ".bss") == 0) {
		current_section = SECTION_BSS;
		return;
	}

	if (strcmp(dir, ".globl") == 0 || strcmp(dir, ".global") == 0) {
		Token *t = tok->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_IDENT) {
				Symbol *sym = symtab_add(t->str);
				symtab_set_binding(sym, STB_GLOBAL);
			}
			t = t->next;
		}
		return;
	}

	if (strcmp(dir, ".local") == 0) {
		Token *t = tok->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_IDENT) {
				Symbol *sym = symtab_add(t->str);
				symtab_set_binding(sym, STB_LOCAL);
			}
			t = t->next;
		}
		return;
	}

	if (strcmp(dir, ".type") == 0) {
		Token *name = expect_ident(tok->next);
		Symbol *sym = symtab_add(name->str);
		Token *t = name->next;
		if (t->kind == TOK_COMMA) {
			t = t->next;
		}
		if (t->kind == TOK_PERCENT) {
			t = t->next;
		}
		if (t->kind == TOK_IDENT) {
			if (strcmp(t->str, "function") == 0) {
				symtab_set_type(sym, STT_FUNC);
			} else if (strcmp(t->str, "object") == 0) {
				symtab_set_type(sym, STT_OBJECT);
			}
		}
		return;
	}

	if (strcmp(dir, ".size") == 0) {
		Token *name = expect_ident(tok->next);
		Symbol *sym = symtab_add(name->str);
		Token *t = name->next;
		if (t->kind == TOK_COMMA) {
			t = t->next;
		}
		if (t->kind == TOK_NUMBER) {
			sym->size = (int)t->val;
		}
		return;
	}

	if (strcmp(dir, ".byte") == 0) {
		Token *t = tok->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_NUMBER || t->kind == TOK_STRING) {
				if (t->kind == TOK_STRING) {
					advance_lc(strlen(t->str));
				} else {
					advance_lc(1);
				}
			}
			t = t->next;
		}
		return;
	}

	if (strcmp(dir, ".word") == 0) {
		Token *t = tok->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_NUMBER) {
				advance_lc(4);
			}
			t = t->next;
		}
		return;
	}

	if (strcmp(dir, ".xword") == 0) {
		Token *t = tok->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_NUMBER || t->kind == TOK_IDENT) {
				advance_lc(8);
			}
			t = t->next;
		}
		return;
	}

	if (strcmp(dir, ".zero") == 0) {
		Token *t = tok->next;
		if (t->kind == TOK_NUMBER) {
			advance_lc(t->val);
		}
		return;
	}

	if (strcmp(dir, ".align") == 0) {
		Token *t = tok->next;
		if (t->kind == TOK_NUMBER) {
			align_lc((int)t->val);
		}
		return;
	}

	if (strcmp(dir, ".file") == 0 || strcmp(dir, ".loc") == 0 ||
	    strcmp(dir, ".section") == 0 || strcmp(dir, ".ident") == 0 ||
	    strcmp(dir, ".cfi_startproc") == 0 ||
	    strcmp(dir, ".cfi_endproc") == 0 ||
	    strcmp(dir, ".cfi_def_cfa_offset") == 0 ||
	    strcmp(dir, ".cfi_offset") == 0) {
		return;
	}
}

void pass1(Token *tok) {
	symtab_init();
	literal_pool_init();
	current_section = SECTION_TEXT;
	lc_text = 0;
	lc_data = 0;
	lc_bss = 0;

	while (tok->kind != TOK_EOF) {
		if (tok->kind == TOK_NEWLINE) {
			tok = tok->next;
			continue;
		}

		if (tok->kind == TOK_LABEL) {
			Symbol *sym = symtab_add(tok->str);
			sym->section = current_section;
			uint64_t *lc = current_lc();
			sym->value = lc ? *lc : 0;
			sym->defined = 1;
			tok = tok->next;
			continue;
		}

		if (tok->kind == TOK_IDENT && tok->str[0] == '.') {
			handle_directive(tok);
			tok = skip_to_newline(tok);
			if (tok->kind == TOK_NEWLINE) {
				tok = tok->next;
			}
			continue;
		}

		if (is_ldr_literal(tok)) {
			Token *t = tok->next;
			t = t->next;
			t = t->next;
			t = t->next;
			if (t->kind == TOK_NUMBER) {
				literal_pool_add_value((uint64_t)t->val);
			} else if (t->kind == TOK_IDENT) {
				literal_pool_add_symbol(t->str);
			}
			advance_lc(4);
			tok = skip_to_newline(tok);
			if (tok->kind == TOK_NEWLINE) {
				tok = tok->next;
			}
			continue;
		}

		if (tok->kind == TOK_IDENT) {
			advance_lc(4);
			tok = skip_to_newline(tok);
			if (tok->kind == TOK_NEWLINE) {
				tok = tok->next;
			}
			continue;
		}

		tok = tok->next;
	}

	uint64_t alignment = 8;
	lc_text = (lc_text + alignment - 1) & ~(alignment - 1);
	literal_pool_base = lc_text;
	lc_text += literal_pool_size();
}

static const char *section_name(int section) {
	switch (section) {
	case SECTION_NONE:
		return "none";
	case SECTION_TEXT:
		return "text";
	case SECTION_DATA:
		return "data";
	case SECTION_BSS:
		return "bss";
	default:
		return "?";
	}
}

static const char *binding_name(int binding) {
	switch (binding) {
	case STB_LOCAL:
		return "local";
	case STB_GLOBAL:
		return "global";
	default:
		return "?";
	}
}

static const char *type_name(int type) {
	switch (type) {
	case STT_NOTYPE:
		return "notype";
	case STT_OBJECT:
		return "object";
	case STT_FUNC:
		return "func";
	default:
		return "?";
	}
}

void dump_symbols(void) {
	printf("Symbol Table (%d symbols):\n", symtab_count());
	printf("%-24s %8s %8s %8s %8s %6s %s\n", "Name", "Section", "Value", "Binding", "Type", "Size", "Defined");
	for (int i = 0; i < symtab_count(); i++) {
		Symbol *sym = symtab_get(i);
		printf("%-24s %8s %8llu %8s %8s %6d %7d\n", sym->name, section_name(sym->section), (unsigned long long)sym->value, binding_name(sym->binding), type_name(sym->type), sym->size, sym->defined);
	}
}

static SectionBuf *current_sec(void) {
	switch (current_section) {
	case SECTION_TEXT:
		return &text_section;
	case SECTION_DATA:
		return &data_section;
	default:
		return NULL;
	}
}

static void emit32(uint32_t val) {
	SectionBuf *sec = current_sec();
	if (sec) {
		section_emit32(sec, val);
	}
}

static void emit_reloc(int type, int sym_idx, int64_t addend) {
	SectionBuf *sec = current_sec();
	if (sec) {
		reloc_add(current_section, sec->size, type, sym_idx, addend);
	}
}

static Token *expect_register(Token *tok) {
	if (tok->kind != TOK_REGISTER) {
		error_tok(tok, "expected register");
	}
	return tok;
}

static Token *expect_comma(Token *tok) {
	if (tok->kind != TOK_COMMA) {
		error_tok(tok, "expected ','");
	}
	return tok->next;
}

static Token *skip_hash(Token *tok) {
	if (tok->kind == TOK_HASH) {
		return tok->next;
	}
	return tok;
}

static bool is_bcond(const char *name, int *cond) {
	if (name[0] != 'b' || name[1] != '.') {
		return false;
	}
	*cond = encode_cond(name + 2);
	return *cond >= 0;
}

static void handle_directive_p2(Token *tok) {
	const char *dir = tok->str;
	SectionBuf *sec = current_sec();

	if (strcmp(dir, ".text") == 0) {
		current_section = SECTION_TEXT;
		return;
	}
	if (strcmp(dir, ".data") == 0) {
		current_section = SECTION_DATA;
		return;
	}
	if (strcmp(dir, ".bss") == 0) {
		current_section = SECTION_BSS;
		return;
	}

	if (strcmp(dir, ".byte") == 0) {
		Token *t = tok->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_NUMBER) {
				if (sec) {
					section_emit8(sec, (uint8_t)t->val);
				}
			} else if (t->kind == TOK_STRING) {
				if (sec) {
					for (char *p = t->str; *p; p++) {
						section_emit8(sec, (uint8_t)*p);
					}
				}
			}
			t = t->next;
		}
		return;
	}

	if (strcmp(dir, ".word") == 0) {
		Token *t = tok->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_NUMBER) {
				if (sec) {
					section_emit32(sec, (uint32_t)t->val);
				}
			}
			t = t->next;
		}
		return;
	}

	if (strcmp(dir, ".xword") == 0) {
		Token *t = tok->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_NUMBER) {
				if (sec) {
					section_emit64(sec, (uint64_t)t->val);
				}
				t = t->next;
			} else if (t->kind == TOK_IDENT) {
				Symbol *sym = symtab_add(t->str);
				int sym_idx = symtab_get_index(sym);
				int64_t addend = 0;

				t = t->next;

				// Parse addend: +NUMBER or negative NUMBER
				if (t->kind == TOK_PLUS) {
					t = t->next;
					if (t->kind == TOK_NUMBER) {
						addend = t->val;
						t = t->next;
					}
				} else if (t->kind == TOK_NUMBER && t->val < 0) {
					addend = t->val;
					t = t->next;
				}

				if (sec) {
					reloc_add(current_section, sec->size, R_AARCH64_ABS64, sym_idx, addend);
					section_emit64(sec, 0);
				}
			} else {
				t = t->next;
			}
		}
		return;
	}

	if (strcmp(dir, ".zero") == 0) {
		Token *t = tok->next;
		if (t->kind == TOK_NUMBER && sec) {
			section_emit_zeros(sec, (size_t)t->val);
		}
		return;
	}

	if (strcmp(dir, ".align") == 0) {
		Token *t = tok->next;
		if (t->kind == TOK_NUMBER && sec) {
			section_align(sec, (int)t->val);
		}
		return;
	}
}

static void handle_instruction(Token *tok) {
	const char *mnemonic = tok->str;
	Token *t = tok->next;
	uint32_t insn = 0;
	int cond;

	if (strcasecmp(mnemonic, "nop") == 0) {
		emit32(encode_nop());
		return;
	}

	if (strcasecmp(mnemonic, "svc") == 0) {
		t = skip_hash(t);
		int imm16 = (int)t->val;
		emit32(encode_svc(imm16));
		return;
	}

	if (strcasecmp(mnemonic, "ret") == 0) {
		int rn = 30;
		if (t->kind == TOK_REGISTER) {
			rn = encode_gpr(t);
		}
		emit32(encode_ret(rn));
		return;
	}

	if (strcasecmp(mnemonic, "bl") == 0) {
		if (t->kind == TOK_IDENT) {
			Symbol *sym = symtab_lookup(t->str);
			if (sym && sym->defined && sym->section == current_section &&
			    sym->binding == STB_LOCAL) {
				SectionBuf *sec = current_sec();
				int64_t offset =
				    (int64_t)sym->value - (int64_t)(sec ? sec->size : 0);
				emit32(encode_bl((int32_t)offset));
			} else {
				sym = symtab_add(t->str);
				int sym_idx = symtab_get_index(sym);
				emit_reloc(R_AARCH64_CALL26, sym_idx, 0);
				emit32(encode_bl(0));
			}
		} else if (t->kind == TOK_NUMBER) {
			emit32(encode_bl((int32_t)t->val));
		}
		return;
	}

	if (strcasecmp(mnemonic, "b") == 0) {
		if (t->kind == TOK_IDENT) {
			Symbol *sym = symtab_lookup(t->str);
			if (sym && sym->defined && sym->section == current_section &&
			    sym->binding == STB_LOCAL) {
				SectionBuf *sec = current_sec();
				int64_t offset =
				    (int64_t)sym->value - (int64_t)(sec ? sec->size : 0);
				emit32(encode_b((int32_t)offset));
			} else {
				sym = symtab_add(t->str);
				int sym_idx = symtab_get_index(sym);
				emit_reloc(R_AARCH64_JUMP26, sym_idx, 0);
				emit32(encode_b(0));
			}
		} else if (t->kind == TOK_NUMBER) {
			emit32(encode_b((int32_t)t->val));
		}
		return;
	}

	if (is_bcond(mnemonic, &cond)) {
		if (t->kind == TOK_IDENT) {
			Symbol *sym = symtab_lookup(t->str);
			if (sym && sym->defined && sym->section == current_section) {
				SectionBuf *sec = current_sec();
				int64_t offset =
				    (int64_t)sym->value - (int64_t)(sec ? sec->size : 0);
				emit32(encode_bcond(cond, (int32_t)offset));
			} else {
				emit32(encode_bcond(cond, 0));
			}
		} else if (t->kind == TOK_NUMBER) {
			emit32(encode_bcond(cond, (int32_t)t->val));
		}
		return;
	}

	if (strcasecmp(mnemonic, "blr") == 0) {
		expect_register(t);
		emit32(encode_blr(encode_gpr(t)));
		return;
	}

	if (strcasecmp(mnemonic, "br") == 0) {
		expect_register(t);
		emit32(encode_br(encode_gpr(t)));
		return;
	}

	if (strcasecmp(mnemonic, "cbz") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_IDENT) {
			Symbol *sym = symtab_lookup(t->str);
			if (sym && sym->defined && sym->section == current_section) {
				SectionBuf *sec = current_sec();
				int64_t offset =
				    (int64_t)sym->value - (int64_t)(sec ? sec->size : 0);
				emit32(encode_cbz(sf, rt, (int32_t)offset));
			} else {
				emit32(encode_cbz(sf, rt, 0));
			}
		} else {
			emit32(encode_cbz(sf, rt, (int32_t)t->val));
		}
		return;
	}

	if (strcasecmp(mnemonic, "cbnz") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_IDENT) {
			Symbol *sym = symtab_lookup(t->str);
			if (sym && sym->defined && sym->section == current_section) {
				SectionBuf *sec = current_sec();
				int64_t offset =
				    (int64_t)sym->value - (int64_t)(sec ? sec->size : 0);
				emit32(encode_cbnz(sf, rt, (int32_t)offset));
			} else {
				emit32(encode_cbnz(sf, rt, 0));
			}
		} else {
			emit32(encode_cbnz(sf, rt, (int32_t)t->val));
		}
		return;
	}

	if (strcasecmp(mnemonic, "adrp") == 0) {
		expect_register(t);
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_IDENT) {
			Symbol *sym = symtab_add(t->str);
			int sym_idx = symtab_get_index(sym);
			emit_reloc(R_AARCH64_ADR_PREL_PG_HI21, sym_idx, 0);
		}
		emit32(encode_adrp(rd, 0));
		return;
	}

	if (strcasecmp(mnemonic, "adr") == 0) {
		expect_register(t);
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		emit32(encode_adr(rd, 0));
		return;
	}

	if (strcasecmp(mnemonic, "add") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);

		if (t->kind == TOK_LO12) {
			t = t->next;
			if (t->kind == TOK_IDENT) {
				Symbol *sym = symtab_add(t->str);
				int sym_idx = symtab_get_index(sym);
				emit_reloc(R_AARCH64_ADD_ABS_LO12_NC, sym_idx, 0);
			}
			emit32(encode_add_imm(sf, rd, rn, 0, 0));
			return;
		}

		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_add_reg(sf, rd, rn, rm));
		} else {
			int shift;
			int imm12 = encode_imm12(t->val, &shift);
			emit32(encode_add_imm(sf, rd, rn, imm12, shift ? 1 : 0));
		}
		return;
	}

	if (strcasecmp(mnemonic, "sub") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_sub_reg(sf, rd, rn, rm));
		} else {
			int shift;
			int imm12 = encode_imm12(t->val, &shift);
			emit32(encode_sub_imm(sf, rd, rn, imm12, shift ? 1 : 0));
		}
		return;
	}

	if (strcasecmp(mnemonic, "adds") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_adds_reg(sf, rd, rn, rm));
		} else {
			int shift;
			int imm12 = encode_imm12(t->val, &shift);
			emit32(encode_adds_imm(sf, rd, rn, imm12, shift ? 1 : 0));
		}
		return;
	}

	if (strcasecmp(mnemonic, "subs") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_subs_reg(sf, rd, rn, rm));
		} else {
			int shift;
			int imm12 = encode_imm12(t->val, &shift);
			emit32(encode_subs_imm(sf, rd, rn, imm12, shift ? 1 : 0));
		}
		return;
	}

	if (strcasecmp(mnemonic, "mul") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_mul(sf, rd, rn, rm));
		return;
	}

	if (strcasecmp(mnemonic, "sdiv") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_sdiv(sf, rd, rn, rm));
		return;
	}

	if (strcasecmp(mnemonic, "udiv") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_udiv(sf, rd, rn, rm));
		return;
	}

	if (strcasecmp(mnemonic, "madd") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int ra = encode_gpr(t);
		emit32(encode_madd(sf, rd, rn, rm, ra));
		return;
	}

	if (strcasecmp(mnemonic, "msub") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int ra = encode_gpr(t);
		emit32(encode_msub(sf, rd, rn, rm, ra));
		return;
	}

	if (strcasecmp(mnemonic, "neg") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_neg(sf, rd, rm));
		return;
	}

	if (strcasecmp(mnemonic, "negs") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_negs(sf, rd, rm));
		return;
	}

	if (strcasecmp(mnemonic, "and") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_and_reg(sf, rd, rn, rm));
		return;
	}

	if (strcasecmp(mnemonic, "orr") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_orr_reg(sf, rd, rn, rm));
		return;
	}

	if (strcasecmp(mnemonic, "eor") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_eor_reg(sf, rd, rn, rm));
		return;
	}

	if (strcasecmp(mnemonic, "mvn") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_mvn(sf, rd, rm));
		return;
	}

	if (strcasecmp(mnemonic, "bic") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_bic(sf, rd, rn, rm));
		return;
	}

	if (strcasecmp(mnemonic, "ands") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_ands_reg(sf, rd, rn, rm));
		return;
	}

	if (strcasecmp(mnemonic, "tst") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_tst_reg(sf, rn, rm));
		return;
	}

	if (strcasecmp(mnemonic, "lsl") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_lsl_reg(sf, rd, rn, rm));
		return;
	}

	if (strcasecmp(mnemonic, "lsr") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_lsr_reg(sf, rd, rn, rm));
		return;
	}

	if (strcasecmp(mnemonic, "asr") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		emit32(encode_asr_reg(sf, rd, rn, rm));
		return;
	}

	if (strcasecmp(mnemonic, "mov") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			if (t->reg_type == REG_SP) {
				emit32(encode_add_imm(sf, rd, 31, 0, 0));
			} else {
				int rm = encode_gpr(t);
				emit32(encode_mov_reg(sf, rd, rm));
			}
		} else {
			emit32(encode_movz(sf, rd, (int)t->val, 0));
		}
		return;
	}

	if (strcasecmp(mnemonic, "movz") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		t = skip_hash(t);
		int imm = (int)t->val;
		int hw = 0;
		t = t->next;
		if (t->kind == TOK_COMMA) {
			t = t->next;
			if (t->kind == TOK_IDENT && strcasecmp(t->str, "lsl") == 0) {
				t = t->next;
				t = skip_hash(t);
				hw = (int)t->val / 16;
			}
		}
		emit32(encode_movz(sf, rd, imm, hw));
		return;
	}

	if (strcasecmp(mnemonic, "movk") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		t = skip_hash(t);
		int imm = (int)t->val;
		int hw = 0;
		t = t->next;
		if (t->kind == TOK_COMMA) {
			t = t->next;
			if (t->kind == TOK_IDENT && strcasecmp(t->str, "lsl") == 0) {
				t = t->next;
				t = skip_hash(t);
				hw = (int)t->val / 16;
			}
		}
		emit32(encode_movk(sf, rd, imm, hw));
		return;
	}

	if (strcasecmp(mnemonic, "movn") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		t = skip_hash(t);
		int imm = (int)t->val;
		int hw = 0;
		t = t->next;
		if (t->kind == TOK_COMMA) {
			t = t->next;
			if (t->kind == TOK_IDENT && strcasecmp(t->str, "lsl") == 0) {
				t = t->next;
				t = skip_hash(t);
				hw = (int)t->val / 16;
			}
		}
		emit32(encode_movn(sf, rd, imm, hw));
		return;
	}

	if (strcasecmp(mnemonic, "cmp") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_cmp_reg(sf, rn, rm));
		} else {
			int shift;
			int imm12 = encode_imm12(t->val, &shift);
			emit32(encode_cmp_imm(sf, rn, imm12, shift ? 1 : 0));
		}
		return;
	}

	if (strcasecmp(mnemonic, "cmn") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_cmn_reg(sf, rn, rm));
		} else {
			int shift;
			int imm12 = encode_imm12(t->val, &shift);
			emit32(encode_cmn_imm(sf, rn, imm12, shift ? 1 : 0));
		}
		return;
	}

	if (strcasecmp(mnemonic, "cset") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		cond = encode_cond(t->str);
		emit32(encode_cset(sf, rd, cond));
		return;
	}

	if (strcasecmp(mnemonic, "csetm") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		cond = encode_cond(t->str);
		emit32(encode_csetm(sf, rd, cond));
		return;
	}

	if (strcasecmp(mnemonic, "csinc") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		t = expect_comma(t->next);
		cond = encode_cond(t->str);
		emit32(encode_csinc(sf, rd, rn, rm, cond));
		return;
	}

	if (strcasecmp(mnemonic, "csel") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		t = expect_comma(t->next);
		cond = encode_cond(t->str);
		emit32(encode_csel(sf, rd, rn, rm, cond));
		return;
	}

	if (strcasecmp(mnemonic, "csinv") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		t = expect_comma(t->next);
		cond = encode_cond(t->str);
		emit32(encode_csinv(sf, rd, rn, rm, cond));
		return;
	}

	if (strcasecmp(mnemonic, "csneg") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rm = encode_gpr(t);
		t = expect_comma(t->next);
		cond = encode_cond(t->str);
		emit32(encode_csneg(sf, rd, rn, rm, cond));
		return;
	}

	if (strcasecmp(mnemonic, "cinc") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		cond = encode_cond(t->str);
		emit32(encode_cinc(sf, rd, rn, cond));
		return;
	}

	if (strcasecmp(mnemonic, "sxtb") == 0) {
		expect_register(t);
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		emit32(encode_sxtb(rd, rn));
		return;
	}

	if (strcasecmp(mnemonic, "sxth") == 0) {
		expect_register(t);
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		emit32(encode_sxth(rd, rn));
		return;
	}

	if (strcasecmp(mnemonic, "sxtw") == 0) {
		expect_register(t);
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		emit32(encode_sxtw(rd, rn));
		return;
	}

	if (strcasecmp(mnemonic, "uxtb") == 0) {
		expect_register(t);
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		emit32(encode_uxtb(rd, rn));
		return;
	}

	if (strcasecmp(mnemonic, "uxth") == 0) {
		expect_register(t);
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		emit32(encode_uxth(rd, rn));
		return;
	}

	if (strcasecmp(mnemonic, "ldr") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int is_fp = t->reg_type == REG_FP;
		int rt = is_fp ? encode_fpr(t) : encode_gpr(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		t = expect_comma(t->next);

		if (t->kind == TOK_EQUALS) {
			t = t->next;
			LiteralEntry *entry = NULL;
			if (t->kind == TOK_NUMBER) {
				entry = literal_pool_add_value((uint64_t)t->val);
			} else if (t->kind == TOK_IDENT) {
				entry = literal_pool_add_symbol(t->str);
			}
			SectionBuf *sec = current_sec();
			int64_t current_pc = sec ? (int64_t)sec->size : 0;
			int64_t pool_entry_addr =
			    (int64_t)literal_pool_base + (int64_t)entry->pool_offset;
			int64_t offset = pool_entry_addr - current_pc;
			emit32(encode_ldr_literal(sf, rt, offset));
			return;
		}

		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			if (t->kind == TOK_RBRACKET) {
				t = t->next;
				if (t->kind == TOK_COMMA) {
					t = t->next;
					t = skip_hash(t);
					int64_t imm = t->val;
					if (is_fp) {
						emit32(encode_ldr_fp_post(ftype, rt, rn, imm));
					} else {
						emit32(encode_ldr_post(sf, rt, rn, imm));
					}
				} else {
					if (is_fp) {
						emit32(encode_ldr_fp_uoff(ftype, rt, rn, 0));
					} else {
						emit32(encode_ldr_uoff(sf, rt, rn, 0));
					}
				}
			} else if (t->kind == TOK_COMMA) {
				t = t->next;
				if (t->kind == TOK_LO12) {
					t = t->next;
					Symbol *sym = NULL;
					if (t->kind == TOK_IDENT) {
						sym = symtab_add(t->str);
					}
					int sym_idx = sym ? symtab_get_index(sym) : -1;
					int reloc_type;
					if (is_fp) {
						reloc_type = sf ? R_AARCH64_LDST64_ABS_LO12_NC
								: R_AARCH64_LDST32_ABS_LO12_NC;
					} else {
						reloc_type = sf ? R_AARCH64_LDST64_ABS_LO12_NC
								: R_AARCH64_LDST32_ABS_LO12_NC;
					}
					emit_reloc(reloc_type, sym_idx, 0);
					if (is_fp) {
						emit32(encode_ldr_fp_uoff(ftype, rt, rn, 0));
					} else {
						emit32(encode_ldr_uoff(sf, rt, rn, 0));
					}
				} else {
					t = skip_hash(t);
					int64_t imm = t->val;
					t = t->next;
					if (t->kind == TOK_RBRACKET) {
						t = t->next;
						if (t->kind == TOK_BANG) {
							if (is_fp) {
								emit32(encode_ldr_fp_pre(ftype, rt, rn, imm));
							} else {
								emit32(encode_ldr_pre(sf, rt, rn, imm));
							}
						} else {
							if (is_fp) {
								emit32(encode_ldr_fp_uoff(ftype, rt, rn, imm));
							} else {
								emit32(encode_ldr_uoff(sf, rt, rn, imm));
							}
						}
					}
				}
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "str") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int is_fp = t->reg_type == REG_FP;
		int rt = is_fp ? encode_fpr(t) : encode_gpr(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		t = expect_comma(t->next);

		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			if (t->kind == TOK_RBRACKET) {
				t = t->next;
				if (t->kind == TOK_COMMA) {
					t = t->next;
					t = skip_hash(t);
					int64_t imm = t->val;
					if (is_fp) {
						emit32(encode_str_fp_post(ftype, rt, rn, imm));
					} else {
						emit32(encode_str_post(sf, rt, rn, imm));
					}
				} else {
					if (is_fp) {
						emit32(encode_str_fp_uoff(ftype, rt, rn, 0));
					} else {
						emit32(encode_str_uoff(sf, rt, rn, 0));
					}
				}
			} else if (t->kind == TOK_COMMA) {
				t = t->next;
				if (t->kind == TOK_LO12) {
					t = t->next;
					Symbol *sym = NULL;
					if (t->kind == TOK_IDENT) {
						sym = symtab_add(t->str);
					}
					int sym_idx = sym ? symtab_get_index(sym) : -1;
					int reloc_type = sf ? R_AARCH64_LDST64_ABS_LO12_NC
							    : R_AARCH64_LDST32_ABS_LO12_NC;
					emit_reloc(reloc_type, sym_idx, 0);
					if (is_fp) {
						emit32(encode_str_fp_uoff(ftype, rt, rn, 0));
					} else {
						emit32(encode_str_uoff(sf, rt, rn, 0));
					}
				} else {
					t = skip_hash(t);
					int64_t imm = t->val;
					t = t->next;
					if (t->kind == TOK_RBRACKET) {
						t = t->next;
						if (t->kind == TOK_BANG) {
							if (is_fp) {
								emit32(encode_str_fp_pre(ftype, rt, rn, imm));
							} else {
								emit32(encode_str_pre(sf, rt, rn, imm));
							}
						} else {
							if (is_fp) {
								emit32(encode_str_fp_uoff(ftype, rt, rn, imm));
							} else {
								emit32(encode_str_uoff(sf, rt, rn, imm));
							}
						}
					}
				}
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldrb") == 0) {
		expect_register(t);
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			if (t->kind == TOK_RBRACKET) {
				emit32(encode_ldrb_uoff(rt, rn, 0));
			} else if (t->kind == TOK_COMMA) {
				t = t->next;
				if (t->kind == TOK_LO12) {
					t = t->next;
					Symbol *sym = NULL;
					if (t->kind == TOK_IDENT) {
						sym = symtab_add(t->str);
					}
					int sym_idx = sym ? symtab_get_index(sym) : -1;
					emit_reloc(R_AARCH64_LDST8_ABS_LO12_NC, sym_idx, 0);
					emit32(encode_ldrb_uoff(rt, rn, 0));
				} else {
					t = skip_hash(t);
					int64_t imm = t->val;
					emit32(encode_ldrb_uoff(rt, rn, imm));
				}
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "strb") == 0) {
		expect_register(t);
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			if (t->kind == TOK_RBRACKET) {
				emit32(encode_strb_uoff(rt, rn, 0));
			} else if (t->kind == TOK_COMMA) {
				t = t->next;
				if (t->kind == TOK_LO12) {
					t = t->next;
					Symbol *sym = NULL;
					if (t->kind == TOK_IDENT) {
						sym = symtab_add(t->str);
					}
					int sym_idx = sym ? symtab_get_index(sym) : -1;
					emit_reloc(R_AARCH64_LDST8_ABS_LO12_NC, sym_idx, 0);
					emit32(encode_strb_uoff(rt, rn, 0));
				} else {
					t = skip_hash(t);
					int64_t imm = t->val;
					emit32(encode_strb_uoff(rt, rn, imm));
				}
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldrh") == 0) {
		expect_register(t);
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			if (t->kind == TOK_RBRACKET) {
				emit32(encode_ldrh_uoff(rt, rn, 0));
			} else if (t->kind == TOK_COMMA) {
				t = t->next;
				if (t->kind == TOK_LO12) {
					t = t->next;
					Symbol *sym = NULL;
					if (t->kind == TOK_IDENT) {
						sym = symtab_add(t->str);
					}
					int sym_idx = sym ? symtab_get_index(sym) : -1;
					emit_reloc(R_AARCH64_LDST16_ABS_LO12_NC, sym_idx, 0);
					emit32(encode_ldrh_uoff(rt, rn, 0));
				} else {
					t = skip_hash(t);
					int64_t imm = t->val;
					emit32(encode_ldrh_uoff(rt, rn, imm));
				}
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "strh") == 0) {
		expect_register(t);
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			if (t->kind == TOK_RBRACKET) {
				emit32(encode_strh_uoff(rt, rn, 0));
			} else if (t->kind == TOK_COMMA) {
				t = t->next;
				if (t->kind == TOK_LO12) {
					t = t->next;
					Symbol *sym = NULL;
					if (t->kind == TOK_IDENT) {
						sym = symtab_add(t->str);
					}
					int sym_idx = sym ? symtab_get_index(sym) : -1;
					emit_reloc(R_AARCH64_LDST16_ABS_LO12_NC, sym_idx, 0);
					emit32(encode_strh_uoff(rt, rn, 0));
				} else {
					t = skip_hash(t);
					int64_t imm = t->val;
					emit32(encode_strh_uoff(rt, rn, imm));
				}
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldrsw") == 0) {
		expect_register(t);
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			if (t->kind == TOK_RBRACKET) {
				emit32(encode_ldrsw_uoff(rt, rn, 0));
			} else if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				int64_t imm = t->val;
				emit32(encode_ldrsw_uoff(rt, rn, imm));
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldrsb") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			int64_t imm = 0;
			if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				imm = t->val;
			}
			if (sf) {
				emit32(encode_ldrsb64_uoff(rt, rn, imm));
			} else {
				emit32(encode_ldrsb32_uoff(rt, rn, imm));
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldrsh") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			int64_t imm = 0;
			if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				imm = t->val;
			}
			if (sf) {
				emit32(encode_ldrsh64_uoff(rt, rn, imm));
			} else {
				emit32(encode_ldrsh32_uoff(rt, rn, imm));
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "stp") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rt1 = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rt2 = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			if (t->kind == TOK_RBRACKET) {
				t = t->next;
				if (t->kind == TOK_COMMA) {
					t = t->next;
					t = skip_hash(t);
					int64_t imm = t->val;
					emit32(encode_stp_post(sf, rt1, rt2, rn, imm));
				} else {
					emit32(encode_stp_off(sf, rt1, rt2, rn, 0));
				}
			} else if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				int64_t imm = t->val;
				t = t->next;
				if (t->kind == TOK_RBRACKET) {
					t = t->next;
					if (t->kind == TOK_BANG) {
						emit32(encode_stp_pre(sf, rt1, rt2, rn, imm));
					} else {
						emit32(encode_stp_off(sf, rt1, rt2, rn, imm));
					}
				}
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldp") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rt1 = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rt2 = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			if (t->kind == TOK_RBRACKET) {
				t = t->next;
				if (t->kind == TOK_COMMA) {
					t = t->next;
					t = skip_hash(t);
					int64_t imm = t->val;
					emit32(encode_ldp_post(sf, rt1, rt2, rn, imm));
				} else {
					emit32(encode_ldp_off(sf, rt1, rt2, rn, 0));
				}
			} else if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				int64_t imm = t->val;
				t = t->next;
				if (t->kind == TOK_RBRACKET) {
					t = t->next;
					if (t->kind == TOK_BANG) {
						emit32(encode_ldp_pre(sf, rt1, rt2, rn, imm));
					} else {
						emit32(encode_ldp_off(sf, rt1, rt2, rn, imm));
					}
				}
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "stur") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			int64_t imm = 0;
			if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				imm = t->val;
			}
			emit32(encode_stur(sf, rt, rn, imm));
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldur") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			int64_t imm = 0;
			if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				imm = t->val;
			}
			emit32(encode_ldur(sf, rt, rn, imm));
		}
		return;
	}

	if (strcasecmp(mnemonic, "sturb") == 0) {
		expect_register(t);
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			int64_t imm = 0;
			if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				imm = t->val;
			}
			emit32(encode_sturb(rt, rn, imm));
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldurb") == 0) {
		expect_register(t);
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			int64_t imm = 0;
			if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				imm = t->val;
			}
			emit32(encode_ldurb(rt, rn, imm));
		}
		return;
	}

	if (strcasecmp(mnemonic, "sturh") == 0) {
		expect_register(t);
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			int64_t imm = 0;
			if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				imm = t->val;
			}
			emit32(encode_sturh(rt, rn, imm));
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldurh") == 0) {
		expect_register(t);
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			int64_t imm = 0;
			if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				imm = t->val;
			}
			emit32(encode_ldurh(rt, rn, imm));
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldursw") == 0) {
		expect_register(t);
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			int64_t imm = 0;
			if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				imm = t->val;
			}
			emit32(encode_ldursw(rt, rn, imm));
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldursb") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			int64_t imm = 0;
			if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				imm = t->val;
			}
			if (sf) {
				emit32(encode_ldursb64(rt, rn, imm));
			} else {
				emit32(encode_ldursb32(rt, rn, imm));
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldursh") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_LBRACKET) {
			t = t->next;
			expect_register(t);
			int rn = encode_gpr(t);
			t = t->next;
			int64_t imm = 0;
			if (t->kind == TOK_COMMA) {
				t = t->next;
				t = skip_hash(t);
				imm = t->val;
			}
			if (sf) {
				emit32(encode_ldursh64(rt, rn, imm));
			} else {
				emit32(encode_ldursh32(rt, rn, imm));
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "fadd") == 0) {
		expect_register(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		int fd = encode_fpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int fn = encode_fpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int fm = encode_fpr(t);
		emit32(encode_fadd(ftype, fd, fn, fm));
		return;
	}

	if (strcasecmp(mnemonic, "fsub") == 0) {
		expect_register(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		int fd = encode_fpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int fn = encode_fpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int fm = encode_fpr(t);
		emit32(encode_fsub(ftype, fd, fn, fm));
		return;
	}

	if (strcasecmp(mnemonic, "fmul") == 0) {
		expect_register(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		int fd = encode_fpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int fn = encode_fpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int fm = encode_fpr(t);
		emit32(encode_fmul(ftype, fd, fn, fm));
		return;
	}

	if (strcasecmp(mnemonic, "fdiv") == 0) {
		expect_register(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		int fd = encode_fpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int fn = encode_fpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int fm = encode_fpr(t);
		emit32(encode_fdiv(ftype, fd, fn, fm));
		return;
	}

	if (strcasecmp(mnemonic, "fneg") == 0) {
		expect_register(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		int fd = encode_fpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int fn = encode_fpr(t);
		emit32(encode_fneg(ftype, fd, fn));
		return;
	}

	if (strcasecmp(mnemonic, "fcmp") == 0) {
		expect_register(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		int fn = encode_fpr(t);
		t = expect_comma(t->next);
		if (t->kind == TOK_REGISTER) {
			int fm = encode_fpr(t);
			emit32(encode_fcmp_reg(ftype, fn, fm));
		} else {
			emit32(encode_fcmp_zero(ftype, fn));
		}
		return;
	}

	if (strcasecmp(mnemonic, "scvtf") == 0) {
		expect_register(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		int fd = encode_fpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rn = encode_gpr(t);
		emit32(encode_scvtf(sf, ftype, fd, rn));
		return;
	}

	if (strcasecmp(mnemonic, "ucvtf") == 0) {
		expect_register(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		int fd = encode_fpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rn = encode_gpr(t);
		emit32(encode_ucvtf(sf, ftype, fd, rn));
		return;
	}

	if (strcasecmp(mnemonic, "fcvtzs") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		int fn = encode_fpr(t);
		emit32(encode_fcvtzs(sf, ftype, rd, fn));
		return;
	}

	if (strcasecmp(mnemonic, "fcvtzu") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		int fn = encode_fpr(t);
		emit32(encode_fcvtzu(sf, ftype, rd, fn));
		return;
	}

	if (strcasecmp(mnemonic, "fcvt") == 0) {
		expect_register(t);
		int dst_type = t->reg_width == 64 ? 1 : 0;
		int fd = encode_fpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int fn = encode_fpr(t);
		if (dst_type) {
			emit32(encode_fcvt_d_s(fd, fn));
		} else {
			emit32(encode_fcvt_s_d(fd, fn));
		}
		return;
	}

	if (strcasecmp(mnemonic, "fmov") == 0) {
		expect_register(t);
		int is_fp_dst = t->reg_type == REG_FP;
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd = is_fp_dst ? encode_fpr(t) : encode_gpr(t);
		t = expect_comma(t->next);
		expect_register(t);
		int rn = encode_gpr(t);
		emit32(encode_fmov_gpr_to_fpr(sf, rd, rn));
		return;
	}

	(void)insn;
	error_tok(tok, "unknown instruction: %s", mnemonic);
}

static void emit_literal_pool(void) {
	int count = literal_pool_count();
	if (count == 0) {
		return;
	}

	section_align(&text_section, 3);

	LiteralEntry **entries = calloc(count, sizeof(LiteralEntry *));
	int idx = count - 1;
	for (LiteralEntry *e = literal_pool_get_list(); e; e = e->next) {
		entries[idx--] = e;
	}

	for (int i = 0; i < count; i++) {
		LiteralEntry *e = entries[i];
		if (e->symbol) {
			Symbol *sym = symtab_add(e->symbol);
			int sym_idx = symtab_get_index(sym);
			reloc_add(SECTION_TEXT, text_section.size, R_AARCH64_ABS64, sym_idx, 0);
			section_emit64(&text_section, 0);
		} else {
			section_emit64(&text_section, e->value);
		}
	}

	free(entries);
}

void pass2(Token *tok) {
	section_init(&text_section);
	section_init(&data_section);
	bss_size = 0;
	reloc_init();

	current_section = SECTION_TEXT;

	while (tok->kind != TOK_EOF) {
		if (tok->kind == TOK_NEWLINE) {
			tok = tok->next;
			continue;
		}

		if (tok->kind == TOK_LABEL) {
			tok = tok->next;
			continue;
		}

		if (tok->kind == TOK_IDENT && tok->str[0] == '.') {
			handle_directive_p2(tok);
			tok = skip_to_newline(tok);
			if (tok->kind == TOK_NEWLINE) {
				tok = tok->next;
			}
			continue;
		}

		if (tok->kind == TOK_IDENT) {
			handle_instruction(tok);
			tok = skip_to_newline(tok);
			if (tok->kind == TOK_NEWLINE) {
				tok = tok->next;
			}
			continue;
		}

		tok = tok->next;
	}

	emit_literal_pool();
}
