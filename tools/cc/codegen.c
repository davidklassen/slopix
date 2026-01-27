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
	error_tok(node->tok, "gen_addr not yet implemented for AArch64");
}

static void load(Type *ty) {
	(void)ty;
	error("load not yet implemented for AArch64");
}

static void store(Type *ty) {
	(void)ty;
	error("store not yet implemented for AArch64");
}

static void cmp_zero(Type *ty) {
	(void)ty;
	error("cmp_zero not yet implemented for AArch64");
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
