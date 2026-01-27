#include <test.h>

// Global variables for variable tests
int g1, g2[4];
static int g3 = 3;

static int ret3(void) {
	return 3;
}

static int add2(int x, int y) {
	return x + y;
}

static int sub2(int x, int y) {
	return x - y;
}

static int add6(int a, int b, int c, int d, int e, int f) {
	return a + b + c + d + e + f;
}

static int add8(int a, int b, int c, int d, int e, int f, int g, int h) {
	return a + b + c + d + e + f + g + h;
}

static int fib(int x) {
	if (x <= 1) {
		return 1;
	}
	return fib(x - 1) + fib(x - 2);
}

static float add_float(float a, float b) {
	return a + b;
}

static double add_double(double a, double b) {
	return a + b;
}

// === ARITHMETIC TESTS (from arith.c) ===

TEST(arithmetic_basic) {
	ASSERT_EQ(0, 0, "zero");
	ASSERT_EQ(42, 42, "literal");
	ASSERT_EQ(5 + 20 - 4, 21, "add sub");
	ASSERT_EQ(5 + 6 * 7, 47, "precedence");
	ASSERT_EQ(5 * (9 - 6), 15, "parens");
	ASSERT_EQ((3 + 5) / 2, 4, "div");
	ASSERT_EQ(-10 + 20, 10, "neg add");
	ASSERT_EQ(- -10, 10, "double neg");
	ASSERT_EQ(- -+10, 10, "mixed unary");
	return 0;
}

TEST(arithmetic_compare) {
	ASSERT_EQ(0 == 1, 0, "eq false");
	ASSERT_EQ(42 == 42, 1, "eq true");
	ASSERT_EQ(0 != 1, 1, "ne true");
	ASSERT_EQ(42 != 42, 0, "ne false");
	ASSERT_EQ(0 < 1, 1, "lt true");
	ASSERT_EQ(1 < 1, 0, "lt eq");
	ASSERT_EQ(2 < 1, 0, "lt false");
	ASSERT_EQ(0 <= 1, 1, "le true");
	ASSERT_EQ(1 <= 1, 1, "le eq");
	ASSERT_EQ(2 <= 1, 0, "le false");
	ASSERT_EQ(1 > 0, 1, "gt true");
	ASSERT_EQ(1 > 1, 0, "gt eq");
	ASSERT_EQ(1 > 2, 0, "gt false");
	ASSERT_EQ(1 >= 0, 1, "ge true");
	ASSERT_EQ(1 >= 1, 1, "ge eq");
	ASSERT_EQ(1 >= 2, 0, "ge false");
	return 0;
}

TEST(arithmetic_compound) {
	ASSERT_EQ(({ int i = 2; i += 5; i; }), 7, "add assign");
	ASSERT_EQ(({ int i = 2; i += 5; }), 7, "add assign val");
	ASSERT_EQ(({ int i = 5; i -= 2; i; }), 3, "sub assign");
	ASSERT_EQ(({ int i = 3; i *= 2; i; }), 6, "mul assign");
	ASSERT_EQ(({ int i = 6; i /= 2; i; }), 3, "div assign");
	ASSERT_EQ(({ int i = 10; i %= 4; i; }), 2, "mod assign");
	ASSERT_EQ(({ int i = 6; i &= 3; i; }), 2, "and assign");
	ASSERT_EQ(({ int i = 6; i |= 3; i; }), 7, "or assign");
	ASSERT_EQ(({ int i = 15; i ^= 5; i; }), 10, "xor assign");
	ASSERT_EQ(({ int i = 1; i <<= 3; i; }), 8, "shl assign");
	ASSERT_EQ(({ int i = 5; i >>= 1; i; }), 2, "shr assign");
	return 0;
}

TEST(arithmetic_incr) {
	ASSERT_EQ(({ int i = 2; ++i; }), 3, "pre incr");
	ASSERT_EQ(({ int i = 2; i++; }), 2, "post incr val");
	ASSERT_EQ(({ int i = 2; i++; i; }), 3, "post incr");
	ASSERT_EQ(({ int i = 2; i--; }), 2, "post decr val");
	ASSERT_EQ(({ int i = 2; i--; i; }), 1, "post decr");
	ASSERT_EQ(({ int a[3]; a[0] = 0; a[1] = 1; a[2] = 2; int *p = a + 1; ++*p; }), 2, "ptr pre incr");
	ASSERT_EQ(({ int a[3]; a[0] = 0; a[1] = 1; a[2] = 2; int *p = a + 1; --*p; }), 0, "ptr pre decr");
	ASSERT_EQ(({ int a[3]; a[0] = 0; a[1] = 1; a[2] = 2; int *p = a + 1; *p++; }), 1, "ptr post incr");
	ASSERT_EQ(({ int a[3]; a[0] = 0; a[1] = 1; a[2] = 2; int *p = a + 1; *p--; }), 1, "ptr post decr");
	return 0;
}

TEST(arithmetic_logical) {
	ASSERT_EQ(!1, 0, "not 1");
	ASSERT_EQ(!2, 0, "not 2");
	ASSERT_EQ(!0, 1, "not 0");
	ASSERT_EQ(!(char)0, 1, "not char 0");
	ASSERT_EQ(!(long)3, 0, "not long");
	ASSERT_EQ(sizeof(!(char)0), 4, "not sizeof char");
	ASSERT_EQ(sizeof(!(long)0), 4, "not sizeof long");
	return 0;
}

