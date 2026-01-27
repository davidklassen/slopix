#ifndef AS_H
#define AS_H

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	TOK_EOF,
	TOK_NEWLINE,
	TOK_LABEL,
	TOK_IDENT,
	TOK_REGISTER,
	TOK_NUMBER,
	TOK_STRING,
	TOK_HASH,
	TOK_COMMA,
	TOK_LBRACKET,
	TOK_RBRACKET,
	TOK_BANG,
	TOK_COLON,
	TOK_EQUALS,
	TOK_LO12,
	TOK_DOT,
	TOK_PLUS,
	TOK_MINUS,
	TOK_PERCENT,
} TokenKind;

typedef enum {
	REG_GP,
	REG_FP,
	REG_SP,
	REG_ZR,
} RegType;

typedef struct Token Token;
struct Token {
	TokenKind kind;
	Token *next;
	char *loc;
	int len;
	int line_no;
	int64_t val;
	char *str;
	int reg_num;
	int reg_width;
	RegType reg_type;
};

extern char *current_file;
extern char *current_input;

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);

Token *tokenize(char *input);
char *read_file(char *path);
void dump_tokens(Token *tok);

#endif
