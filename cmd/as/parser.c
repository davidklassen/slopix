#include "as.h"
#include <strings.h>

static int current_section;

// Macro collection state
static bool collecting_macro = false;
static char *macro_name = NULL;
static int macro_num_params = 0;
static char *macro_params[16];
static Token *macro_body_head = NULL;
static Token *macro_body_tail = NULL;
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

static int log2_of(uint64_t n) {
	if (n == 0 || (n & (n - 1)) != 0) {
		return -1;
	}
	int power = 0;
	while (n > 1) {
		n >>= 1;
		power++;
	}
	return power;
}

static int64_t parse_expr(Token **tok);

static int64_t parse_primary(Token **tok) {
	Token *t = *tok;
	if (t->kind == TOK_NUMBER) {
		*tok = t->next;
		return t->val;
	}
	if (t->kind == TOK_IDENT) {
		Symbol *sym = symtab_lookup(t->str);
		if (!sym || !sym->defined) {
			error_tok(t, "undefined symbol '%s'", t->str);
		}
		*tok = t->next;
		return (int64_t)sym->value;
	}
	if (t->kind == TOK_LPAREN) {
		*tok = t->next;
		int64_t val = parse_expr(tok);
		if ((*tok)->kind != TOK_RPAREN) {
			error_tok(*tok, "expected ')'");
		}
		*tok = (*tok)->next;
		return val;
	}
	error_tok(t, "expected number, symbol, or '('");
	return 0;
}

static int64_t parse_shift_expr(Token **tok) {
	int64_t val = parse_primary(tok);
	while ((*tok)->kind == TOK_LSHIFT) {
		*tok = (*tok)->next;
		val = val << parse_primary(tok);
	}
	return val;
}

static int64_t parse_or_expr(Token **tok) {
	int64_t val = parse_shift_expr(tok);
	while ((*tok)->kind == TOK_PIPE) {
		*tok = (*tok)->next;
		val = val | parse_shift_expr(tok);
	}
	return val;
}

static int64_t parse_expr(Token **tok) {
	return parse_or_expr(tok);
}

static Token *skip_to_newline(Token *tok) {
	while (tok->kind != TOK_NEWLINE && tok->kind != TOK_EOF) {
		tok = tok->next;
	}
	return tok;
}

static int find_param(Macro *m, const char *name) {
	for (int i = 0; i < m->num_params; i++) {
		if (strcmp(m->params[i], name) == 0) {
			return i;
		}
	}
	return -1;
}

static Token *expand_macro(Macro *m, Token **args, int nargs, int line_no) {
	Token dummy = {};
	Token *tail = &dummy;

	for (Token *t = m->body; t; t = t->next) {
		if (t->kind == TOK_BACKSLASH && t->next) {
			Token *param_tok = t->next;
			const char *param_name = param_tok->str;
			int idx = find_param(m, param_name);
			if (idx >= 0 && idx < nargs) {
				Token *arg = args[idx];
				bool make_label = (param_tok->kind == TOK_LABEL);
				if (!make_label && param_tok->kind == TOK_IDENT) {
					make_label = param_tok->next &&
						     param_tok->next->kind == TOK_COLON;
				}
				if (make_label) {
					Token *label = clone_token(arg);
					label->kind = TOK_LABEL;
					label->line_no = line_no;
					tail = tail->next = label;
					if (param_tok->kind == TOK_LABEL) {
						t = param_tok;
					} else {
						t = param_tok->next;
					}
				} else {
					Token *cloned = clone_token(arg);
					cloned->line_no = line_no;
					tail = tail->next = cloned;
					t = param_tok;
				}
				continue;
			}
		}
		Token *cloned = clone_token(t);
		cloned->line_no = line_no;
		tail = tail->next = cloned;
	}
	return dummy.next;
}