TEST(arithmetic_bitwise) {
	ASSERT_EQ(~0, -1, "bitnot 0");
	ASSERT_EQ(~-1, 0, "bitnot -1");
	ASSERT_EQ(17 % 6, 5, "mod");
	ASSERT_EQ(((long)17) % 6, 5, "mod long");
	ASSERT_EQ(0 & 1, 0, "and 0 1");
	ASSERT_EQ(3 & 1, 1, "and 3 1");
	ASSERT_EQ(7 & 3, 3, "and 7 3");
	ASSERT_EQ(-1 & 10, 10, "and -1 10");
	ASSERT_EQ(0 | 1, 1, "or 0 1");
	ASSERT_EQ(0 ^ 0, 0, "xor 0 0");
	ASSERT_EQ(1 << 0, 1, "shl 0");
	ASSERT_EQ(1 << 3, 8, "shl 3");
	ASSERT_EQ(5 << 1, 10, "shl 1");
	ASSERT_EQ(5 >> 1, 2, "shr 1");
	ASSERT_EQ(-1 >> 1, -1, "shr signed");
	return 0;
}

TEST(arithmetic_ternary) {
	ASSERT_EQ(0 ? 1 : 2, 2, "ternary false");
	ASSERT_EQ(1 ? 1 : 2, 1, "ternary true");
	ASSERT_EQ(0 ? -2 : -1, -1, "ternary neg false");
	ASSERT_EQ(1 ? -2 : -1, -2, "ternary neg true");
	ASSERT_EQ(sizeof(0 ? 1 : 2), 4, "ternary sizeof");
	ASSERT_EQ(sizeof(0 ? (long)1 : (long)2), 8, "ternary sizeof long");
	return 0;
}

// === VARIABLE TESTS (from variable.c) ===

TEST(variables_local) {
	ASSERT_EQ(({ int a; a = 3; a; }), 3, "assign");
	ASSERT_EQ(({ int a = 3; a; }), 3, "init");
	ASSERT_EQ(({ int a = 3; int z = 5; a + z; }), 8, "two vars");
	ASSERT_EQ(({ int a; int b; a = b = 3; a + b; }), 6, "chain assign");
	ASSERT_EQ(({ int foo = 3; foo; }), 3, "long name");
	ASSERT_EQ(({ int foo123 = 3; int bar = 5; foo123 + bar; }), 8, "alphanum name");
	return 0;
}

TEST(variables_sizeof) {
	ASSERT_EQ(({ int x; sizeof(x); }), 4, "sizeof int");
	ASSERT_EQ(({ int x; sizeof x; }), 4, "sizeof no paren");
	ASSERT_EQ(({ int *x; sizeof(x); }), 8, "sizeof ptr");
	ASSERT_EQ(({ int x[4]; sizeof(x); }), 16, "sizeof arr");
	ASSERT_EQ(({ int x[3][4]; sizeof(x); }), 48, "sizeof 2d arr");
	ASSERT_EQ(({ int x[3][4]; sizeof(*x); }), 16, "sizeof 2d deref");
	ASSERT_EQ(({ int x[3][4]; sizeof(**x); }), 4, "sizeof 2d double deref");
	ASSERT_EQ(({ char x; sizeof(x); }), 1, "sizeof char");
	ASSERT_EQ(({ char x[10]; sizeof(x); }), 10, "sizeof char arr");
	ASSERT_EQ(({ long x; sizeof(x); }), 8, "sizeof long");
	ASSERT_EQ(({ short x; sizeof(x); }), 2, "sizeof short");
	return 0;
}

TEST(variables_global) {
	ASSERT_EQ(g1, 0, "global zero init");
	ASSERT_EQ(({ g1 = 3; g1; }), 3, "global assign");
	ASSERT_EQ(({ g2[0] = 0; g2[1] = 1; g2[2] = 2; g2[3] = 3; g2[0]; }), 0, "global arr 0");
	ASSERT_EQ(({ g2[0] = 0; g2[1] = 1; g2[2] = 2; g2[3] = 3; g2[1]; }), 1, "global arr 1");
	ASSERT_EQ(({ g2[0] = 0; g2[1] = 1; g2[2] = 2; g2[3] = 3; g2[2]; }), 2, "global arr 2");
	ASSERT_EQ(({ g2[0] = 0; g2[1] = 1; g2[2] = 2; g2[3] = 3; g2[3]; }), 3, "global arr 3");
	ASSERT_EQ(sizeof(g1), 4, "sizeof global");
	ASSERT_EQ(sizeof(g2), 16, "sizeof global arr");
	ASSERT_EQ(g3, 3, "static global");
	return 0;
}

TEST(variables_scope) {
	ASSERT_EQ(({ int x = 2; { int x = 3; } x; }), 2, "block shadow");
	ASSERT_EQ(({ int x = 2; { int x = 3; } int y = 4; x; }), 2, "block shadow seq");
	ASSERT_EQ(({ int x = 2; { x = 3; } x; }), 3, "block modify");
	return 0;
}

TEST(variables_types) {
	ASSERT_EQ(({ char x = 1; x; }), 1, "char var");
	ASSERT_EQ(({ char x = 1; char y = 2; x; }), 1, "char var x");
	ASSERT_EQ(({ char x = 1; char y = 2; y; }), 2, "char var y");
	return 0;
}

// === CONTROL FLOW TESTS (from control.c) ===

