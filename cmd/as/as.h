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

// Section identifiers
enum {
	SECTION_NONE = 0,
	SECTION_TEXT = 1,
	SECTION_DATA = 2,
	SECTION_BSS = 3,
};

// ELF symbol binding
enum {
	STB_LOCAL = 0,
	STB_GLOBAL = 1,
};

// ELF symbol type
enum {
	STT_NOTYPE = 0,
	STT_OBJECT = 1,
	STT_FUNC = 2,
};

typedef struct Symbol Symbol;
struct Symbol {
	char *name;
	int section;
	uint64_t value;
	int binding;
	int type;
	int size;
	int defined;
};

// symtab.c
void symtab_init(void);
Symbol *symtab_lookup(const char *name);
Symbol *symtab_add(const char *name);
void symtab_set_binding(Symbol *sym, int binding);
void symtab_set_type(Symbol *sym, int type);
int symtab_count(void);
Symbol *symtab_get(int index);

// parser.c
void pass1(Token *tok);
void dump_symbols(void);

// Condition codes for conditional branches
typedef enum {
	COND_EQ = 0,
	COND_NE = 1,
	COND_CS = 2,
	COND_CC = 3,
	COND_MI = 4,
	COND_PL = 5,
	COND_VS = 6,
	COND_VC = 7,
	COND_HI = 8,
	COND_LS = 9,
	COND_GE = 10,
	COND_LT = 11,
	COND_GT = 12,
	COND_LE = 13,
	COND_AL = 14,
} CondCode;

// encode.c
int encode_gpr(Token *tok);
int encode_fpr(Token *tok);
int encode_cond(const char *cond_str);

int encode_imm12(int64_t val, int *shift);
int encode_imm16(int64_t val);
int encode_branch26(int64_t offset);
int encode_branch19(int64_t offset);

uint32_t encode_add_reg(int sf, int rd, int rn, int rm);
uint32_t encode_add_imm(int sf, int rd, int rn, int imm12, int shift);
uint32_t encode_sub_reg(int sf, int rd, int rn, int rm);
uint32_t encode_sub_imm(int sf, int rd, int rn, int imm12, int shift);
uint32_t encode_mov_reg(int sf, int rd, int rm);
uint32_t encode_movz(int sf, int rd, int imm16, int hw);
uint32_t encode_movk(int sf, int rd, int imm16, int hw);
uint32_t encode_movn(int sf, int rd, int imm16, int hw);
uint32_t encode_b(int32_t offset);
uint32_t encode_bl(int32_t offset);
uint32_t encode_bcond(int cond, int32_t offset);
uint32_t encode_cbz(int sf, int rt, int32_t offset);
uint32_t encode_cbnz(int sf, int rt, int32_t offset);
uint32_t encode_ret(int rn);
uint32_t encode_blr(int rn);

void test_encode(void);

#endif
