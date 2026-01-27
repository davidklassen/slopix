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

static void pushf(void) {
	println("  str d0, [sp, #-16]!");
	depth++;
}

static void popf(int reg) {
	println("  ldr d%d, [sp], #16", reg);
	depth--;
}

// Round up `n` to the nearest multiple of `align`.
int align_to(int n, int align) {
	return (n + align - 1) / align * align;
}

// Store GP register to stack
static void store_gp(int r, int offset, int sz) {
	switch (sz) {
	case 1:
		println("  strb %s, [x29, #%d]", argreg32[r], offset);
		return;
	case 2:
		println("  strh %s, [x29, #%d]", argreg32[r], offset);
		return;
	case 4:
		println("  str %s, [x29, #%d]", argreg32[r], offset);
		return;
	default:
		println("  str %s, [x29, #%d]", argreg64[r], offset);
		return;
	}
}

// Store FP register to stack
static void store_fp(int r, int offset, int sz) {
	if (sz == 4) {
		println("  str s%d, [x29, #%d]", r, offset);
	} else {
		println("  str d%d, [x29, #%d]", r, offset);
	}
}

// Recursive helper to push args right-to-left
static void push_args2(Node *args, bool first_pass) {
	if (!args) {
		return;
	}
	push_args2(args->next, first_pass);
	if ((first_pass && !args->pass_by_stack) ||
	    (!first_pass && args->pass_by_stack)) {
		return;
	}
	gen_expr(args);
	if (is_flonum(args->ty)) {
		pushf();
	} else {
		push();
	}
}

// Push all arguments, return count of stack args (for cleanup)
static int push_args(Node *node) {
	int stack = 0, gp = 0, fp = 0;

	// First pass: mark which args go on stack
	for (Node *arg = node->args; arg; arg = arg->next) {
		Type *ty = arg->ty;
		if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
			continue; // Defer to later steps
		}
		if (is_flonum(ty)) {
			if (fp++ >= FP_MAX) {
				arg->pass_by_stack = true;
				stack++;
			}
		} else {
			if (gp++ >= GP_MAX) {
				arg->pass_by_stack = true;
				stack++;
			}
		}
	}

	// Ensure 16-byte alignment
	if ((depth + stack) % 2 == 1) {
		println("  str xzr, [sp, #-16]!");
		depth++;
		stack++;
	}

	// Push: stack args first, then register args
	push_args2(node->args, true);
	push_args2(node->args, false);

	return stack;
}

static void gen_addr(Node *node) {
	switch (node->kind) {
	case ND_VAR:
		if (node->var->is_local) {
			println("  add x0, x29, #%d", node->var->offset);
		} else {
			println("  adrp x0, %s", node->var->name);
			println("  add x0, x0, :lo12:%s", node->var->name);
		}
		return;
	case ND_DEREF:
		gen_expr(node->lhs);
		return;
	case ND_COMMA:
		gen_expr(node->lhs);
		gen_addr(node->rhs);
		return;
	case ND_MEMBER:
		gen_addr(node->lhs);
		println("  add x0, x0, #%d", node->member->offset);
		return;
	case ND_ASSIGN:
	case ND_COND:
		if (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION) {
			gen_expr(node);
			return;
		}
		break;
	default:
		break;
	}
	error_tok(node->tok, "not an lvalue");
}