TEST(control_if) {
	ASSERT_EQ(({ int x; if (0){ x = 2;
} else{ x = 3;
} x; }), 3, "if false");
	ASSERT_EQ(({ int x; if (1 - 1){ x = 2;
} else{ x = 3;
} x; }), 3, "if expr false");
	ASSERT_EQ(({ int x; if (1){ x = 2;
} else{ x = 3;
} x; }), 2, "if true");
	ASSERT_EQ(({ int x; if (2 - 1){ x = 2;
} else{ x = 3;
} x; }), 2, "if expr true");
	return 0;
}

TEST(control_for) {
	ASSERT_EQ(({ int i = 0; int j = 0; for (i = 0; i <= 10; i = i + 1){ j = i + j;
} j; }), 55, "for sum");
	ASSERT_EQ(({ int j = 0; for (int i = 0; i <= 10; i = i + 1){ j = j + i;
} j; }), 55, "for decl sum");
	ASSERT_EQ(({ int i = 3; int j = 0; for (int i = 0; i <= 10; i = i + 1){ j = j + i;
} i; }), 3, "for decl scope");
	return 0;
}

TEST(control_while) {
	ASSERT_EQ(({ int i = 0; while (i < 10){ i = i + 1;
} i; }), 10, "while");
	ASSERT_EQ(({ int i = 0; int j = 0; while (i <= 10) { j = i + j; i = i + 1; } j; }), 55, "while sum");
	return 0;
}

TEST(control_dowhile) {
	ASSERT_EQ(({ int i = 0; int j = 0; do { j++; } while (i++ < 6); j; }), 7, "do while");
	return 0;
}

TEST(control_logical) {
	ASSERT_EQ(0 || 1, 1, "or 0 1");
	ASSERT_EQ(0 || (2 - 2) || 5, 1, "or chain");
	ASSERT_EQ(0 || 0, 0, "or 0 0");
	ASSERT_EQ(0 && 1, 0, "and 0 1");
	ASSERT_EQ((2 - 2) && 5, 0, "and short circuit");
	ASSERT_EQ(1 && 5, 1, "and 1 5");
	return 0;
}

TEST(control_break) {
	ASSERT_EQ(({ int i = 0; for (; i < 10; i++) { if (i == 3){ break;
} } i; }), 3, "for break");
	ASSERT_EQ(({ int i = 0; while (1) { if (i++ == 3){ break;
} } i; }), 4, "while break");
	ASSERT_EQ(({ int i = 0; for (; i < 10; i++) { for (;;){ break;
} if (i == 3){ break;
} } i; }), 3, "nested break");
	return 0;
}

TEST(control_continue) {
	ASSERT_EQ(({ int i = 0; int j = 0; for (; i < 10; i++) { if (i > 5){ continue;
} j++; } i; }), 10, "continue i");
	ASSERT_EQ(({ int i = 0; int j = 0; for (; i < 10; i++) { if (i > 5){ continue;
} j++; } j; }), 6, "continue j");
	return 0;
}

TEST(control_switch) {
	ASSERT_EQ(({ int i = 0; switch (0) { case 0: i = 5; break; case 1: i = 6; break; case 2: i = 7; break; } i; }), 5, "switch 0");
	ASSERT_EQ(({ int i = 0; switch (1) { case 0: i = 5; break; case 1: i = 6; break; case 2: i = 7; break; } i; }), 6, "switch 1");
	ASSERT_EQ(({ int i = 0; switch (2) { case 0: i = 5; break; case 1: i = 6; break; case 2: i = 7; break; } i; }), 7, "switch 2");
	ASSERT_EQ(({ int i = 0; switch (3) { case 0: i = 5; break; case 1: i = 6; break; case 2: i = 7; break; } i; }), 0, "switch no match");
	ASSERT_EQ(({ int i = 0; switch (0) { case 0: i = 5; break; default: i = 7; } i; }), 5, "switch default 0");
	ASSERT_EQ(({ int i = 0; switch (1) { case 0: i = 5; break; default: i = 7; } i; }), 7, "switch default 1");
	ASSERT_EQ(({ int i = 0; switch (1) { case 0: 0; case 1: 0; case 2: 0; i = 2; } i; }), 2, "switch fallthrough");
	return 0;
}

TEST(control_goto) {
	ASSERT_EQ(({ int i = 0; goto a; a: i++; b: i++; c: i++; i; }), 3, "goto a");
	ASSERT_EQ(({ int i = 0; goto e; d: i++; e: i++; f: i++; i; }), 2, "goto e");
	return 0;
}

TEST(control_comma) {
	ASSERT_EQ((1, 2, 3), 3, "comma");
	ASSERT_EQ(({ int i = 2, j = 3; (i = 5, j) = 6; i; }), 5, "comma lhs");
	ASSERT_EQ(({ int i = 2, j = 3; (i = 5, j) = 6; j; }), 6, "comma rhs");
	return 0;
}

TEST(control_block) {
	ASSERT_EQ(({ 1; { 2; } 3; }), 3, "block");
	ASSERT_EQ(({ ; ; ; 5; }), 5, "empty stmts");
	return 0;
}

// === FUNCTION TESTS ===

TEST(function_calls) {
	ASSERT_EQ(ret3(), 3, "ret3");
	ASSERT_EQ(add2(3, 5), 8, "add2");
	ASSERT_EQ(sub2(5, 3), 2, "sub2");
	ASSERT_EQ(add6(1, 2, 3, 4, 5, 6), 21, "add6");
	ASSERT_EQ(add8(1, 2, 3, 4, 5, 6, 7, 8), 36, "add8");
	ASSERT_EQ(fib(9), 55, "fib");
	return 0;
}

