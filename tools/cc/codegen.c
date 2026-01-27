#include "chibicc.h"

#define GP_MAX 8
#define FP_MAX 8

static FILE *output_file;
static int depth;
static char *argreg64[] = {"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"};
static char *argreg32[] = {"w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7"};
static Obj *current_fn;

static void gen_expr(Node *node);
static void gen_stmt(Node *node);

__attribute__((format(printf, 1, 2))) static void println(char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(output_file, fmt, ap);
	va_end(ap);
	fprintf(output_file, "\n");
}

static int count(void) {
	static int i = 1;
	return i++;
}

static void push(void) {
	println("  str x0, [sp, #-16]!");
	depth++;
}

static void pop(char *reg) {
	println("  ldr %s, [sp], #16", reg);
	depth--;
}

// Round up `n` to the nearest multiple of `align`.
int align_to(int n, int align) {
	return (n + align - 1) / align * align;
}

static void gen_addr(Node *node) {
	switch (node->kind) {
	case ND_VAR:
		if (node->var->is_local) {
			println("  add x0, x29, #%d", node->var->offset);
		} else {
			error_tok(node->tok, "global variables not yet implemented");
		}
		return;
	case ND_DEREF:
		gen_expr(node->lhs);
		return;
	case ND_COMMA:
		gen_expr(node->lhs);
		gen_addr(node->rhs);
		return;
	default:
		break;
	}
	error_tok(node->tok, "not an lvalue");
}

static void load(Type *ty) {
	// Arrays and structs decay to pointers, no load needed
	if (ty->kind == TY_ARRAY || ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
		return;
	}

	// Integer loads based on size and signedness
	if (ty->size == 1) {
		if (ty->is_unsigned) {
			println("  ldrb w0, [x0]");
		} else {
			println("  ldrsb x0, [x0]");
		}
	} else if (ty->size == 2) {
		if (ty->is_unsigned) {
			println("  ldrh w0, [x0]");
		} else {
			println("  ldrsh x0, [x0]");
		}
	} else if (ty->size == 4) {
		if (ty->is_unsigned) {
			println("  ldr w0, [x0]");
		} else {
			println("  ldrsw x0, [x0]");
		}
	} else {
		println("  ldr x0, [x0]");
	}
}

static void store(Type *ty) {
	pop("x1"); // Address was pushed before evaluating RHS

	// Struct/union copy
	if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
		for (int i = 0; i < ty->size; i++) {
			println("  ldrb w2, [x0, #%d]", i);
			println("  strb w2, [x1, #%d]", i);
		}
		return;
	}

	// Integer stores based on size
	if (ty->size == 1) {
		println("  strb w0, [x1]");
	} else if (ty->size == 2) {
		println("  strh w0, [x1]");
	} else if (ty->size == 4) {
		println("  str w0, [x1]");
	} else {
		println("  str x0, [x1]");
	}
}

static void cmp_zero(Type *ty) {
	(void)ty;
	println("  cmp x0, #0");
}

static void pushf(void) {
	error("pushf not yet implemented for AArch64");
}

static void popf(int reg) {
	(void)reg;
	error("popf not yet implemented for AArch64");
}

static void cast(Type *from, Type *to) {
	(void)from;
	(void)to;
	error("cast not yet implemented for AArch64");
}

