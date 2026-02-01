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
	TOK_LPAREN,
	TOK_RPAREN,
	TOK_PIPE,
	TOK_LSHIFT,
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
void warn(char *fmt, ...);
void warn_tok(Token *tok, char *fmt, ...);

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
	STT_SECTION = 3,
};

// ELF64 structures
typedef struct {
	unsigned char e_ident[16];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
	uint32_t sh_name;
	uint32_t sh_type;
	uint64_t sh_flags;
	uint64_t sh_addr;
	uint64_t sh_offset;
	uint64_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint64_t sh_addralign;
	uint64_t sh_entsize;
} Elf64_Shdr;

typedef struct {
	uint32_t st_name;
	uint8_t st_info;
	uint8_t st_other;
	uint16_t st_shndx;
	uint64_t st_value;
	uint64_t st_size;
} Elf64_Sym;

typedef struct {
	uint64_t r_offset;
	uint64_t r_info;
	int64_t r_addend;
} Elf64_Rela;

// ELF identification
#define ELFMAG0	    0x7f
#define ELFMAG1	    'E'
#define ELFMAG2	    'L'
#define ELFMAG3	    'F'
#define ELFCLASS64  2
#define ELFDATA2LSB 1
#define EV_CURRENT  1
#define ET_REL	    1
#define EM_AARCH64  183

// Section types
#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_NOBITS   8

// Section flags
#define SHF_WRITE     (1 << 0)
#define SHF_ALLOC     (1 << 1)
#define SHF_EXECINSTR (1 << 2)
#define SHF_INFO_LINK (1 << 6)

// Special section indices
#define SHN_UNDEF 0
#define SHN_ABS	  0xfff1

// Macros
#define ELF64_ST_INFO(bind, type) (((bind) << 4) | ((type) & 0xf))
#define ELF64_R_INFO(sym, type)	  (((uint64_t)(sym) << 32) | (type))

// AArch64 relocations
#define R_AARCH64_NONE		     0
#define R_AARCH64_ABS64		     257
#define R_AARCH64_ADR_PREL_LO21	     274
#define R_AARCH64_ADR_PREL_PG_HI21   275
#define R_AARCH64_ADD_ABS_LO12_NC    277
#define R_AARCH64_LDST8_ABS_LO12_NC  278
#define R_AARCH64_JUMP26	     282
#define R_AARCH64_CALL26	     283
#define R_AARCH64_LDST16_ABS_LO12_NC 284
#define R_AARCH64_LDST32_ABS_LO12_NC 285
#define R_AARCH64_LDST64_ABS_LO12_NC 286

// Section buffer
typedef struct {
	uint8_t *data;
	size_t size;
	size_t capacity;
} SectionBuf;

// String table
typedef struct {
	char *data;
	size_t size;
	size_t capacity;
} StringTable;

// Relocation entry
typedef struct Reloc Reloc;
struct Reloc {
	int section;
	uint64_t offset;
	int type;
	int symbol_idx;
	int64_t addend;
	Reloc *next;
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
	int explicit_local;
};

// Literal pool entry
typedef struct LiteralEntry LiteralEntry;
struct LiteralEntry {
	uint64_t value;
	char *symbol;
	uint64_t pool_offset;
	LiteralEntry *next;
};

// symtab.c
void symtab_init(void);
Symbol *symtab_lookup(const char *name);
Symbol *symtab_add(const char *name);
void symtab_set_binding(Symbol *sym, int binding);
void symtab_set_type(Symbol *sym, int type);
int symtab_count(void);
Symbol *symtab_get(int index);
int symtab_get_index(Symbol *sym);

// literal.c
void literal_pool_init(void);
LiteralEntry *literal_pool_add_value(uint64_t value);
LiteralEntry *literal_pool_add_symbol(const char *name);
int literal_pool_count(void);
uint64_t literal_pool_size(void);
LiteralEntry *literal_pool_get_list(void);

// parser.c
void pass1(Token *tok);
void pass2(Token *tok);
void dump_symbols(void);

// section.c
extern SectionBuf text_section;
extern SectionBuf data_section;
extern size_t bss_size;

