#include "as.h"

static Token *new_token(TokenKind kind, char *start, char *end, int line_no) {
	Token *tok = calloc(1, sizeof(Token));
	tok->kind = kind;
	tok->loc = start;
	tok->len = end - start;
	tok->line_no = line_no;
	return tok;
}

static bool startswith(char *p, char *q) {
	return strncmp(p, q, strlen(q)) == 0;
}

static bool is_ident1(char c) {
	return isalpha(c) || c == '_' || c == '.';
}

static bool is_ident2(char c) {
	return is_ident1(c) || isdigit(c);
}

static int read_ident_len(char *p) {
	char *start = p;
	if (!is_ident1(*p)) {
		return 0;
	}
	while (is_ident2(*p)) {
		p++;
	}
	return p - start;
}

static char *copy_str(char *start, int len) {
	char *s = malloc(len + 1);
	memcpy(s, start, len);
	s[len] = '\0';
	return s;
}

static Token *read_register(char *p, int line_no) {
	char *start = p;
	int width = 0;
	int num = 0;
	RegType type = REG_GP;

	if (startswith(p, "sp") && !is_ident2(p[2])) {
		Token *tok = new_token(TOK_REGISTER, start, p + 2, line_no);
		tok->reg_num = 31;
		tok->reg_width = 64;
		tok->reg_type = REG_SP;
		return tok;
	}

	if (startswith(p, "xzr") && !is_ident2(p[3])) {
		Token *tok = new_token(TOK_REGISTER, start, p + 3, line_no);
		tok->reg_num = 31;
		tok->reg_width = 64;
		tok->reg_type = REG_ZR;
		return tok;
	}

	if (startswith(p, "wzr") && !is_ident2(p[3])) {
		Token *tok = new_token(TOK_REGISTER, start, p + 3, line_no);
		tok->reg_num = 31;
		tok->reg_width = 32;
		tok->reg_type = REG_ZR;
		return tok;
	}

	if (*p == 'x' || *p == 'X') {
		width = 64;
		type = REG_GP;
		p++;
	} else if (*p == 'w' || *p == 'W') {
		width = 32;
		type = REG_GP;
		p++;
	} else if (*p == 'd' || *p == 'D') {
		width = 64;
		type = REG_FP;
		p++;
	} else if (*p == 's' || *p == 'S') {
		width = 32;
		type = REG_FP;
		p++;
	} else {
		return NULL;
	}

	if (!isdigit(*p)) {
		return NULL;
	}

	num = *p++ - '0';
	if (isdigit(*p)) {
		num = num * 10 + (*p++ - '0');
	}

	if (isalnum(*p)) {
		return NULL;
	}

	if (type == REG_GP && num > 30) {
		return NULL;
	}
	if (type == REG_FP && num > 31) {
		return NULL;
	}

	Token *tok = new_token(TOK_REGISTER, start, p, line_no);
	tok->reg_num = num;
	tok->reg_width = width;
	tok->reg_type = type;
	return tok;
}

static int from_hex(char c) {
	if ('0' <= c && c <= '9') {
		return c - '0';
	}
	if ('a' <= c && c <= 'f') {
		return c - 'a' + 10;
	}
	return c - 'A' + 10;
}

static int read_escaped_char(char **new_pos, char *p) {
	if ('0' <= *p && *p <= '7') {
		int c = *p++ - '0';
		if ('0' <= *p && *p <= '7') {
			c = (c << 3) + (*p++ - '0');
			if ('0' <= *p && *p <= '7') {
				c = (c << 3) + (*p++ - '0');
			}
		}
		*new_pos = p;
		return c;
	}

	if (*p == 'x') {
		p++;
		if (!isxdigit(*p)) {
			error_at(p, "invalid hex escape sequence");
		}
		int c = 0;
		for (; isxdigit(*p); p++) {
			c = (c << 4) + from_hex(*p);
		}
		*new_pos = p;
		return c;
	}

	*new_pos = p + 1;

	switch (*p) {
	case 'a':
		return '\a';
	case 'b':
		return '\b';
	case 't':
		return '\t';
	case 'n':
		return '\n';
	case 'v':
		return '\v';
	case 'f':
		return '\f';
	case 'r':
		return '\r';
	case 'e':
		return 27;
	default:
		return *p;
	}
}

static Token *read_string(char *start, int line_no) {
	char *p = start + 1;
	char *end = p;

	while (*end != '"') {
		if (*end == '\n' || *end == '\0') {
			error_at(start, "unclosed string literal");
		}
		if (*end == '\\') {
			end++;
		}
		end++;
	}

	char *buf = malloc(end - p + 1);
	int len = 0;

	while (p < end) {
		if (*p == '\\') {
			buf[len++] = read_escaped_char(&p, p + 1);
		} else {
			buf[len++] = *p++;
		}
	}
	buf[len] = '\0';

	Token *tok = new_token(TOK_STRING, start, end + 1, line_no);
	tok->str = buf;
	return tok;
}

