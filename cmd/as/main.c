#include "as.h"

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

static void usage(void) {
	fprintf(stderr, "Usage: as [options] <input.s>\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -dump-tokens    Print tokens and exit\n");
	fprintf(stderr, "  -dump-symbols   Print symbol table and exit\n");
	fprintf(stderr, "  -o <file>       Output file\n");
	exit(1);
}

int main(int argc, char **argv) {
	char *input_file = NULL;
	char *output_file = NULL;
	bool dump_tokens_flag = false;
	bool dump_symbols_flag = false;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-dump-tokens") == 0) {
			dump_tokens_flag = true;
		} else if (strcmp(argv[i], "-dump-symbols") == 0) {
			dump_symbols_flag = true;
		} else if (strcmp(argv[i], "-o") == 0) {
			if (i + 1 >= argc) {
				usage();
			}
			output_file = argv[++i];
		} else if (argv[i][0] == '-') {
			error("unknown option: %s", argv[i]);
		} else {
			if (input_file) {
				error("multiple input files");
			}
			input_file = argv[i];
		}
	}

	if (!input_file) {
		usage();
	}

	(void)output_file;

	current_file = input_file;
	current_input = read_file(input_file);
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

	printf("Parsed %s successfully (%d symbols)\n", input_file, symtab_count());
	return 0;
}
