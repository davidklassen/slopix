#include "as.h"

#define AS_VERSION "as (slopix) 1.0"

char *current_file;
char *current_input;

void error(char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}

static void verror_at(char *loc, char *fmt, va_list ap) {
	char *line = loc;
	while (current_input < line && line[-1] != '\n') {
		line--;
	}

	char *end = loc;
	while (*end && *end != '\n') {
		end++;
	}

	int line_no = 1;
	for (char *p = current_input; p < loc; p++) {
		if (*p == '\n') {
			line_no++;
		}
	}

	int indent = fprintf(stderr, "%s:%d: ", current_file, line_no);
	fprintf(stderr, "%.*s\n", (int)(end - line), line);

	int pos = (int)(loc - line) + indent;
	fprintf(stderr, "%*s", pos, "");
	fprintf(stderr, "^ ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}

void error_at(char *loc, char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	verror_at(loc, fmt, ap);
	va_end(ap);
	exit(1);
}

void error_tok(Token *tok, char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	verror_at(tok->loc, fmt, ap);
	va_end(ap);
	exit(1);
}

static void vwarn_at(char *loc, char *fmt, va_list ap) {
	char *line = loc;
	while (current_input < line && line[-1] != '\n') {
		line--;
	}

	char *end = loc;
	while (*end && *end != '\n') {
		end++;
	}

	int line_no = 1;
	for (char *p = current_input; p < loc; p++) {
		if (*p == '\n') {
			line_no++;
		}
	}

	int indent = fprintf(stderr, "%s:%d: warning: ", current_file, line_no);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	fprintf(stderr, "%*s%.*s\n", indent - 10, "", (int)(end - line), line);
	fprintf(stderr, "%*s^\n", indent - 10 + (int)(loc - line), "");
}

void warn(char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s: warning: ", current_file);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

void warn_tok(Token *tok, char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vwarn_at(tok->loc, fmt, ap);
	va_end(ap);
}

static char *read_stdin(void) {
	size_t capacity = 4096;
	size_t size = 0;
	char *buf = malloc(capacity);

	while (!feof(stdin)) {
		if (size + 1024 > capacity) {
			capacity *= 2;
			buf = realloc(buf, capacity);
		}
		size_t n = fread(buf + size, 1, 1024, stdin);
		size += n;
	}

	if (size + 2 > capacity) {
		buf = realloc(buf, size + 2);
	}
	if (size == 0 || buf[size - 1] != '\n') {
		buf[size++] = '\n';
	}
	buf[size] = '\0';
	return buf;
}

char *read_file(char *path) {
	FILE *fp = fopen(path, "r");
	if (!fp) {
		error("cannot open %s: %s", path, strerror(errno));
	}

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char *buf = malloc(size + 2);
	fread(buf, 1, size, fp);
	fclose(fp);

	if (size == 0 || buf[size - 1] != '\n') {
		buf[size++] = '\n';
	}
	buf[size] = '\0';
	return buf;
}

static char *token_kind_name(TokenKind kind) {
	switch (kind) {
	case TOK_EOF:
		return "EOF";
	case TOK_NEWLINE:
		return "NEWLINE";
	case TOK_LABEL:
		return "LABEL";
	case TOK_IDENT:
		return "IDENT";
	case TOK_REGISTER:
		return "REGISTER";
	case TOK_NUMBER:
		return "NUMBER";
	case TOK_STRING:
		return "STRING";
	case TOK_HASH:
		return "HASH";
	case TOK_COMMA:
		return "COMMA";
	case TOK_LBRACKET:
		return "LBRACKET";
	case TOK_RBRACKET:
		return "RBRACKET";
	case TOK_BANG:
		return "BANG";
	case TOK_COLON:
		return "COLON";
	case TOK_EQUALS:
		return "EQUALS";
	case TOK_LO12:
		return "LO12";
	case TOK_DOT:
		return "DOT";
	case TOK_PLUS:
		return "PLUS";
	case TOK_MINUS:
		return "MINUS";
	case TOK_PERCENT:
		return "PERCENT";
	}
	return "UNKNOWN";
}

static char *reg_type_name(RegType type) {
	switch (type) {
	case REG_GP:
		return "GP";
	case REG_FP:
		return "FP";
	case REG_SP:
		return "SP";
	case REG_ZR:
		return "ZR";
	}
	return "UNKNOWN";
}

void dump_tokens(Token *tok) {
	for (Token *t = tok; t; t = t->next) {
		printf("%3d: %-10s ", t->line_no, token_kind_name(t->kind));
		switch (t->kind) {
		case TOK_REGISTER:
			printf("%.*s (num=%d, width=%d, type=%s)\n",
			       t->len,
			       t->loc,
			       t->reg_num,
			       t->reg_width,
			       reg_type_name(t->reg_type));
			break;
		case TOK_NUMBER:
			printf("%lld\n", (long long)t->val);
			break;
		case TOK_STRING:
			printf("\"%s\"\n", t->str);
			break;
		case TOK_LABEL:
		case TOK_IDENT:
			printf("%s\n", t->str);
			break;
		case TOK_NEWLINE:
			printf("\\n\n");
			break;
		case TOK_EOF:
			printf("\n");
			break;
		default:
			printf("%.*s\n", t->len, t->loc);
			break;
		}
	}
}

static void usage(int code) {
	fprintf(stderr, "Usage: as [options] [input.s]\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -o <file>       Output file (default: a.out)\n");
	fprintf(stderr, "  -v              Verbose output\n");
	fprintf(stderr, "  --version       Print version and exit\n");
	fprintf(stderr, "  --help          Print this help and exit\n");
	fprintf(stderr, "  -               Read from stdin\n");
	fprintf(stderr, "Debug options:\n");
	fprintf(stderr, "  -dump-tokens    Print tokens and exit\n");
	fprintf(stderr, "  -dump-symbols   Print symbol table and exit\n");
	fprintf(stderr, "  -test-encode    Run encoding tests and exit\n");
	exit(code);
}

static void version(void) {
	printf("%s\n", AS_VERSION);
	exit(0);
}

static void print_verbose_summary(const char *input, const char *output) {
	int nsyms = symtab_count();
	int nlocals = 0, nglobals = 0;
	for (int i = 0; i < nsyms; i++) {
		Symbol *sym = symtab_get(i);
		if (sym->binding == STB_LOCAL) {
			nlocals++;
		} else {
			nglobals++;
		}
	}

	int text_relocs = reloc_count(SECTION_TEXT);
	int data_relocs = reloc_count(SECTION_DATA);

	fprintf(stderr, "%s -> %s\n", input, output);
	fprintf(stderr, "  .text:  %zu bytes\n", text_section.size);
	fprintf(stderr, "  .data:  %zu bytes\n", data_section.size);
	fprintf(stderr, "  .bss:   %zu bytes\n", bss_size);
	fprintf(stderr, "  symbols: %d (%d local, %d global)\n", nsyms, nlocals, nglobals);
	fprintf(stderr, "  relocations: %d (.text: %d, .data: %d)\n", text_relocs + data_relocs, text_relocs, data_relocs);
}

int main(int argc, char **argv) {
	char *input_file = NULL;
	char *output_file = NULL;
	bool dump_tokens_flag = false;
	bool dump_symbols_flag = false;
	bool test_encode_flag = false;
	bool verbose_flag = false;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-dump-tokens") == 0) {
			dump_tokens_flag = true;
		} else if (strcmp(argv[i], "-dump-symbols") == 0) {
			dump_symbols_flag = true;
		} else if (strcmp(argv[i], "-test-encode") == 0) {
			test_encode_flag = true;
		} else if (strcmp(argv[i], "-o") == 0) {
			if (i + 1 >= argc) {
				usage(1);
			}
			output_file = argv[++i];
		} else if (strcmp(argv[i], "-v") == 0) {
			verbose_flag = true;
		} else if (strcmp(argv[i], "--version") == 0) {
			version();
		} else if (strcmp(argv[i], "--help") == 0) {
			usage(0);
		} else if (strcmp(argv[i], "-") == 0) {
			if (input_file) {
				error("multiple input files");
			}
			input_file = argv[i];
		} else if (argv[i][0] == '-') {
			error("unknown option: %s", argv[i]);
		} else {
			if (input_file) {
				error("multiple input files");
			}
			input_file = argv[i];
		}
	}

	if (test_encode_flag) {
		test_encode();
		return 0;
	}

	if (!input_file) {
		input_file = "-";
	}

	current_file = input_file;
	if (strcmp(input_file, "-") == 0) {
		current_input = read_stdin();
	} else {
		current_input = read_file(input_file);
	}
	Token *tok = tokenize(current_input);

	if (dump_tokens_flag) {
		dump_tokens(tok);
		return 0;
	}

	pass1(tok);

	if (dump_symbols_flag) {
		dump_symbols();
		return 0;
	}

	pass2(tok);

	const char *outfile = output_file ? output_file : "a.out";
	elf_write(outfile);

	if (verbose_flag) {
		print_verbose_summary(input_file, outfile);
	}

	return 0;
}
