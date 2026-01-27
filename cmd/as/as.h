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

// encode.c - helper functions
int encode_gpr(Token *tok);
int encode_fpr(Token *tok);
int encode_cond(const char *cond_str);

int encode_imm12(int64_t val, int *shift);
int encode_imm16(int64_t val);
int encode_imm9(int64_t val);
int encode_imm7(int64_t val, int scale);
int encode_imm12_unsigned(int64_t val, int scale);
int encode_imm21(int64_t val);
int invert_cond(int cond);
int encode_branch26(int64_t offset);
int encode_branch19(int64_t offset);

// encode.c - arithmetic
uint32_t encode_add_reg(int sf, int rd, int rn, int rm);
uint32_t encode_add_imm(int sf, int rd, int rn, int imm12, int shift);
uint32_t encode_sub_reg(int sf, int rd, int rn, int rm);
uint32_t encode_sub_imm(int sf, int rd, int rn, int imm12, int shift);
uint32_t encode_adds_reg(int sf, int rd, int rn, int rm);
uint32_t encode_adds_imm(int sf, int rd, int rn, int imm12, int shift);
uint32_t encode_subs_reg(int sf, int rd, int rn, int rm);
uint32_t encode_subs_imm(int sf, int rd, int rn, int imm12, int shift);
uint32_t encode_mul(int sf, int rd, int rn, int rm);
uint32_t encode_sdiv(int sf, int rd, int rn, int rm);
uint32_t encode_udiv(int sf, int rd, int rn, int rm);
uint32_t encode_madd(int sf, int rd, int rn, int rm, int ra);
uint32_t encode_msub(int sf, int rd, int rn, int rm, int ra);
uint32_t encode_neg(int sf, int rd, int rm);
uint32_t encode_negs(int sf, int rd, int rm);

// encode.c - logical
uint32_t encode_and_reg(int sf, int rd, int rn, int rm);
uint32_t encode_orr_reg(int sf, int rd, int rn, int rm);
uint32_t encode_eor_reg(int sf, int rd, int rn, int rm);
uint32_t encode_mvn(int sf, int rd, int rm);
uint32_t encode_bic(int sf, int rd, int rn, int rm);
uint32_t encode_ands_reg(int sf, int rd, int rn, int rm);
uint32_t encode_tst_reg(int sf, int rn, int rm);

// encode.c - shift (variable)
uint32_t encode_lsl_reg(int sf, int rd, int rn, int rm);
uint32_t encode_lsr_reg(int sf, int rd, int rn, int rm);
uint32_t encode_asr_reg(int sf, int rd, int rn, int rm);

// encode.c - move
uint32_t encode_mov_reg(int sf, int rd, int rm);
uint32_t encode_movz(int sf, int rd, int imm16, int hw);
uint32_t encode_movk(int sf, int rd, int imm16, int hw);
uint32_t encode_movn(int sf, int rd, int imm16, int hw);

// encode.c - compare and conditional
uint32_t encode_cmp_reg(int sf, int rn, int rm);
uint32_t encode_cmp_imm(int sf, int rn, int imm12, int shift);
uint32_t encode_cmn_reg(int sf, int rn, int rm);
uint32_t encode_cmn_imm(int sf, int rn, int imm12, int shift);
uint32_t encode_cset(int sf, int rd, int cond);
uint32_t encode_csetm(int sf, int rd, int cond);
uint32_t encode_csinc(int sf, int rd, int rn, int rm, int cond);
uint32_t encode_csel(int sf, int rd, int rn, int rm, int cond);
uint32_t encode_csinv(int sf, int rd, int rn, int rm, int cond);
uint32_t encode_csneg(int sf, int rd, int rn, int rm, int cond);
uint32_t encode_cinc(int sf, int rd, int rn, int cond);

// encode.c - extension
uint32_t encode_sxtb(int rd, int rn);
uint32_t encode_sxth(int rd, int rn);
uint32_t encode_sxtw(int rd, int rn);
uint32_t encode_uxtb(int rd, int rn);
uint32_t encode_uxth(int rd, int rn);

// encode.c - branch
uint32_t encode_b(int32_t offset);
uint32_t encode_bl(int32_t offset);
uint32_t encode_bcond(int cond, int32_t offset);
uint32_t encode_cbz(int sf, int rt, int32_t offset);
uint32_t encode_cbnz(int sf, int rt, int32_t offset);
uint32_t encode_ret(int rn);
uint32_t encode_blr(int rn);
uint32_t encode_br(int rn);

// encode.c - load/store unsigned offset
uint32_t encode_ldr_uoff(int sf, int rt, int rn, int64_t imm);
uint32_t encode_str_uoff(int sf, int rt, int rn, int64_t imm);
uint32_t encode_ldrb_uoff(int rt, int rn, int64_t imm);
uint32_t encode_strb_uoff(int rt, int rn, int64_t imm);
uint32_t encode_ldrh_uoff(int rt, int rn, int64_t imm);
uint32_t encode_strh_uoff(int rt, int rn, int64_t imm);
uint32_t encode_ldrsw_uoff(int rt, int rn, int64_t imm);
uint32_t encode_ldrsb64_uoff(int rt, int rn, int64_t imm);
uint32_t encode_ldrsb32_uoff(int rt, int rn, int64_t imm);
uint32_t encode_ldrsh64_uoff(int rt, int rn, int64_t imm);
uint32_t encode_ldrsh32_uoff(int rt, int rn, int64_t imm);

// encode.c - load/store pre/post-index
uint32_t encode_ldr_pre(int sf, int rt, int rn, int64_t imm);
uint32_t encode_str_pre(int sf, int rt, int rn, int64_t imm);
uint32_t encode_ldr_post(int sf, int rt, int rn, int64_t imm);
uint32_t encode_str_post(int sf, int rt, int rn, int64_t imm);

// encode.c - load/store unscaled
uint32_t encode_stur(int sf, int rt, int rn, int64_t imm);
uint32_t encode_ldur(int sf, int rt, int rn, int64_t imm);
uint32_t encode_sturb(int rt, int rn, int64_t imm);
uint32_t encode_ldurb(int rt, int rn, int64_t imm);
uint32_t encode_sturh(int rt, int rn, int64_t imm);
uint32_t encode_ldurh(int rt, int rn, int64_t imm);
uint32_t encode_ldursw(int rt, int rn, int64_t imm);
uint32_t encode_ldursb64(int rt, int rn, int64_t imm);
uint32_t encode_ldursb32(int rt, int rn, int64_t imm);
uint32_t encode_ldursh64(int rt, int rn, int64_t imm);
uint32_t encode_ldursh32(int rt, int rn, int64_t imm);

// encode.c - load/store pair
uint32_t encode_stp_pre(int sf, int rt1, int rt2, int rn, int64_t imm);
uint32_t encode_ldp_post(int sf, int rt1, int rt2, int rn, int64_t imm);
uint32_t encode_stp_off(int sf, int rt1, int rt2, int rn, int64_t imm);
uint32_t encode_ldp_off(int sf, int rt1, int rt2, int rn, int64_t imm);
uint32_t encode_stp_post(int sf, int rt1, int rt2, int rn, int64_t imm);
uint32_t encode_ldp_pre(int sf, int rt1, int rt2, int rn, int64_t imm);

// encode.c - load literal
uint32_t encode_ldr_literal(int sf, int rt, int64_t offset);

// encode.c - address generation
uint32_t encode_adrp(int rd, int64_t offset);
uint32_t encode_adr(int rd, int64_t offset);

// encode.c - miscellaneous
uint32_t encode_nop(void);

void test_encode(void);

#endif