TEST(pointers) {
	ASSERT_EQ(({ int x=3; *&x; }), 3, "deref addr");
	ASSERT_EQ(({ int x=3; int *y=&x; int **z=&y; **z; }), 3, "double deref");
	ASSERT_EQ(({ int x=3; int y=5; *(&x+1); }), 5, "ptr arith +1");
	ASSERT_EQ(({ int x=3; int y=5; *(&y-1); }), 3, "ptr arith -1");
	ASSERT_EQ(({ int x=3; int y=5; *(&x-(-1)); }), 5, "ptr arith neg");
	ASSERT_EQ(({ int x=3; int *y=&x; *y=5; x; }), 5, "write via ptr");
	ASSERT_EQ(({ int x=3; int y=5; *(&x+1)=7; y; }), 7, "write via ptr arith");
	ASSERT_EQ(({ int x=3; int y=5; *(&y-2+1)=7; x; }), 7, "write via complex ptr");
	ASSERT_EQ(({ int x=3; (&x+2)-&x+3; }), 5, "ptr diff");
	ASSERT_EQ(({ int x, y; x=3; y=5; x+y; }), 8, "multi decl");
	ASSERT_EQ(({ int x=3, y=5; x+y; }), 8, "multi init");
	return 0;
}

TEST(arrays) {
	ASSERT_EQ(({ int x[2]; int *y=&x; *y=3; *x; }), 3, "arr ptr assign");
	ASSERT_EQ(({ int x[3]; *x=3; *(x+1)=4; *(x+2)=5; *x; }), 3, "arr elem 0");
	ASSERT_EQ(({ int x[3]; *x=3; *(x+1)=4; *(x+2)=5; *(x+1); }), 4, "arr elem 1");
	ASSERT_EQ(({ int x[3]; *x=3; *(x+1)=4; *(x+2)=5; *(x+2); }), 5, "arr elem 2");
	ASSERT_EQ(({ int x[2][3]; int *y=x; *y=0; **x; }), 0, "2d arr [0][0]");
	ASSERT_EQ(({ int x[2][3]; int *y=x; *(y+1)=1; *(*x+1); }), 1, "2d arr [0][1]");
	ASSERT_EQ(({ int x[2][3]; int *y=x; *(y+2)=2; *(*x+2); }), 2, "2d arr [0][2]");
	ASSERT_EQ(({ int x[2][3]; int *y=x; *(y+3)=3; **(x+1); }), 3, "2d arr [1][0]");
	ASSERT_EQ(({ int x[2][3]; int *y=x; *(y+4)=4; *(*(x+1)+1); }), 4, "2d arr [1][1]");
	ASSERT_EQ(({ int x[2][3]; int *y=x; *(y+5)=5; *(*(x+1)+2); }), 5, "2d arr [1][2]");
	ASSERT_EQ(({ int x[3]; *x=3; x[1]=4; x[2]=5; *x; }), 3, "subscript 0");
	ASSERT_EQ(({ int x[3]; *x=3; x[1]=4; x[2]=5; *(x+1); }), 4, "subscript 1");
	ASSERT_EQ(({ int x[3]; *x=3; x[1]=4; x[2]=5; *(x+2); }), 5, "subscript 2");
	ASSERT_EQ(({ int x[3]; *x=3; x[1]=4; 2[x]=5; *(x+2); }), 5, "reverse subscript");
	ASSERT_EQ(({ int x[2][3]; int *y=x; y[0]=0; x[0][0]; }), 0, "2d subscript [0][0]");
	ASSERT_EQ(({ int x[2][3]; int *y=x; y[1]=1; x[0][1]; }), 1, "2d subscript [0][1]");
	ASSERT_EQ(({ int x[2][3]; int *y=x; y[2]=2; x[0][2]; }), 2, "2d subscript [0][2]");
	ASSERT_EQ(({ int x[2][3]; int *y=x; y[3]=3; x[1][0]; }), 3, "2d subscript [1][0]");
	ASSERT_EQ(({ int x[2][3]; int *y=x; y[4]=4; x[1][1]; }), 4, "2d subscript [1][1]");
	ASSERT_EQ(({ int x[2][3]; int *y=x; y[5]=5; x[1][2]; }), 5, "2d subscript [1][2]");
	return 0;
}