static Token *read_number(char *start, int line_no) {
	char *p = start;
	int64_t val = 0;
	bool negative = false;

	if (*p == '-') {
		negative = true;
		p++;
	}

	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		p += 2;
		if (!isxdigit(*p)) {
			error_at(start, "invalid hex number");
		}
		while (isxdigit(*p)) {
			val = (val << 4) + from_hex(*p++);
		}
	} else {
		while (isdigit(*p)) {
			val = val * 10 + (*p++ - '0');
		}
	}

	if (negative) {
		val = -val;
	}

	Token *tok = new_token(TOK_NUMBER, start, p, line_no);
	tok->val = val;
	return tok;
}

Token *tokenize(char *input) {
	char *p = input;
	Token head = {};
	Token *cur = &head;
	int line_no = 1;

	while (*p) {
		if (*p == '\n') {
			cur = cur->next = new_token(TOK_NEWLINE, p, p + 1, line_no);
			p++;
			line_no++;
			continue;
		}

		if (isspace(*p)) {
			p++;
			continue;
		}

		if (startswith(p, "//")) {
			p += 2;
			while (*p && *p != '\n') {
				p++;
			}
			continue;
		}

		if (startswith(p, "/*")) {
			char *q = strstr(p + 2, "*/");
			if (!q) {
				error_at(p, "unclosed block comment");
			}
			for (char *c = p; c < q + 2; c++) {
				if (*c == '\n') {
					line_no++;
				}
			}
			p = q + 2;
			continue;
		}

		if (startswith(p, ":lo12:")) {
			cur = cur->next = new_token(TOK_LO12, p, p + 6, line_no);
			p += 6;
			continue;
		}

		if (*p == '"') {
			cur = cur->next = read_string(p, line_no);
			p += cur->len;
			continue;
		}

		if (isdigit(*p)) {
			cur = cur->next = read_number(p, line_no);
			p += cur->len;
			continue;
		}

		Token *reg = read_register(p, line_no);
		if (reg) {
			cur = cur->next = reg;
			p += cur->len;
			continue;
		}

		int ident_len = read_ident_len(p);
		if (ident_len > 0) {
			if (p[ident_len] == ':' && !startswith(p + ident_len, ":lo12:")) {
				Token *tok = new_token(TOK_LABEL, p, p + ident_len, line_no);
				tok->str = copy_str(p, ident_len);
				cur = cur->next = tok;
				p += ident_len + 1;
				continue;
			}

			Token *tok = new_token(TOK_IDENT, p, p + ident_len, line_no);
			tok->str = copy_str(p, ident_len);
			cur = cur->next = tok;
			p += ident_len;
			continue;
		}

		if (*p == '#') {
			cur = cur->next = new_token(TOK_HASH, p, p + 1, line_no);
			p++;
			continue;
		}

		if (*p == ',') {
			cur = cur->next = new_token(TOK_COMMA, p, p + 1, line_no);
			p++;
			continue;
		}

		if (*p == '[') {
			cur = cur->next = new_token(TOK_LBRACKET, p, p + 1, line_no);
			p++;
			continue;
		}

		if (*p == ']') {
			cur = cur->next = new_token(TOK_RBRACKET, p, p + 1, line_no);
			p++;
			continue;
		}

		if (*p == '!') {
			cur = cur->next = new_token(TOK_BANG, p, p + 1, line_no);
			p++;
			continue;
		}

		if (*p == ':') {
			cur = cur->next = new_token(TOK_COLON, p, p + 1, line_no);
			p++;
			continue;
		}

		if (*p == '=') {
			cur = cur->next = new_token(TOK_EQUALS, p, p + 1, line_no);
			p++;
			continue;
		}

		if (*p == '.') {
			cur = cur->next = new_token(TOK_DOT, p, p + 1, line_no);
			p++;
			continue;
		}

		if (*p == '+') {
			cur = cur->next = new_token(TOK_PLUS, p, p + 1, line_no);
			p++;
			continue;
		}

		if (*p == '-') {
			if (isdigit(p[1])) {
				cur = cur->next = read_number(p, line_no);
				p += cur->len;
				continue;
			}
			cur = cur->next = new_token(TOK_MINUS, p, p + 1, line_no);
			p++;
			continue;
		}

		if (*p == '%') {
			cur = cur->next = new_token(TOK_PERCENT, p, p + 1, line_no);
			p++;
			continue;
		}

		error_at(p, "invalid token");
	}

	cur = cur->next = new_token(TOK_EOF, p, p, line_no);
	return head.next;
}