void section_init(SectionBuf *sec);
void section_emit8(SectionBuf *sec, uint8_t val);
void section_emit32(SectionBuf *sec, uint32_t val);
void section_emit64(SectionBuf *sec, uint64_t val);
void section_align(SectionBuf *sec, int power);
void section_emit_zeros(SectionBuf *sec, size_t count);

// strtab.c
void strtab_init(StringTable *st);
uint32_t strtab_add(StringTable *st, const char *str);

// reloc.c
void reloc_init(void);
void reloc_add(int section, uint64_t offset, int type, int sym_idx, int64_t addend);
int reloc_count(int section);
Reloc *reloc_get_list(int section);

// elf_write.c
void elf_write(const char *filename);

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

// Logical immediate encoding
typedef struct {
	int n;
	int imms;
	int immr;
} LogicalImm;

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
int encode_logical_imm(int sf, uint64_t val, LogicalImm *out);

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

// encode.c - logical (register)
uint32_t encode_and_reg(int sf, int rd, int rn, int rm);
uint32_t encode_orr_reg(int sf, int rd, int rn, int rm);
uint32_t encode_eor_reg(int sf, int rd, int rn, int rm);
uint32_t encode_mvn(int sf, int rd, int rm);
uint32_t encode_bic(int sf, int rd, int rn, int rm);
uint32_t encode_ands_reg(int sf, int rd, int rn, int rm);
uint32_t encode_tst_reg(int sf, int rn, int rm);

// encode.c - logical (immediate)
uint32_t encode_and_imm(int sf, int rd, int rn, int n, int imms, int immr);
uint32_t encode_orr_imm(int sf, int rd, int rn, int n, int imms, int immr);
uint32_t encode_eor_imm(int sf, int rd, int rn, int n, int imms, int immr);
uint32_t encode_ands_imm(int sf, int rd, int rn, int n, int imms, int immr);
uint32_t encode_tst_imm(int sf, int rn, int n, int imms, int immr);

// encode.c - shift (variable)
uint32_t encode_lsl_reg(int sf, int rd, int rn, int rm);
uint32_t encode_lsr_reg(int sf, int rd, int rn, int rm);
uint32_t encode_asr_reg(int sf, int rd, int rn, int rm);

// encode.c - shift (immediate)
uint32_t encode_lsl_imm(int sf, int rd, int rn, int shift);
uint32_t encode_lsr_imm(int sf, int rd, int rn, int shift);
uint32_t encode_asr_imm(int sf, int rd, int rn, int shift);

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
uint32_t encode_stur_fp(int ftype, int ft, int rn, int64_t imm);
uint32_t encode_ldur_fp(int ftype, int ft, int rn, int64_t imm);
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
uint32_t encode_svc(int imm16);

// encode.c - floating-point
uint32_t encode_fmov_gpr_to_fpr(int sf, int fd, int rn);
uint32_t encode_fadd(int ftype, int fd, int fn, int fm);
uint32_t encode_fsub(int ftype, int fd, int fn, int fm);
uint32_t encode_fmul(int ftype, int fd, int fn, int fm);
uint32_t encode_fdiv(int ftype, int fd, int fn, int fm);
uint32_t encode_fneg(int ftype, int fd, int fn);
uint32_t encode_fcmp_reg(int ftype, int fn, int fm);
uint32_t encode_fcmp_zero(int ftype, int fn);
uint32_t encode_scvtf(int sf, int ftype, int fd, int rn);
uint32_t encode_ucvtf(int sf, int ftype, int fd, int rn);
uint32_t encode_fcvtzs(int sf, int ftype, int rd, int fn);
uint32_t encode_fcvtzu(int sf, int ftype, int rd, int fn);
uint32_t encode_fcvt_d_s(int fd, int fn);
uint32_t encode_fcvt_s_d(int fd, int fn);
uint32_t encode_ldr_fp_uoff(int ftype, int ft, int rn, int64_t imm);
uint32_t encode_str_fp_uoff(int ftype, int ft, int rn, int64_t imm);
uint32_t encode_ldr_fp_pre(int ftype, int ft, int rn, int64_t imm);
uint32_t encode_str_fp_pre(int ftype, int ft, int rn, int64_t imm);
uint32_t encode_ldr_fp_post(int ftype, int ft, int rn, int64_t imm);
uint32_t encode_str_fp_post(int ftype, int ft, int rn, int64_t imm);

void test_encode(void);

#endif