TEST(casts) {
	ASSERT_EQ((int)8590066177, 131585, "long to int");
	ASSERT_EQ((short)8590066177, 513, "long to short");
	ASSERT_EQ((char)8590066177, 1, "long to char");
	ASSERT_EQ((long)1, 1, "int to long");
	ASSERT_EQ((long)&*(int *)0, 0, "null ptr cast");
	ASSERT_EQ(({ int x=512; *(char *)&x=1; x; }), 513, "cast ptr write");
	ASSERT_EQ(({ int x=5; long y=(long)&x; *(int*)y; }), 5, "round trip cast");
	(void)1;
	ASSERT_EQ((char)255, -1, "char sign");
	ASSERT_EQ((signed char)255, -1, "signed char");
	ASSERT_EQ((unsigned char)255, 255, "unsigned char");
	ASSERT_EQ((short)65535, -1, "short sign");
	ASSERT_EQ((unsigned short)65535, 65535, "unsigned short");
	ASSERT_EQ((long)(int)0xffffffff, -1, "int sign");
	ASSERT_EQ((unsigned)0xffffffff, 0xffffffff, "unsigned int");
	ASSERT_EQ(-1 < 1, 1, "signed cmp");
	ASSERT_EQ(-1 < (unsigned)1, 0, "unsigned cmp");
	ASSERT_EQ((char)127 + (char)127, 254, "char add");
	ASSERT_EQ((short)32767 + (short)32767, 65534, "short add");
	ASSERT_EQ(-1 >> 1, -1, "signed shift");
	ASSERT_EQ((unsigned long)-1, -1, "ulong cast");
	ASSERT_EQ(((unsigned)-1) >> 1, 2147483647, "unsigned shift");
	ASSERT_EQ((-100) / 2, -50, "signed div");
	ASSERT_EQ(((unsigned)-100) / 2, 2147483598, "unsigned div");
	ASSERT_EQ(((unsigned long)-100) / 2, 9223372036854775758UL, "ulong div");
	ASSERT_EQ(((long)-1) / (unsigned)100, 0, "mixed div");
	ASSERT_EQ((-100) % 7, -2, "signed mod");
	ASSERT_EQ(((unsigned)-100) % 7, 2, "unsigned mod");
	ASSERT_EQ(((unsigned long)-100) % 9, 6, "ulong mod");
	ASSERT_EQ((int)(unsigned short)65535, 65535, "ushort to int");
	ASSERT_EQ(({ unsigned short x = 65535; x; }), 65535, "ushort var");
	ASSERT_EQ(({ unsigned short x = 65535; (int)x; }), 65535, "ushort cast int");
	ASSERT_EQ(({ typedef short T; T x = 65535; (int)x; }), -1, "typedef short");
	ASSERT_EQ(({ typedef unsigned short T; T x = 65535; (int)x; }), 65535, "typedef ushort");
	return 0;
}

TEST(usual_conversions) {
	ASSERT_EQ(-10 + (long)5, (long)-5, "add long");
	ASSERT_EQ(-10 - (long)5, (long)-15, "sub long");
	ASSERT_EQ(-10 * (long)5, (long)-50, "mul long");
	ASSERT_EQ(-10 / (long)5, (long)-2, "div long");
	ASSERT_EQ(-2 < (long)-1, 1, "cmp lt long");
	ASSERT_EQ(-2 <= (long)-1, 1, "cmp le long");
	ASSERT_EQ(-2 > (long)-1, 0, "cmp gt long");
	ASSERT_EQ(-2 >= (long)-1, 0, "cmp ge long");
	ASSERT_EQ((long)-2 < -1, 1, "long cmp lt");
	ASSERT_EQ((long)-2 <= -1, 1, "long cmp le");
	ASSERT_EQ((long)-2 > -1, 0, "long cmp gt");
	ASSERT_EQ((long)-2 >= -1, 0, "long cmp ge");
	ASSERT_EQ((long)(2147483647 + 2147483647 + 2), 0, "int overflow");
	ASSERT_EQ(({ long x; x=-1; x; }), (long)-1, "long assign");
	ASSERT_EQ(({ char x[3]; x[0]=0; x[1]=1; x[2]=2; char *y=x+1; y[0]; }), 1, "char ptr idx");
	ASSERT_EQ(({ char x[3]; x[0]=0; x[1]=1; x[2]=2; char *y=x+1; y[-1]; }), 0, "char ptr neg idx");
	return 0;
}

TEST(float_conversions) {
	ASSERT_EQ((float)(char)35, 35, "char to float");
	ASSERT_EQ((float)(short)35, 35, "short to float");
	ASSERT_EQ((float)(int)35, 35, "int to float");
	ASSERT_EQ((float)(long)35, 35, "long to float");
	ASSERT_EQ((float)(unsigned char)35, 35, "uchar to float");
	ASSERT_EQ((float)(unsigned short)35, 35, "ushort to float");
	ASSERT_EQ((float)(unsigned int)35, 35, "uint to float");
	ASSERT_EQ((float)(unsigned long)35, 35, "ulong to float");
	ASSERT_EQ((double)(char)35, 35, "char to double");
	ASSERT_EQ((double)(short)35, 35, "short to double");
	ASSERT_EQ((double)(int)35, 35, "int to double");
	ASSERT_EQ((double)(long)35, 35, "long to double");
	ASSERT_EQ((double)(unsigned char)35, 35, "uchar to double");
	ASSERT_EQ((double)(unsigned short)35, 35, "ushort to double");
	ASSERT_EQ((double)(unsigned int)35, 35, "uint to double");
	ASSERT_EQ((double)(unsigned long)35, 35, "ulong to double");
	ASSERT_EQ((char)(float)35, 35, "float to char");
	ASSERT_EQ((short)(float)35, 35, "float to short");
	ASSERT_EQ((int)(float)35, 35, "float to int");
	ASSERT_EQ((long)(float)35, 35, "float to long");
	ASSERT_EQ((unsigned char)(float)35, 35, "float to uchar");
	ASSERT_EQ((unsigned short)(float)35, 35, "float to ushort");
	ASSERT_EQ((unsigned int)(float)35, 35, "float to uint");
	ASSERT_EQ((unsigned long)(float)35, 35, "float to ulong");
	ASSERT_EQ((char)(double)35, 35, "double to char");
	ASSERT_EQ((short)(double)35, 35, "double to short");
	ASSERT_EQ((int)(double)35, 35, "double to int");
	ASSERT_EQ((long)(double)35, 35, "double to long");
	ASSERT_EQ((unsigned char)(double)35, 35, "double to uchar");
	ASSERT_EQ((unsigned short)(double)35, 35, "double to ushort");
	ASSERT_EQ((unsigned int)(double)35, 35, "double to uint");
	ASSERT_EQ((unsigned long)(double)35, 35, "double to ulong");
	return 0;
}