static void load(Type *ty) {
	// Arrays, structs, and functions decay to pointers, no load needed
	if (ty->kind == TY_ARRAY || ty->kind == TY_STRUCT ||
	    ty->kind == TY_UNION || ty->kind == TY_FUNC) {
		return;
	}

	// Float loads
	if (ty->kind == TY_FLOAT) {
		println("  ldr s0, [x0]");
		return;
	}
	if (ty->kind == TY_DOUBLE) {
		println("  ldr d0, [x0]");
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

	// Float stores
	if (ty->kind == TY_FLOAT) {
		println("  str s0, [x1]");
		return;
	}
	if (ty->kind == TY_DOUBLE) {
		println("  str d0, [x1]");
		return;
	}

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
	if (is_flonum(ty)) {
		if (ty->kind == TY_FLOAT) {
			println("  fcmp s0, #0.0");
		} else {
			println("  fcmp d0, #0.0");
		}
	} else {
		println("  cmp x0, #0");
	}
}

enum { I8,
       I16,
       I32,
       I64,
       U8,
       U16,
       U32,
       U64,
       F32,
       F64 };

static int getTypeId(Type *ty) {
	switch (ty->kind) {
	case TY_CHAR:
		return ty->is_unsigned ? U8 : I8;
	case TY_SHORT:
		return ty->is_unsigned ? U16 : I16;
	case TY_INT:
		return ty->is_unsigned ? U32 : I32;
	case TY_LONG:
		return ty->is_unsigned ? U64 : I64;
	case TY_FLOAT:
		return F32;
	case TY_DOUBLE:
		return F64;
	default:
		return U64;
	}
}

// Integer sign/zero extension
static char i32i8[] = "sxtb w0, w0";
static char i32u8[] = "uxtb w0, w0";
static char i32i16[] = "sxth w0, w0";
static char i32u16[] = "uxth w0, w0";
static char i32i64[] = "sxtw x0, w0";
static char u32i64[] = "mov w0, w0";
static char i64i32[] = "sxtw x0, w0";
static char i64u32[] = "mov w0, w0";

// Integer to float
static char i32f32[] = "scvtf s0, w0";
static char i32f64[] = "scvtf d0, w0";
static char i64f32[] = "scvtf s0, x0";
static char i64f64[] = "scvtf d0, x0";
static char u32f32[] = "ucvtf s0, w0";
static char u32f64[] = "ucvtf d0, w0";
static char u64f32[] = "ucvtf s0, x0";
static char u64f64[] = "ucvtf d0, x0";

// Float to integer (with narrowing for small types)
static char f32i8[] = "fcvtzs w0, s0\n  sxtb w0, w0";
static char f32u8[] = "fcvtzs w0, s0\n  uxtb w0, w0";
static char f32i16[] = "fcvtzs w0, s0\n  sxth w0, w0";
static char f32u16[] = "fcvtzs w0, s0\n  uxth w0, w0";
static char f32i32[] = "fcvtzs w0, s0";
static char f32u32[] = "fcvtzu w0, s0";
static char f32i64[] = "fcvtzs x0, s0";
static char f32u64[] = "fcvtzu x0, s0";

static char f64i8[] = "fcvtzs w0, d0\n  sxtb w0, w0";
static char f64u8[] = "fcvtzs w0, d0\n  uxtb w0, w0";
static char f64i16[] = "fcvtzs w0, d0\n  sxth w0, w0";
static char f64u16[] = "fcvtzs w0, d0\n  uxth w0, w0";
static char f64i32[] = "fcvtzs w0, d0";
static char f64u32[] = "fcvtzu w0, d0";
static char f64i64[] = "fcvtzs x0, d0";
static char f64u64[] = "fcvtzu x0, d0";

// Float precision
static char f32f64[] = "fcvt d0, s0";
static char f64f32[] = "fcvt s0, d0";

// clang-format off
static char *cast_table[][10] = {
	// to:   I8      I16     I32      I64     U8      U16     U32      U64     F32     F64
	// I8
	{NULL,   NULL,   NULL,    i32i64, i32u8,  NULL,   NULL,    i32i64, i32f32, i32f64},
	// I16
	{i32i8,  NULL,   NULL,    i32i64, i32u8,  i32u16, NULL,    i32i64, i32f32, i32f64},
	// I32
	{i32i8,  i32i16, NULL,    i32i64, i32u8,  i32u16, i64u32,  i32i64, i32f32, i32f64},
	// I64
	{i32i8,  i32i16, i64i32,  NULL,   i32u8,  i32u16, i64u32,  NULL,   i64f32, i64f64},
	// U8
	{i32i8,  NULL,   NULL,    u32i64, NULL,   NULL,   NULL,    u32i64, i32f32, i32f64},
	// U16
	{i32i8,  i32i16, NULL,    u32i64, i32u8,  NULL,   NULL,    u32i64, i32f32, i32f64},
	// U32
	{i32i8,  i32i16, NULL,    u32i64, i32u8,  i32u16, NULL,    u32i64, u32f32, u32f64},
	// U64
	{i32i8,  i32i16, i64i32,  NULL,   i32u8,  i32u16, i64u32,  NULL,   u64f32, u64f64},
	// F32
	{f32i8,  f32i16, f32i32, f32i64, f32u8,  f32u16, f32u32, f32u64, NULL,   f32f64},
	// F64
	{f64i8,  f64i16, f64i32, f64i64, f64u8,  f64u16, f64u32, f64u64, f64f32, NULL},
};
// clang-format on

static void cast(Type *from, Type *to) {
	if (to->kind == TY_VOID) {
		return;
	}

	if (to->kind == TY_BOOL) {
		cmp_zero(from);
		println("  cset w0, ne");
		return;
	}

	int t1 = getTypeId(from);
	int t2 = getTypeId(to);
	if (cast_table[t1][t2]) {
		println("  %s", cast_table[t1][t2]);
	}
}

static void gen_expr(Node *node) {
	println("  .loc %d %d", node->tok->file->file_no, node->tok->line_no);

	switch (node->kind) {
	case ND_NULL_EXPR:
		return;
	case ND_NUM:
		switch (node->ty->kind) {
		case TY_FLOAT: {
			union {
				float f32;
				uint32_t u32;
			} u = {node->fval};
			println("  mov w0, #%u", u.u32 & 0xFFFF);
			if (u.u32 >> 16) {
				println("  movk w0, #%u, lsl #16",
					(u.u32 >> 16) & 0xFFFF);
			}
			println("  fmov s0, w0");
			return;
		}
		case TY_DOUBLE: {
			union {
				double f64;
				uint64_t u64;
			} u = {node->fval};
			println("  ldr x0, =%" PRIu64, u.u64);
			println("  fmov d0, x0");
			return;
		}
		default:
			if (node->val >= 0 && node->val <= 65535) {
				println("  mov x0, #%" PRId64, node->val);
			} else {
				println("  ldr x0, =%" PRId64, node->val);
			}
			return;
		}
	case ND_CAST:
		gen_expr(node->lhs);
		cast(node->lhs->ty, node->ty);
		return;
	case ND_NEG:
		gen_expr(node->lhs);
		switch (node->ty->kind) {
		case TY_FLOAT:
			println("  fneg s0, s0");
			return;
		case TY_DOUBLE:
			println("  fneg d0, d0");
			return;
		default:
			println("  neg x0, x0");
			return;
		}
	case ND_BITNOT:
		gen_expr(node->lhs);
		println("  mvn x0, x0");
		return;
	case ND_NOT:
		gen_expr(node->lhs);
		cmp_zero(node->lhs->ty);
		println("  cset x0, eq");
		return;
	case ND_VAR:
		gen_addr(node);
		load(node->ty);
		return;
	case ND_MEMBER: {
		gen_addr(node);
		load(node->ty);

		Member *mem = node->member;
		if (mem->is_bitfield) {
			println("  lsl x0, x0, #%d", 64 - mem->bit_width - mem->bit_offset);
			if (mem->ty->is_unsigned) {
				println("  lsr x0, x0, #%d", 64 - mem->bit_width);
			} else {
				println("  asr x0, x0, #%d", 64 - mem->bit_width);
			}
		}
		return;
	}
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

		if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
			Member *mem = node->lhs->member;

			println("  mov x8, x0");

			println("  and x2, x0, #%ld", (1L << mem->bit_width) - 1);
			println("  lsl x2, x2, #%d", mem->bit_offset);

			println("  ldr x0, [sp]");
			load(mem->ty);

			long mask = ((1L << mem->bit_width) - 1) << mem->bit_offset;
			println("  ldr x3, =%ld", mask);
			println("  bic x0, x0, x3");
			println("  orr x0, x0, x2");

			store(node->ty);
			println("  mov x0, x8");
			return;
		}

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
	case ND_FUNCALL: {
		int stack_args = push_args(node);
		gen_expr(node->lhs);	 // Function address -> x0
		println("  mov x9, x0"); // Save to temp register

		// Pop args into registers
		int gp = 0, fp = 0;
		for (Node *arg = node->args; arg; arg = arg->next) {
			Type *ty = arg->ty;
			if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
				continue;
			}
			if (is_flonum(ty)) {
				if (!arg->pass_by_stack && fp < FP_MAX) {
					popf(fp++);
				}
			} else {
				if (!arg->pass_by_stack && gp < GP_MAX) {
					pop(argreg64[gp++]);
				}
			}
		}

		println("  blr x9"); // Indirect call

		// Clean up stack args
		if (stack_args > 0) {
			println("  add sp, sp, #%d", stack_args * 16);
			depth -= stack_args;
		}
		return;
	}
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
		// Float binary operations
		if (is_flonum(node->lhs->ty)) {
			gen_expr(node->rhs);
			pushf();
			gen_expr(node->lhs);
			popf(1); // d1 = RHS, d0 = LHS

			char *s = (node->lhs->ty->kind == TY_FLOAT) ? "s" : "d";

			switch (node->kind) {
			case ND_ADD:
				println("  fadd %s0, %s0, %s1", s, s, s);
				return;
			case ND_SUB:
				println("  fsub %s0, %s0, %s1", s, s, s);
				return;
			case ND_MUL:
				println("  fmul %s0, %s0, %s1", s, s, s);
				return;
			case ND_DIV:
				println("  fdiv %s0, %s0, %s1", s, s, s);
				return;
			case ND_EQ:
				println("  fcmp %s0, %s1", s, s);
				println("  cset x0, eq");
				return;
			case ND_NE:
				println("  fcmp %s0, %s1", s, s);
				println("  cset x0, ne");
				return;
			case ND_LT:
				println("  fcmp %s0, %s1", s, s);
				println("  cset x0, mi");
				return;
			case ND_LE:
				println("  fcmp %s0, %s1", s, s);
				println("  cset x0, ls");
				return;
			default:
				error_tok(node->tok, "invalid float operation");
			}
		}

		// Integer binary operations
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
			if (node->ty->is_unsigned) {
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
			if (node->lhs->ty->is_unsigned) {
				println("  cset x0, lo");
			} else {
				println("  cset x0, lt");
			}
			break;
		case ND_LE:
			println("  cmp x0, x1");
			if (node->lhs->ty->is_unsigned) {
				println("  cset x0, ls");
			} else {
				println("  cset x0, le");
			}
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

		int top = 16; // Stack params start at FP+16
		int bottom = 0;
		int gp = 0, fp = 0;

		// Mark stack-passed parameters (args 9+)
		for (Obj *var = fn->params; var; var = var->next) {
			Type *ty = var->ty;
			if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
				continue;
			}
			if (is_flonum(ty)) {
				if (fp++ < FP_MAX) {
					continue;
				}
			} else {
				if (gp++ < GP_MAX) {
					continue;
				}
			}
			top = align_to(top, 8);
			var->offset = top;
			top += MAX(8, var->ty->size);
		}

		// Local variables get negative offsets
		for (Obj *var = fn->locals; var; var = var->next) {
			if (var->offset) {
				continue; // Already assigned (stack param)
			}
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

		if (var->is_static) {
			println("  .local %s", var->name);
		} else {
			println("  .globl %s", var->name);
		}

		if (var->init_data) {
			println("  .data");
		} else {
			println("  .bss");
		}

		println("  .type %s, %%object", var->name);
		println("  .size %s, %d", var->name, var->ty->size);
		println("  .align %d", var->align);
		println("%s:", var->name);

		if (var->init_data) {
			Relocation *rel = var->rel;
			int pos = 0;
			while (pos < var->ty->size) {
				if (rel && rel->offset == pos) {
					println("  .xword %s%+ld", *rel->label, rel->addend);
					rel = rel->next;
					pos += 8;
				} else {
					println("  .byte %d", var->init_data[pos++]);
				}
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

		// Copy register parameters to stack slots
		int gp = 0, fp = 0;
		for (Obj *var = fn->params; var; var = var->next) {
			Type *ty = var->ty;
			if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
				continue;
			}
			if (is_flonum(ty)) {
				if (fp < FP_MAX) {
					store_fp(fp++, var->offset, ty->size);
				}
			} else {
				if (gp < GP_MAX) {
					store_gp(gp++, var->offset, ty->size);
				}
			}
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
