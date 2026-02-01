#include "as.h"

static Macro *macros = NULL;

void macro_init(void) {
	macros = NULL;
}

Macro *macro_lookup(const char *name) {
	for (Macro *m = macros; m; m = m->next) {
		if (strcmp(m->name, name) == 0) {
			return m;
		}
	}
	return NULL;
}

Token *clone_token(Token *tok) {
	Token *t = calloc(1, sizeof(Token));
	*t = *tok;
	t->next = NULL;
	if (tok->str) {
		t->str = strdup(tok->str);
	}
	return t;
}

Token *clone_token_list(Token *head) {
	Token dummy = {};
	Token *tail = &dummy;
	for (Token *t = head; t; t = t->next) {
		tail = tail->next = clone_token(t);
	}
	return dummy.next;
}

Macro *macro_define(const char *name, int num_params, char **params, Token *body) {
	Macro *m = calloc(1, sizeof(Macro));
	m->name = strdup(name);
	m->num_params = num_params;
	if (num_params > 0) {
		m->params = calloc(num_params, sizeof(char *));
		for (int i = 0; i < num_params; i++) {
			m->params[i] = strdup(params[i]);
		}
	}
	m->body = clone_token_list(body);
	m->next = macros;
	macros = m;
	return m;
}

static int test_failures = 0;

static void check_bytes(const char *name, uint8_t *buf, size_t size, uint8_t *expected, size_t expected_size) {
	if (size != expected_size) {
		printf("FAIL %s: size %zu != expected %zu\n", name, size, expected_size);
		test_failures++;
		return;
	}
	for (size_t i = 0; i < size; i++) {
		if (buf[i] != expected[i]) {
			printf("FAIL %s: byte[%zu] = 0x%02x != expected 0x%02x\n",
			       name,
			       i,
			       buf[i],
			       expected[i]);
			test_failures++;
			return;
		}
	}
	printf("PASS %s\n", name);
}

static void assemble_and_check(const char *name, const char *src, uint8_t *expected, size_t expected_size) {
	char *input = strdup(src);
	current_file = "<test>";
	current_input = input;
	Token *tok = tokenize(input);
	pass1(tok);
	pass2(tok);
	check_bytes(name, text_section.data, text_section.size, expected, expected_size);
	free(input);
}

void test_macro(void) {
	printf("Testing macro expansion...\n\n");

	printf("Parameterless macro:\n");
	{
		// .macro nop2
		//     nop
		//     nop
		// .endm
		// nop2
		// Expected: two NOP instructions (0xd503201f each)
		uint8_t expected[] = {0x1f, 0x20, 0x03, 0xd5, 0x1f, 0x20, 0x03, 0xd5};
		assemble_and_check(
		    "nop2",
		    ".macro nop2\n    nop\n    nop\n.endm\nnop2\n",
		    expected,
		    sizeof(expected));
	}

	printf("\nImmediate parameter:\n");
	{
		// .macro load_imm reg, val
		//     mov \reg, #\val
		// .endm
		// load_imm x0, 42
		// Expected: mov x0, #42 = 0xd2800540
		uint8_t expected[] = {0x40, 0x05, 0x80, 0xd2};
		assemble_and_check(
		    "load_imm x0, 42",
		    ".macro load_imm reg, val\n    mov \\reg, #\\val\n.endm\nload_imm x0, 42\n",
		    expected,
		    sizeof(expected));
	}

	printf("\nLabel parameter:\n");
	{
		// .macro entry name
		// \name:
		//     nop
		// .endm
		// entry foo
		// b foo
		// Expected: nop (0xd503201f), b -4 (0x17ffffff)
		uint8_t expected[] = {0x1f, 0x20, 0x03, 0xd5, 0xff, 0xff, 0xff, 0x17};
		assemble_and_check(
		    "entry with label",
		    ".macro entry name\n\\name:\n    nop\n.endm\nentry foo\nb foo\n",
		    expected,
		    sizeof(expected));
	}

	printf("\nSymbol in immediate (.equ):\n");
	{
		// .equ SIZE, 16
		// .macro adjust
		//     sub sp, sp, #SIZE
		// .endm
		// adjust
		// Expected: sub sp, sp, #16 = 0xd10043ff
		uint8_t expected[] = {0xff, 0x43, 0x00, 0xd1};
		assemble_and_check(
		    "sub with .equ symbol",
		    ".equ SIZE, 16\n.macro adjust\n    sub sp, sp, #SIZE\n.endm\nadjust\n",
		    expected,
		    sizeof(expected));
	}

	printf("\nMultiple invocations:\n");
	{
		// .macro inc reg
		//     add \reg, \reg, #1
		// .endm
		// inc x0
		// inc x1
		// Expected: add x0, x0, #1 = 0x91000400, add x1, x1, #1 = 0x91000421
		uint8_t expected[] = {0x00, 0x04, 0x00, 0x91, 0x21, 0x04, 0x00, 0x91};
		assemble_and_check(
		    "multiple invocations",
		    ".macro inc reg\n    add \\reg, \\reg, #1\n.endm\ninc x0\ninc x1\n",
		    expected,
		    sizeof(expected));
	}

	printf("\n");
	if (test_failures == 0) {
		printf("All macro tests passed!\n");
	} else {
		printf("%d macro test(s) FAILED\n", test_failures);
		exit(1);
	}
}