TEST(float_comparisons) {
	ASSERT_EQ(2e3 == 2e3, 1, "double eq");
	ASSERT_EQ(2e3 == 2e5, 0, "double ne");
	ASSERT_EQ(2.0 == 2, 1, "double int eq");
	ASSERT_EQ(5.1 < 5, 0, "double lt false");
	ASSERT_EQ(5.0 < 5, 0, "double lt eq");
	ASSERT_EQ(4.9 < 5, 1, "double lt true");
	ASSERT_EQ(5.1 <= 5, 0, "double le false");
	ASSERT_EQ(5.0 <= 5, 1, "double le eq");
	ASSERT_EQ(4.9 <= 5, 1, "double le true");
	ASSERT_EQ(2e3f == 2e3, 1, "float eq");
	ASSERT_EQ(2e3f == 2e5, 0, "float ne");
	ASSERT_EQ(2.0f == 2, 1, "float int eq");
	ASSERT_EQ(5.1f < 5, 0, "float lt false");
	ASSERT_EQ(5.0f < 5, 0, "float lt eq");
	ASSERT_EQ(4.9f < 5, 1, "float lt true");
	ASSERT_EQ(5.1f <= 5, 0, "float le false");
	ASSERT_EQ(5.0f <= 5, 1, "float le eq");
	ASSERT_EQ(4.9f <= 5, 1, "float le true");
	return 0;
}

TEST(float_arithmetic) {
	ASSERT_EQ((long)(2.3 + 3.8), 6, "double add");
	ASSERT_EQ((long)(2.3 - 3.8), -1, "double sub");
	ASSERT_EQ((long)(-3.8), -3, "double neg");
	ASSERT_EQ((long)(3.3 * 4), 13, "double mul");
	ASSERT_EQ((long)(5.0 / 2), 2, "double div");
	ASSERT_EQ((long)(2.3f + 3.8f), 6, "float add");
	ASSERT_EQ((long)(2.3f + 3.8), 6, "mixed add");
	ASSERT_EQ((long)(2.3f - 3.8), -1, "mixed sub");
	ASSERT_EQ((long)(-3.8f), -3, "float neg");
	ASSERT_EQ((long)(3.3f * 4), 13, "float mul");
	ASSERT_EQ((long)(5.0f / 2), 2, "float div");
	return 0;
}

TEST(float_nan) {
	ASSERT_EQ(0.0 / 0.0 == 0.0 / 0.0, 0, "nan eq");
	ASSERT_EQ(0.0 / 0.0 != 0.0 / 0.0, 1, "nan ne");
	ASSERT_EQ(0.0 / 0.0 < 0, 0, "nan lt");
	ASSERT_EQ(0.0 / 0.0 <= 0, 0, "nan le");
	return 0;
}

TEST(float_bool) {
	ASSERT_EQ(!3., 0, "double not");
	ASSERT_EQ(!0., 1, "double zero not");
	ASSERT_EQ(!3.f, 0, "float not");
	ASSERT_EQ(!0.f, 1, "float zero not");
	ASSERT_EQ(0.0 ? 3 : 5, 5, "double ternary false");
	ASSERT_EQ(1.2 ? 3 : 5, 3, "double ternary true");
	return 0;
}

TEST(float_functions) {
	ASSERT_EQ(add_float(2.0f, 3.0f), 5, "float func");
	ASSERT_EQ(add_double(4.0, 6.0), 10, "double func");
	ASSERT_EQ(({ float x = 3.5; float y = 3.5; x + y; }), 7, "float local add");
	ASSERT_EQ(({ double x = 5.5; double y = 6.5; x + y; }), 12, "double local add");
	return 0;
}

TEST(struct_basics) {
	ASSERT_EQ(({ struct {int a; int b;} x; x.a=1; x.b=2; x.a; }), 1, "struct member a");
	ASSERT_EQ(({ struct {int a; int b;} x; x.a=1; x.b=2; x.b; }), 2, "struct member b");
	ASSERT_EQ(({ struct {char a; int b; char c;} x; x.a=1; x.b=2; x.c=3; x.a; }), 1, "struct char a");
	ASSERT_EQ(({ struct {char a; int b; char c;} x; x.b=1; x.b=2; x.c=3; x.b; }), 2, "struct int b");
	ASSERT_EQ(({ struct {char a; int b; char c;} x; x.a=1; x.b=2; x.c=3; x.c; }), 3, "struct char c");
	return 0;
}

TEST(struct_arrays) {
	ASSERT_EQ(({ struct {char a; char b;} x[3]; char *p=x; p[0]=0; x[0].a; }), 0, "struct arr [0].a");
	ASSERT_EQ(({ struct {char a; char b;} x[3]; char *p=x; p[1]=1; x[0].b; }), 1, "struct arr [0].b");
	ASSERT_EQ(({ struct {char a; char b;} x[3]; char *p=x; p[2]=2; x[1].a; }), 2, "struct arr [1].a");
	ASSERT_EQ(({ struct {char a; char b;} x[3]; char *p=x; p[3]=3; x[1].b; }), 3, "struct arr [1].b");
	ASSERT_EQ(({ struct {char a[3]; char b[5];} x; char *p=&x; x.a[0]=6; p[0]; }), 6, "nested arr a");
	ASSERT_EQ(({ struct {char a[3]; char b[5];} x; char *p=&x; x.b[0]=7; p[3]; }), 7, "nested arr b");
	ASSERT_EQ(({ struct { struct { char b; } a; } x; x.a.b=6; x.a.b; }), 6, "nested struct");
	return 0;
}