static void gen_expr(Node *node) {
	println("  .loc %d %d", node->tok->file->file_no, node->tok->line_no);

	switch (node->kind) {
	case ND_NULL_EXPR:
		return;
	case ND_NUM:
		if (node->val >= 0 && node->val <= 65535) {
			println("  mov x0, #%" PRId64, node->val);
		} else {
			println("  ldr x0, =%" PRId64, node->val);
		}
		return;
	case ND_CAST:
		gen_expr(node->lhs);
		// For now, ignore casts - stub implementation
		return;
	case ND_NEG:
		gen_expr(node->lhs);
		println("  neg x0, x0");
		return;
	case ND_BITNOT:
		gen_expr(node->lhs);
		println("  mvn x0, x0");
		return;
	case ND_NOT:
		gen_expr(node->lhs);
		println("  cmp x0, #0");
		println("  cset x0, eq");
		return;
	case ND_VAR:
		gen_addr(node);
		load(node->ty);
		return;
	case ND_MEMZERO:
		// Zero-initialize a local variable
		println("  add x0, x29, #%d", node->var->offset);
		for (int i = 0; i < node->var->ty->size; i++) {
			println("  strb wzr, [x0, #%d]", i);
		}
		return;
	case ND_ASSIGN:
		gen_addr(node->lhs);
		push();
		gen_expr(node->rhs);
		store(node->ty);
		return;
	case ND_COMMA:
		gen_expr(node->lhs);
		gen_expr(node->rhs);
		return;
	case ND_STMT_EXPR:
		for (Node *n = node->body; n; n = n->next) {
			gen_stmt(n);
		}
		return;
	case ND_LOGAND: {
		int c = count();
		gen_expr(node->lhs);
		cmp_zero(node->lhs->ty);
		println("  b.eq .L.false.%d", c);
		gen_expr(node->rhs);
		cmp_zero(node->rhs->ty);
		println("  b.eq .L.false.%d", c);
		println("  mov x0, #1");
		println("  b .L.end.%d", c);
		println(".L.false.%d:", c);
		println("  mov x0, #0");
		println(".L.end.%d:", c);
		return;
	}
	case ND_LOGOR: {
		int c = count();
		gen_expr(node->lhs);
		cmp_zero(node->lhs->ty);
		println("  b.ne .L.true.%d", c);
		gen_expr(node->rhs);
		cmp_zero(node->rhs->ty);
		println("  b.ne .L.true.%d", c);
		println("  mov x0, #0");
		println("  b .L.end.%d", c);
		println(".L.true.%d:", c);
		println("  mov x0, #1");
		println(".L.end.%d:", c);
		return;
	}
	case ND_COND: {
		int c = count();
		gen_expr(node->cond);
		cmp_zero(node->cond->ty);
		println("  b.eq .L.else.%d", c);
		gen_expr(node->then);
		println("  b .L.end.%d", c);
		println(".L.else.%d:", c);
		gen_expr(node->els);
		println(".L.end.%d:", c);
		return;
	}
	case ND_ADDR:
		gen_addr(node->lhs);
		return;
	case ND_DEREF:
		gen_expr(node->lhs);
		load(node->ty);
		return;
	case ND_ADD:
	case ND_SUB:
	case ND_MUL:
	case ND_DIV:
	case ND_MOD:
	case ND_BITAND:
	case ND_BITOR:
	case ND_BITXOR:
	case ND_SHL:
	case ND_SHR:
	case ND_EQ:
	case ND_NE:
	case ND_LT:
	case ND_LE:
		gen_expr(node->rhs);
		push();
		gen_expr(node->lhs);
		pop("x1");
		// x0 = LHS, x1 = RHS

		switch (node->kind) {
		case ND_ADD:
			println("  add x0, x0, x1");
			break;
		case ND_SUB:
			println("  sub x0, x0, x1");
			break;
		case ND_MUL:
			println("  mul x0, x0, x1");
			break;
		case ND_DIV:
			if (node->ty->is_unsigned) {
				println("  udiv x0, x0, x1");
			} else {
				println("  sdiv x0, x0, x1");
			}
			break;
		case ND_MOD:
			if (node->ty->is_unsigned) {
				println("  udiv x2, x0, x1");
			} else {
				println("  sdiv x2, x0, x1");
			}
			println("  msub x0, x2, x1, x0");
			break;
		case ND_BITAND:
			println("  and x0, x0, x1");
			break;
		case ND_BITOR:
			println("  orr x0, x0, x1");
			break;
		case ND_BITXOR:
			println("  eor x0, x0, x1");
			break;
		case ND_SHL:
			println("  lsl x0, x0, x1");
			break;
		case ND_SHR:
			if (node->lhs->ty->is_unsigned) {
				println("  lsr x0, x0, x1");
			} else {
				println("  asr x0, x0, x1");
			}
			break;
		case ND_EQ:
			println("  cmp x0, x1");
			println("  cset x0, eq");
			break;
		case ND_NE:
			println("  cmp x0, x1");
			println("  cset x0, ne");
			break;
		case ND_LT:
			println("  cmp x0, x1");
			println("  cset x0, lt");
			break;
		case ND_LE:
			println("  cmp x0, x1");
			println("  cset x0, le");
			break;
		default:
			break;
		}
		return;
	default:
		break;
	}

	error_tok(node->tok, "expression type not yet implemented for AArch64");
}