static Token *parse_macro_args(Token *tok, Token **args, int max_args) {
	int nargs = 0;
	while (tok->kind != TOK_NEWLINE && tok->kind != TOK_EOF &&
	       nargs < max_args) {
		if (tok->kind == TOK_COMMA) {
			tok = tok->next;
			continue;
		}
		args[nargs++] = tok;
		tok = tok->next;
	}
	return tok;
}

static void splice_tokens(Token *head, Token *after) {
	if (!head) {
		return;
	}
	Token *last = head;
	while (last->next) {
		last = last->next;
	}
	last->next = after;
}

static int64_t get_imm_value(Token *t) {
	if (t->kind == TOK_NUMBER) {
		return t->val;
	}
	if (t->kind == TOK_IDENT) {
		Symbol *sym = symtab_lookup(t->str);
		if (sym && sym->defined) {
			return (int64_t)sym->value;
		}
	}
	return t->val;
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
				sym->explicit_local = 1;
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

	if (strcmp(dir, ".ascii") == 0 || strcmp(dir, ".asciz") == 0 ||
	    strcmp(dir, ".string") == 0) {
		int add_nul = (strcmp(dir, ".ascii") != 0);
		Token *t = tok->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_STRING) {
				advance_lc(strlen(t->str) + (add_nul ? 1 : 0));
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

	if (strcmp(dir, ".xword") == 0 || strcmp(dir, ".quad") == 0) {
		Token *t = tok->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_NUMBER || t->kind == TOK_IDENT) {
				advance_lc(8);
				t = t->next;
				// Skip any operators and their operands (e.g., + TABLE_FLAGS)
				while (t->kind == TOK_PLUS || t->kind == TOK_MINUS ||
				       t->kind == TOK_PIPE) {
					t = t->next;
					if (t->kind == TOK_NUMBER || t->kind == TOK_IDENT) {
						t = t->next;
					}
				}
				continue;
			}
			t = t->next;
		}
		return;
	}

	if (strcmp(dir, ".zero") == 0 || strcmp(dir, ".space") == 0 ||
	    strcmp(dir, ".skip") == 0) {
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

	if (strcmp(dir, ".equ") == 0) {
		Token *t = tok->next;
		if (t->kind != TOK_IDENT) {
			error_tok(t, "expected symbol name after .equ");
		}
		char *name = t->str;
		t = t->next;
		if (t->kind == TOK_COMMA) {
			t = t->next;
		}
		int64_t value = parse_expr(&t);
		Symbol *sym = symtab_add(name);
		sym->section = SECTION_NONE;
		sym->value = (uint64_t)value;
		sym->defined = 1;
		return;
	}

	if (strcmp(dir, ".fill") == 0) {
		Token *t = tok->next;
		int64_t count = (t->kind == TOK_NUMBER) ? t->val : 0;
		t = t->next;
		if (t->kind == TOK_COMMA) {
			t = t->next;
		}
		int64_t size = (t->kind == TOK_NUMBER) ? t->val : 0;
		advance_lc(count * size);
		return;
	}

	if (strcmp(dir, ".balign") == 0) {
		Token *t = tok->next;
		int64_t boundary;
		if (t->kind == TOK_NUMBER) {
			boundary = t->val;
		} else if (t->kind == TOK_IDENT) {
			Symbol *sym = symtab_lookup(t->str);
			if (!sym || !sym->defined) {
				error_tok(t, "undefined symbol '%s'", t->str);
			}
			boundary = (int64_t)sym->value;
		} else {
			error_tok(t, "expected alignment value");
			return;
		}
		int power = log2_of((uint64_t)boundary);
		if (power < 0) {
			error_tok(t, "alignment must be a power of 2");
		}
		align_lc(power);
		return;
	}

	if (strcmp(dir, ".section") == 0) {
		Token *t = tok->next;
		if (t->kind == TOK_IDENT || t->kind == TOK_LABEL) {
			const char *name = t->str;
			if (strncmp(name, ".text", 5) == 0) {
				current_section = SECTION_TEXT;
				free(text_section_name);
				text_section_name = strdup(name);
				text_section_is_code = true;
			} else if (strcmp(name, ".data") == 0) {
				current_section = SECTION_DATA;
			} else if (strcmp(name, ".bss") == 0) {
				current_section = SECTION_BSS;
			} else {
				current_section = SECTION_TEXT;
				free(text_section_name);
				text_section_name = strdup(name);
				text_section_is_code = false;
			}
		}
		return;
	}

	if (strcmp(dir, ".macro") == 0) {
		Token *t = tok->next;
		if (t->kind != TOK_IDENT) {
			error_tok(t, "expected macro name");
		}
		macro_name = strdup(t->str);
		macro_num_params = 0;
		t = t->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_IDENT) {
				macro_params[macro_num_params++] = strdup(t->str);
			}
			t = t->next;
		}
		macro_body_head = NULL;
		macro_body_tail = NULL;
		collecting_macro = true;
		return;
	}

	if (strcmp(dir, ".endm") == 0) {
		if (!collecting_macro) {
			error_tok(tok, ".endm without .macro");
		}
		macro_define(macro_name, macro_num_params, macro_params, macro_body_head);
		free(macro_name);
		for (int i = 0; i < macro_num_params; i++) {
			free(macro_params[i]);
		}
		macro_name = NULL;
		macro_num_params = 0;
		macro_body_head = NULL;
		macro_body_tail = NULL;
		collecting_macro = false;
		return;
	}

	if (strcmp(dir, ".file") == 0 || strcmp(dir, ".loc") == 0 ||
	    strcmp(dir, ".ident") == 0 ||
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
	macro_init();
	current_section = SECTION_TEXT;
	text_section_name = strdup(".text");
	text_section_is_code = true;
	lc_text = 0;
	lc_data = 0;
	lc_bss = 0;
	collecting_macro = false;

	while (tok->kind != TOK_EOF) {
		if (collecting_macro) {
			if (tok->kind == TOK_IDENT && tok->str[0] == '.' &&
			    strcmp(tok->str, ".endm") == 0) {
				handle_directive(tok);
				tok = skip_to_newline(tok);
				if (tok->kind == TOK_NEWLINE) {
					tok = tok->next;
				}
				continue;
			}
			Token *line_start = tok;
			while (tok->kind != TOK_NEWLINE && tok->kind != TOK_EOF) {
				Token *cloned = clone_token(tok);
				if (macro_body_tail) {
					macro_body_tail->next = cloned;
				} else {
					macro_body_head = cloned;
				}
				macro_body_tail = cloned;
				tok = tok->next;
			}
			if (tok->kind == TOK_NEWLINE) {
				Token *nl = clone_token(tok);
				if (macro_body_tail) {
					macro_body_tail->next = nl;
				} else {
					macro_body_head = nl;
				}
				macro_body_tail = nl;
				tok = tok->next;
			}
			continue;
		}

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
			Macro *m = macro_lookup(tok->str);
			if (m) {
				Token *args[16];
				Token *after = parse_macro_args(tok->next, args, m->num_params);
				Token *expanded = expand_macro(m, args, m->num_params, tok->line_no);
				if (expanded) {
					splice_tokens(expanded, after);
					tok = expanded;
				} else {
					tok = after;
				}
				continue;
			}
		}

		if (is_ldr_literal(tok)) {
			Token *t = tok->next;
			t = t->next;
			t = t->next;
			t = t->next;
			if (t->kind == TOK_NUMBER) {
				literal_pool_add_value((uint64_t)t->val);
			} else if (t->kind == TOK_IDENT) {
				Symbol *sym = symtab_lookup(t->str);
				if (sym && sym->defined && sym->type == STT_NOTYPE &&
				    sym->section == 0) {
					literal_pool_add_value(sym->value);
				} else {
					literal_pool_add_symbol(t->str);
				}
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

	if (strcmp(dir, ".section") == 0) {
		Token *t = tok->next;
		if (t->kind == TOK_IDENT || t->kind == TOK_LABEL) {
			const char *name = t->str;
			if (strncmp(name, ".text", 5) == 0) {
				current_section = SECTION_TEXT;
			} else if (strcmp(name, ".data") == 0) {
				current_section = SECTION_DATA;
			} else if (strcmp(name, ".bss") == 0) {
				current_section = SECTION_BSS;
			} else {
				current_section = SECTION_TEXT;
			}
		}
		return;
	}
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

	if (strcmp(dir, ".ascii") == 0 || strcmp(dir, ".asciz") == 0 ||
	    strcmp(dir, ".string") == 0) {
		int add_nul = (strcmp(dir, ".ascii") != 0);
		Token *t = tok->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_STRING) {
				if (sec) {
					for (char *p = t->str; *p; p++) {
						section_emit8(sec, (uint8_t)*p);
					}
					if (add_nul) {
						section_emit8(sec, 0);
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

	if (strcmp(dir, ".xword") == 0 || strcmp(dir, ".quad") == 0) {
		Token *t = tok->next;
		while (t->kind != TOK_NEWLINE && t->kind != TOK_EOF) {
			if (t->kind == TOK_NUMBER) {
				if (sec) {
					section_emit64(sec, (uint64_t)t->val);
				}
				t = t->next;
			} else if (t->kind == TOK_IDENT) {
				Symbol *sym = symtab_lookup(t->str);
				t = t->next;

				// Check if this is an absolute symbol (like from .equ)
				if (sym && sym->defined && sym->section == SECTION_NONE) {
					// Absolute constant - evaluate the full expression
					int64_t value = (int64_t)sym->value;

					// Handle + or - followed by another symbol or number
					while (t->kind == TOK_PLUS || t->kind == TOK_MINUS ||
					       t->kind == TOK_PIPE) {
						int op = t->kind;
						t = t->next;
						int64_t operand = 0;
						if (t->kind == TOK_NUMBER) {
							operand = t->val;
							t = t->next;
						} else if (t->kind == TOK_IDENT) {
							Symbol *sym2 = symtab_lookup(t->str);
							if (sym2 && sym2->defined &&
							    sym2->section == SECTION_NONE) {
								operand = (int64_t)sym2->value;
							}
							t = t->next;
						}
						if (op == TOK_PLUS)
							value += operand;
						else if (op == TOK_MINUS)
							value -= operand;
						else if (op == TOK_PIPE)
							value |= operand;
					}

					if (sec) {
						section_emit64(sec, (uint64_t)value);
					}
				} else {
					// Symbol reference - emit relocation
					int sym_idx = symtab_get_index(symtab_add(sym ? sym->name : ""));
					int64_t addend = 0;

					// Parse addend: +NUMBER, -NUMBER, or +SYMBOL (absolute constant)
					while (t->kind == TOK_PLUS || t->kind == TOK_MINUS) {
						int op = t->kind;
						t = t->next;
						int64_t operand = 0;
						if (t->kind == TOK_NUMBER) {
							operand = t->val;
							t = t->next;
						} else if (t->kind == TOK_IDENT) {
							Symbol *sym2 = symtab_lookup(t->str);
							if (sym2 && sym2->defined &&
							    sym2->section == SECTION_NONE) {
								operand = (int64_t)sym2->value;
							}
							t = t->next;
						}
						if (op == TOK_PLUS)
							addend += operand;
						else
							addend -= operand;
					}

					if (sec) {
						reloc_add(current_section, sec->size, R_AARCH64_ABS64, sym_idx, addend);
						section_emit64(sec, 0);
					}
				}
			} else {
				t = t->next;
			}
		}
		return;
	}

	if (strcmp(dir, ".zero") == 0 || strcmp(dir, ".space") == 0 ||
	    strcmp(dir, ".skip") == 0) {
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

	if (strcmp(dir, ".fill") == 0) {
		Token *t = tok->next;
		int64_t count = t->val;
		t = t->next;
		if (t->kind == TOK_COMMA) {
			t = t->next;
		}
		int64_t size = t->val;
		t = t->next;
		if (t->kind == TOK_COMMA) {
			t = t->next;
		}
		int64_t value = (t->kind == TOK_NUMBER) ? t->val : 0;
		if (!sec) {
			return;
		}
		for (int64_t i = 0; i < count; i++) {
			for (int64_t j = 0; j < size; j++) {
				section_emit8(sec, (uint8_t)(value >> (j * 8)));
			}
		}
		return;
	}

	if (strcmp(dir, ".balign") == 0) {
		Token *t = tok->next;
		int64_t boundary;
		if (t->kind == TOK_NUMBER) {
			boundary = t->val;
		} else if (t->kind == TOK_IDENT) {
			Symbol *sym = symtab_lookup(t->str);
			boundary = (int64_t)sym->value;
		} else {
			return;
		}
		int power = log2_of((uint64_t)boundary);
		if (sec && power >= 0) {
			section_align(sec, power);
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

	if (strcasecmp(mnemonic, "isb") == 0) {
		emit32(encode_isb());
		return;
	}

	if (strcasecmp(mnemonic, "dsb") == 0) {
		if (t->kind == TOK_IDENT && strcasecmp(t->str, "sy") == 0) {
			emit32(encode_dsb_sy());
			return;
		}
		error_tok(t, "only 'dsb sy' supported");
	}

	if (strcasecmp(mnemonic, "tlbi") == 0) {
		if (t->kind == TOK_IDENT && strcasecmp(t->str, "vmalle1") == 0) {
			emit32(encode_tlbi_vmalle1());
			return;
		}
		error_tok(t, "only 'tlbi vmalle1' supported");
	}

	if (strcasecmp(mnemonic, "eret") == 0) {
		emit32(encode_eret());
		return;
	}

	if (strcasecmp(mnemonic, "wfe") == 0) {
		emit32(encode_wfe());
		return;
	}

	if (strcasecmp(mnemonic, "msr") == 0) {
		if (t->kind == TOK_IDENT) {
			if (strcasecmp(t->str, "daifset") == 0) {
				t = expect_comma(t->next);
				t = skip_hash(t);
				emit32(encode_msr_pstate(3, 6, (int)t->val));
				return;
			}
			if (strcasecmp(t->str, "daifclr") == 0) {
				t = expect_comma(t->next);
				t = skip_hash(t);
				emit32(encode_msr_pstate(3, 7, (int)t->val));
				return;
			}
			if (strcasecmp(t->str, "spsel") == 0) {
				t = expect_comma(t->next);
				t = skip_hash(t);
				emit32(encode_msr_pstate(0, 5, (int)t->val));
				return;
			}
			int sysreg = encode_sysreg(t->str);
			if (sysreg < 0)
				error_tok(t, "unknown system register");
			t = expect_comma(t->next);
			expect_register(t);
			emit32(encode_msr_reg(sysreg, encode_gpr(t)));
			return;
		}
		error_tok(t, "expected system register");
	}

	if (strcasecmp(mnemonic, "mrs") == 0) {
		expect_register(t);
		int rt = encode_gpr(t);
		t = expect_comma(t->next);
		int sysreg = encode_sysreg(t->str);
		if (sysreg < 0)
			error_tok(t, "unknown system register");
		emit32(encode_mrs(sysreg, rt));
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
		if (t->kind == TOK_IDENT) {
			Symbol *sym = symtab_add(t->str);
			int sym_idx = symtab_get_index(sym);
			emit_reloc(R_AARCH64_ADR_PREL_LO21, sym_idx, 0);
		}
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
			int64_t val = get_imm_value(t);
			if (val < 0) {
				int imm12 = encode_imm12(-val, &shift);
				if (imm12 < 0) {
					error_tok(t, "immediate value %lld not encodable as 12-bit immediate", (long long)val);
				}
				emit32(encode_sub_imm(sf, rd, rn, imm12, shift ? 1 : 0));
			} else {
				int imm12 = encode_imm12(val, &shift);
				if (imm12 < 0) {
					error_tok(t, "immediate value %lld not encodable as 12-bit immediate", (long long)val);
				}
				emit32(encode_add_imm(sf, rd, rn, imm12, shift ? 1 : 0));
			}
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
			int64_t val = get_imm_value(t);
			if (val < 0) {
				int imm12 = encode_imm12(-val, &shift);
				if (imm12 < 0) {
					error_tok(t, "immediate value %lld not encodable as 12-bit immediate", (long long)val);
				}
				emit32(encode_add_imm(sf, rd, rn, imm12, shift ? 1 : 0));
			} else {
				int imm12 = encode_imm12(val, &shift);
				if (imm12 < 0) {
					error_tok(t, "immediate value %lld not encodable as 12-bit immediate", (long long)val);
				}
				emit32(encode_sub_imm(sf, rd, rn, imm12, shift ? 1 : 0));
			}
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
			int64_t val = t->val;
			if (val < 0) {
				int imm12 = encode_imm12(-val, &shift);
				if (imm12 < 0) {
					error_tok(t, "immediate value %lld not encodable as 12-bit immediate", (long long)t->val);
				}
				emit32(encode_subs_imm(sf, rd, rn, imm12, shift ? 1 : 0));
			} else {
				int imm12 = encode_imm12(val, &shift);
				if (imm12 < 0) {
					error_tok(t, "immediate value %lld not encodable as 12-bit immediate", (long long)t->val);
				}
				emit32(encode_adds_imm(sf, rd, rn, imm12, shift ? 1 : 0));
			}
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
			int64_t val = t->val;
			if (val < 0) {
				int imm12 = encode_imm12(-val, &shift);
				if (imm12 < 0) {
					error_tok(t, "immediate value %lld not encodable as 12-bit immediate", (long long)t->val);
				}
				emit32(encode_adds_imm(sf, rd, rn, imm12, shift ? 1 : 0));
			} else {
				int imm12 = encode_imm12(val, &shift);
				if (imm12 < 0) {
					error_tok(t, "immediate value %lld not encodable as 12-bit immediate", (long long)t->val);
				}
				emit32(encode_subs_imm(sf, rd, rn, imm12, shift ? 1 : 0));
			}
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
		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_and_reg(sf, rd, rn, rm));
		} else {
			uint64_t val = (uint64_t)t->val;
			LogicalImm imm;
			if (!encode_logical_imm(sf, val, &imm)) {
				error_tok(t, "immediate 0x%llx not encodable as logical immediate", (unsigned long long)val);
			}
			emit32(encode_and_imm(sf, rd, rn, imm.n, imm.imms, imm.immr));
		}
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
		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_orr_reg(sf, rd, rn, rm));
		} else {
			uint64_t val = (uint64_t)t->val;
			LogicalImm imm;
			if (!encode_logical_imm(sf, val, &imm)) {
				error_tok(t, "immediate 0x%llx not encodable as logical immediate", (unsigned long long)val);
			}
			emit32(encode_orr_imm(sf, rd, rn, imm.n, imm.imms, imm.immr));
		}
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
		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_eor_reg(sf, rd, rn, rm));
		} else {
			uint64_t val = (uint64_t)t->val;
			LogicalImm imm;
			if (!encode_logical_imm(sf, val, &imm)) {
				error_tok(t, "immediate 0x%llx not encodable as logical immediate", (unsigned long long)val);
			}
			emit32(encode_eor_imm(sf, rd, rn, imm.n, imm.imms, imm.immr));
		}
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
		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_ands_reg(sf, rd, rn, rm));
		} else {
			uint64_t val = (uint64_t)t->val;
			LogicalImm imm;
			if (!encode_logical_imm(sf, val, &imm)) {
				error_tok(t, "immediate 0x%llx not encodable as logical immediate", (unsigned long long)val);
			}
			emit32(encode_ands_imm(sf, rd, rn, imm.n, imm.imms, imm.immr));
		}
		return;
	}

	if (strcasecmp(mnemonic, "tst") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rn = encode_gpr(t);
		t = expect_comma(t->next);
		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_tst_reg(sf, rn, rm));
		} else {
			uint64_t val = (uint64_t)t->val;
			LogicalImm imm;
			if (!encode_logical_imm(sf, val, &imm)) {
				error_tok(t, "immediate 0x%llx not encodable as logical immediate", (unsigned long long)val);
			}
			emit32(encode_tst_imm(sf, rn, imm.n, imm.imms, imm.immr));
		}
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
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_lsl_reg(sf, rd, rn, rm));
		} else {
			t = skip_hash(t);
			int shift = (int)t->val;
			emit32(encode_lsl_imm(sf, rd, rn, shift));
		}
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
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_lsr_reg(sf, rd, rn, rm));
		} else {
			t = skip_hash(t);
			int shift = (int)t->val;
			emit32(encode_lsr_imm(sf, rd, rn, shift));
		}
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
		if (t->kind == TOK_REGISTER) {
			int rm = encode_gpr(t);
			emit32(encode_asr_reg(sf, rd, rn, rm));
		} else {
			t = skip_hash(t);
			int shift = (int)t->val;
			emit32(encode_asr_imm(sf, rd, rn, shift));
		}
		return;
	}

	if (strcasecmp(mnemonic, "mov") == 0) {
		expect_register(t);
		int sf = t->reg_width == 64 ? 1 : 0;
		int rd_is_sp = (t->reg_type == REG_SP);
		int rd = rd_is_sp ? 31 : encode_gpr(t);
		t = expect_comma(t->next);
		t = skip_hash(t);
		if (t->kind == TOK_REGISTER) {
			if (t->reg_type == REG_SP) {
				// mov rd, sp -> add rd, sp, #0
				emit32(encode_add_imm(sf, rd, 31, 0, 0));
			} else if (rd_is_sp) {
				// mov sp, rm -> add sp, rm, #0
				int rm = encode_gpr(t);
				emit32(encode_add_imm(sf, 31, rm, 0, 0));
			} else {
				int rm = encode_gpr(t);
				emit32(encode_mov_reg(sf, rd, rm));
			}
		} else {
			uint64_t val;
			if (t->kind == TOK_IDENT) {
				Symbol *sym = symtab_lookup(t->str);
				if (!sym || !sym->defined)
					error_tok(t, "undefined symbol");
				val = sym->value;
			} else {
				val = (uint64_t)t->val;
			}
			int hw = 0;
			uint16_t imm16 = 0;
			// Find which 16-bit chunk has the value
			if ((val & 0xFFFFULL) == val) {
				hw = 0;
				imm16 = (uint16_t)val;
			} else if ((val & 0xFFFF0000ULL) == val) {
				hw = 1;
				imm16 = (uint16_t)(val >> 16);
			} else if ((val & 0xFFFF00000000ULL) == val) {
				hw = 2;
				imm16 = (uint16_t)(val >> 32);
			} else if ((val & 0xFFFF000000000000ULL) == val) {
				hw = 3;
				imm16 = (uint16_t)(val >> 48);
			} else if (t->val < 0 && t->val >= -65536) {
				// Small negative: use MOVN
				emit32(encode_movn(sf, rd, (int)(~t->val), 0));
				return;
			} else {
				// Multi-chunk value: use MOVZ + MOVK sequence
				imm16 = (uint16_t)(val & 0xFFFF);
				emit32(encode_movz(sf, rd, imm16, 0));
				if (val >> 16) {
					imm16 = (uint16_t)((val >> 16) & 0xFFFF);
					if (imm16)
						emit32(encode_movk(sf, rd, imm16, 1));
				}
				if (val >> 32) {
					imm16 = (uint16_t)((val >> 32) & 0xFFFF);
					if (imm16)
						emit32(encode_movk(sf, rd, imm16, 2));
				}
				if (val >> 48) {
					imm16 = (uint16_t)((val >> 48) & 0xFFFF);
					if (imm16)
						emit32(encode_movk(sf, rd, imm16, 3));
				}
				return;
			}
			emit32(encode_movz(sf, rd, imm16, hw));
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
			if (imm12 < 0) {
				error_tok(t, "immediate value %lld not encodable as 12-bit immediate", (long long)t->val);
			}
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
			if (imm12 < 0) {
				error_tok(t, "immediate value %lld not encodable as 12-bit immediate", (long long)t->val);
			}
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
				// Check if it's a defined constant (.equ)
				Symbol *sym = symtab_lookup(t->str);
				if (sym && sym->defined && sym->type == STT_NOTYPE &&
				    sym->section == 0) {
					entry = literal_pool_add_value(sym->value);
				} else {
					entry = literal_pool_add_symbol(t->str);
				}
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
		int ftype = t->reg_width == 64 ? 1 : 0;
		int rt = t->reg_type == REG_FP ? encode_fpr(t) : encode_gpr(t);
		int is_fp = t->reg_type == REG_FP;
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
			if (is_fp) {
				emit32(encode_stur_fp(ftype, rt, rn, imm));
			} else {
				emit32(encode_stur(ftype, rt, rn, imm));
			}
		}
		return;
	}

	if (strcasecmp(mnemonic, "ldur") == 0) {
		expect_register(t);
		int ftype = t->reg_width == 64 ? 1 : 0;
		int rt = t->reg_type == REG_FP ? encode_fpr(t) : encode_gpr(t);
		int is_fp = t->reg_type == REG_FP;
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
			if (is_fp) {
				emit32(encode_ldur_fp(ftype, rt, rn, imm));
			} else {
				emit32(encode_ldur(ftype, rt, rn, imm));
			}
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

static void check_undefined_symbols(void) {
	for (int i = 0; i < symtab_count(); i++) {
		Symbol *sym = symtab_get(i);
		if (!sym->defined && sym->explicit_local) {
			error("undefined local symbol: '%s'", sym->name);
		}
	}
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
	bss_size = lc_bss;
	text_section_alignment = 2;
	data_section_alignment = 3;
	reloc_init();

	current_section = SECTION_TEXT;
	bool skip_macro_body = false;

	while (tok->kind != TOK_EOF) {
		if (skip_macro_body) {
			if (tok->kind == TOK_IDENT && tok->str[0] == '.' &&
			    strcmp(tok->str, ".endm") == 0) {
				skip_macro_body = false;
			}
			tok = skip_to_newline(tok);
			if (tok->kind == TOK_NEWLINE) {
				tok = tok->next;
			}
			continue;
		}

		if (tok->kind == TOK_NEWLINE) {
			tok = tok->next;
			continue;
		}

		if (tok->kind == TOK_LABEL) {
			tok = tok->next;
			continue;
		}

		if (tok->kind == TOK_IDENT && tok->str[0] == '.') {
			if (strcmp(tok->str, ".macro") == 0) {
				skip_macro_body = true;
				tok = skip_to_newline(tok);
				if (tok->kind == TOK_NEWLINE) {
					tok = tok->next;
				}
				continue;
			}
			handle_directive_p2(tok);
			tok = skip_to_newline(tok);
			if (tok->kind == TOK_NEWLINE) {
				tok = tok->next;
			}
			continue;
		}

		if (tok->kind == TOK_IDENT) {
			Macro *m = macro_lookup(tok->str);
			if (m) {
				Token *args[16];
				Token *after = parse_macro_args(tok->next, args, m->num_params);
				Token *expanded = expand_macro(m, args, m->num_params, tok->line_no);
				if (expanded) {
					splice_tokens(expanded, after);
					tok = expanded;
				} else {
					tok = after;
				}
				continue;
			}
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
	check_undefined_symbols();
}