TEST(struct_sizeof) {
	ASSERT_EQ(({ struct {int a;} x; sizeof(x); }), 4, "sizeof 1 int");
	ASSERT_EQ(({ struct {int a; int b;} x; sizeof(x); }), 8, "sizeof 2 int");
	ASSERT_EQ(({ struct {int a, b;} x; sizeof(x); }), 8, "sizeof 2 int comma");
	ASSERT_EQ(({ struct {int a[3];} x; sizeof(x); }), 12, "sizeof int[3]");
	ASSERT_EQ(({ struct {int a;} x[4]; sizeof(x); }), 16, "sizeof struct[4]");
	ASSERT_EQ(({ struct {int a[3];} x[2]; sizeof(x); }), 24, "sizeof nested arr");
	ASSERT_EQ(({ struct {char a; char b;} x; sizeof(x); }), 2, "sizeof 2 char");
	ASSERT_EQ(({ struct {} x; sizeof(x); }), 0, "sizeof empty");
	ASSERT_EQ(({ struct {char a; int b;} x; sizeof(x); }), 8, "sizeof char int pad");
	ASSERT_EQ(({ struct {int a; char b;} x; sizeof(x); }), 8, "sizeof int char pad");
	ASSERT_EQ(({ struct t {int a; int b;} x; struct t y; sizeof(y); }), 8, "sizeof named");
	ASSERT_EQ(({ struct t {int a; int b;}; struct t y; sizeof(y); }), 8, "sizeof tag only");
	ASSERT_EQ(({ struct t {char a[2];}; { struct t {char a[4];}; } struct t y; sizeof(y); }), 2, "sizeof scope");
	ASSERT_EQ(({ struct t {int x;}; int t=1; struct t y; y.x=2; t+y.x; }), 3, "tag vs var");
	ASSERT_EQ(({ struct {char a; long b;} x; sizeof(x); }), 16, "sizeof char long");
	ASSERT_EQ(({ struct {char a; short b;} x; sizeof(x); }), 4, "sizeof char short");
	ASSERT_EQ(({ struct foo *bar; sizeof(bar); }), 8, "sizeof ptr");
	ASSERT_EQ(({ struct T *foo; struct T {int x;}; sizeof(struct T); }), 4, "sizeof forward");
	ASSERT_EQ(({ struct T { struct T *next; int x; } a; struct T b; b.x=1; a.next=&b; a.next->x; }), 1, "self ref");
	ASSERT_EQ(({ typedef struct T T; struct T { int x; }; sizeof(T); }), 4, "typedef struct");
	return 0;
}

TEST(struct_pointers) {
	ASSERT_EQ(({ struct t {char a;} x; struct t *y = &x; x.a=3; y->a; }), 3, "arrow read");
	ASSERT_EQ(({ struct t {char a;} x; struct t *y = &x; y->a=3; x.a; }), 3, "arrow write");
	return 0;
}

TEST(struct_assignment) {
	ASSERT_EQ(({ struct {int a,b;} x,y; x.a=3; y=x; y.a; }), 3, "struct assign");
	ASSERT_EQ(({ struct t {int a,b;}; struct t x; x.a=7; struct t y; struct t *z=&y; *z=x; y.a; }), 7, "ptr assign");
	ASSERT_EQ(({ struct t {int a,b;}; struct t x; x.a=7; struct t y, *p=&x, *q=&y; *q=*p; y.a; }), 7, "ptr deref assign");
	ASSERT_EQ(({ struct t {char a, b;} x, y; x.a=5; y=x; y.a; }), 5, "small struct assign");
	ASSERT_EQ(({ struct {int a;} x={1}, y={2}; (x=y).a; }), 2, "assign expr");
	ASSERT_EQ(({ struct {int a;} x={1}, y={2}; (1?x:y).a; }), 1, "ternary true");
	ASSERT_EQ(({ struct {int a;} x={1}, y={2}; (0?x:y).a; }), 2, "ternary false");
	return 0;
}

TEST(union_basics) {
	ASSERT_EQ(({ union { int a; char b[6]; } x; sizeof(x); }), 8, "union sizeof");
	ASSERT_EQ(({ union { int a; char b[4]; } x; x.a = 515; x.b[0]; }), 3, "union byte 0");
	ASSERT_EQ(({ union { int a; char b[4]; } x; x.a = 515; x.b[1]; }), 2, "union byte 1");
	ASSERT_EQ(({ union { int a; char b[4]; } x; x.a = 515; x.b[2]; }), 0, "union byte 2");
	ASSERT_EQ(({ union { int a; char b[4]; } x; x.a = 515; x.b[3]; }), 0, "union byte 3");
	ASSERT_EQ(({ union {int a,b;} x,y; x.a=3; y.a=5; y=x; y.a; }), 3, "union assign");
	ASSERT_EQ(({ union {struct {int a,b;} c;} x,y; x.c.b=3; y.c.b=5; y=x; y.c.b; }), 3, "union nested");
	return 0;
}

TEST(union_anonymous) {
	ASSERT_EQ(({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.a; }), 0xef, "anon byte 0");
	ASSERT_EQ(({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.b; }), 0xbe, "anon byte 1");
	ASSERT_EQ(({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.c; }), 0xad, "anon byte 2");
	ASSERT_EQ(({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.d; }), 0xde, "anon byte 3");
	ASSERT_EQ(({struct { union { int a,b; }; union { int c,d; }; } x; x.a=3; x.b; }), 3, "anon union a");
	ASSERT_EQ(({struct { union { int a,b; }; union { int c,d; }; } x; x.d=5; x.c; }), 5, "anon union d");
	return 0;
}

