#include "as.h"

static int current_section;
static uint64_t lc_text;
static uint64_t lc_data;
static uint64_t lc_bss;

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
		Token *name = expect_ident(tok->next);
		Symbol *sym = symtab_add(name->str);
		symtab_set_binding(sym, STB_GLOBAL);
		return;
	}

	if (strcmp(dir, ".local") == 0) {
		Token *name = expect_ident(tok->next);
		Symbol *sym = symtab_add(name->str);
		symtab_set_binding(sym, STB_LOCAL);
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