static void gen_stmt(Node *node) {
	println("  .loc %d %d", node->tok->file->file_no, node->tok->line_no);

	switch (node->kind) {
	case ND_RETURN:
		if (node->lhs) {
			gen_expr(node->lhs);
		}
		println("  b .L.return.%s", current_fn->name);
		return;
	case ND_EXPR_STMT:
		gen_expr(node->lhs);
		return;
	case ND_BLOCK:
		for (Node *n = node->body; n; n = n->next) {
			gen_stmt(n);
		}
		return;
	case ND_IF: {
		int c = count();
		gen_expr(node->cond);
		cmp_zero(node->cond->ty);
		println("  b.eq .L.else.%d", c);
		gen_stmt(node->then);
		println("  b .L.end.%d", c);
		println(".L.else.%d:", c);
		if (node->els) {
			gen_stmt(node->els);
		}
		println(".L.end.%d:", c);
		return;
	}
	case ND_FOR: {
		int c = count();
		if (node->init) {
			gen_stmt(node->init);
		}
		println(".L.begin.%d:", c);
		if (node->cond) {
			gen_expr(node->cond);
			cmp_zero(node->cond->ty);
			println("  b.eq %s", node->brk_label);
		}
		gen_stmt(node->then);
		println("%s:", node->cont_label);
		if (node->inc) {
			gen_expr(node->inc);
		}
		println("  b .L.begin.%d", c);
		println("%s:", node->brk_label);
		return;
	}
	case ND_GOTO:
		println("  b %s", node->unique_label);
		return;
	case ND_LABEL:
		println("%s:", node->unique_label);
		gen_stmt(node->lhs);
		return;
	case ND_DO: {
		int c = count();
		println(".L.begin.%d:", c);
		gen_stmt(node->then);
		println("%s:", node->cont_label);
		gen_expr(node->cond);
		cmp_zero(node->cond->ty);
		println("  b.ne .L.begin.%d", c);
		println("%s:", node->brk_label);
		return;
	}
	case ND_SWITCH:
		gen_expr(node->cond);
		for (Node *n = node->case_next; n; n = n->case_next) {
			if (n->begin == n->end) {
				println("  cmp x0, #%ld", n->begin);
				println("  b.eq %s", n->label);
			} else {
				// Range case (GNU extension: case 1 ... 5)
				int c = count();
				println("  cmp x0, #%ld", n->begin);
				println("  b.lt .L.switch.skip.%d", c);
				println("  cmp x0, #%ld", n->end);
				println("  b.le %s", n->label);
				println(".L.switch.skip.%d:", c);
			}
		}
		if (node->default_case) {
			println("  b %s", node->default_case->label);
		}
		println("  b %s", node->brk_label);
		gen_stmt(node->then);
		println("%s:", node->brk_label);
		return;
	case ND_CASE:
		println("%s:", node->label);
		gen_stmt(node->lhs);
		return;
	default:
		break;
	}

	error_tok(node->tok, "statement type not yet implemented for AArch64");
}

static void assign_lvar_offsets(Obj *prog) {
	for (Obj *fn = prog; fn; fn = fn->next) {
		if (!fn->is_function) {
			continue;
		}

		int bottom = 0;

		for (Obj *var = fn->locals; var; var = var->next) {
			bottom += var->ty->size;
			bottom = align_to(bottom, var->align);
			var->offset = -bottom;
		}

		fn->stack_size = align_to(bottom, 16);
	}
}

static void emit_data(Obj *prog) {
	for (Obj *var = prog; var; var = var->next) {
		if (var->is_function || !var->is_definition) {
			continue;
		}

		// Stub: global variables not yet implemented
		// For simple programs this won't be reached
		if (var->is_static) {
			println("  .local %s", var->name);
		} else {
			println("  .globl %s", var->name);
		}

		println("  .data");
		println("  .type %s, %%object", var->name);
		println("  .size %s, %d", var->name, var->ty->size);
		println("  .align %d", var->align);
		println("%s:", var->name);

		if (var->init_data) {
			for (int i = 0; i < var->ty->size; i++) {
				println("  .byte %d", var->init_data[i]);
			}
		} else {
			println("  .zero %d", var->ty->size);
		}
	}
}

static void emit_text(Obj *prog) {
	for (Obj *fn = prog; fn; fn = fn->next) {
		if (!fn->is_function || !fn->is_definition) {
			continue;
		}

		if (!fn->is_live) {
			continue;
		}

		if (fn->is_static) {
			println("  .local %s", fn->name);
		} else {
			println("  .globl %s", fn->name);
		}

		println("  .text");
		println("  .type %s, %%function", fn->name);
		println("%s:", fn->name);
		current_fn = fn;

		// Prologue
		println("  stp x29, x30, [sp, #-16]!");
		println("  mov x29, sp");
		if (fn->stack_size > 0) {
			println("  sub sp, sp, #%d", fn->stack_size);
		}

		// Emit code
		gen_stmt(fn->body);
		assert(depth == 0);

		// main() returns 0 if it falls through
		if (strcmp(fn->name, "main") == 0) {
			println("  mov x0, #0");
		}

		// Epilogue
		println(".L.return.%s:", fn->name);
		if (fn->stack_size > 0) {
			println("  add sp, sp, #%d", fn->stack_size);
		}
		println("  ldp x29, x30, [sp], #16");
		println("  ret");
	}
}

void codegen(Obj *prog, FILE *out) {
	output_file = out;

	File **files = get_input_files();
	for (int i = 0; files[i]; i++) {
		println("  .file %d \"%s\"", files[i]->file_no, files[i]->name);
	}

	assign_lvar_offsets(prog);
	emit_data(prog);
	emit_text(prog);
}