struct bitfield_global {
	char a;
	int b : 5;
	int c : 10;
} g45 = {1, 2, 3}, g46 = {};

TEST(bitfield_basics) {
	ASSERT_EQ(sizeof(struct { int x : 1; }), 4, "bf int sizeof");
	ASSERT_EQ(sizeof(struct { long x : 1; }), 8, "bf long sizeof");

	struct bit1 {
		short a;
		char b;
		int c : 2;
		int d : 3;
		int e : 3;
	};

	ASSERT_EQ(sizeof(struct bit1), 4, "bf packed sizeof");
	ASSERT_EQ(({ struct bit1 x; x.a=1; x.b=2; x.c=3; x.d=4; x.e=5; x.a; }), 1, "bf assign a");
	ASSERT_EQ(({ struct bit1 x={1,2,3,4,5}; x.a; }), 1, "bf init a");
	ASSERT_EQ(({ struct bit1 x={1,2,3,4,5}; x.b; }), 2, "bf init b");
	ASSERT_EQ(({ struct bit1 x={1,2,3,4,5}; x.c; }), -1, "bf init c sign");
	ASSERT_EQ(({ struct bit1 x={1,2,3,4,5}; x.d; }), -4, "bf init d sign");
	ASSERT_EQ(({ struct bit1 x={1,2,3,4,5}; x.e; }), -3, "bf init e sign");
	return 0;
}

TEST(bitfield_global) {
	ASSERT_EQ(g45.a, 1, "global bf a");
	ASSERT_EQ(g45.b, 2, "global bf b");
	ASSERT_EQ(g45.c, 3, "global bf c");
	ASSERT_EQ(g46.a, 0, "global bf zero a");
	ASSERT_EQ(g46.b, 0, "global bf zero b");
	ASSERT_EQ(g46.c, 0, "global bf zero c");
	return 0;
}

TEST(bitfield_incr) {
	typedef struct {
		int a : 10;
		int b : 10;
		int c : 10;
	} T3;

	ASSERT_EQ(({ T3 x={1,2,3}; x.a++; }), 1, "bf post incr a");
	ASSERT_EQ(({ T3 x={1,2,3}; x.b++; }), 2, "bf post incr b");
	ASSERT_EQ(({ T3 x={1,2,3}; x.c++; }), 3, "bf post incr c");
	ASSERT_EQ(({ T3 x={1,2,3}; ++x.a; }), 2, "bf pre incr a");
	ASSERT_EQ(({ T3 x={1,2,3}; ++x.b; }), 3, "bf pre incr b");
	ASSERT_EQ(({ T3 x={1,2,3}; ++x.c; }), 4, "bf pre incr c");
	return 0;
}

TEST(bitfield_sizeof) {
	ASSERT_EQ(sizeof(struct {int a:3; int c:1; int c:5; }), 4, "bf sizeof packed");
	ASSERT_EQ(sizeof(struct {int a:3; int:0; int c:5; }), 8, "bf sizeof zero");
	ASSERT_EQ(sizeof(struct {int a:3; int:0; }), 4, "bf sizeof trailing");
	return 0;
}

TEST_SUITE(codegen) {
	// Arithmetic (from arith.c)
	RUN_TEST(arithmetic_basic);
	RUN_TEST(arithmetic_compare);
	RUN_TEST(arithmetic_compound);
	RUN_TEST(arithmetic_incr);
	RUN_TEST(arithmetic_logical);
	RUN_TEST(arithmetic_bitwise);
	RUN_TEST(arithmetic_ternary);
	// Variables (from variable.c)
	RUN_TEST(variables_local);
	RUN_TEST(variables_sizeof);
	RUN_TEST(variables_global);
	RUN_TEST(variables_scope);
	RUN_TEST(variables_types);
	// Control flow (from control.c)
	RUN_TEST(control_if);
	RUN_TEST(control_for);
	RUN_TEST(control_while);
	RUN_TEST(control_dowhile);
	RUN_TEST(control_logical);
	RUN_TEST(control_break);
	RUN_TEST(control_continue);
	RUN_TEST(control_switch);
	RUN_TEST(control_goto);
	RUN_TEST(control_comma);
	RUN_TEST(control_block);
	// Functions (from function.c)
	RUN_TEST(function_calls);
	// Pointers and arrays (from pointer.c)
	RUN_TEST(pointers);
	RUN_TEST(arrays);
	// Type conversions (from cast.c, usualconv.c)
	RUN_TEST(casts);
	RUN_TEST(usual_conversions);
	// Floating point (from float.c)
	RUN_TEST(float_conversions);
	RUN_TEST(float_comparisons);
	RUN_TEST(float_arithmetic);
	RUN_TEST(float_nan);
	RUN_TEST(float_bool);
	RUN_TEST(float_functions);
	// Structs (from struct.c)
	RUN_TEST(struct_basics);
	RUN_TEST(struct_arrays);
	RUN_TEST(struct_sizeof);
	RUN_TEST(struct_pointers);
	RUN_TEST(struct_assignment);
	// Unions (from union.c)
	RUN_TEST(union_basics);
	RUN_TEST(union_anonymous);
	// Bitfields (from bitfield.c)
	RUN_TEST(bitfield_basics);
	RUN_TEST(bitfield_global);
	RUN_TEST(bitfield_incr);
	RUN_TEST(bitfield_sizeof);
}
