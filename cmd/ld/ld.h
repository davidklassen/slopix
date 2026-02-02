#ifndef LD_H
#define LD_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
} Elf64_Phdr;

// ELF identification
#define ELFMAG0	    0x7f
#define ELFMAG1	    'E'
#define ELFMAG2	    'L'
#define ELFMAG3	    'F'
#define ELFCLASS64  2
#define ELFDATA2LSB 1
#define EV_CURRENT  1
#define ET_REL	    1
#define ET_EXEC	    2
#define EM_AARCH64  183

// Program header types
#define PT_NULL 0
#define PT_LOAD 1

// Program header flags
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

// Page alignment
#define PAGE_SIZE 0x1000

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

// ELF symbol binding
#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2

// ELF symbol type
#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4

// Extraction macros
#define ELF64_ST_BIND(info)	  ((info) >> 4)
#define ELF64_ST_TYPE(info)	  ((info) & 0xf)
#define ELF64_ST_INFO(bind, type) (((bind) << 4) | ((type) & 0xf))
#define ELF64_R_SYM(info)	  ((info) >> 32)
#define ELF64_R_TYPE(info)	  ((uint32_t)(info))
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

// Object file representation
typedef struct {
	char *filename;
	uint8_t *data;
	size_t size;
	Elf64_Ehdr *ehdr;
	Elf64_Shdr *shdrs;
	int shnum;
	char *shstrtab;
	char *strtab;
	Elf64_Sym *symtab;
	int symcount;
	int symtab_shndx;
	bool is_archive_member;
} ObjectFile;

// Archive format constants
#define AR_MAGIC     "!<arch>\n"
#define AR_MAGIC_LEN 8

// Archive member (parsed from 60-byte ar header)
typedef struct {
	char *name;
	uint8_t *data;
	size_t size;
	size_t offset;
	bool extracted;
} ArchiveMember;

// Archive symbol index entry
typedef struct {
	const char *name;
	int member_idx;
} ArchiveSymbol;

// Archive file
typedef struct {
	char *path;
	uint8_t *data;
	size_t size;
	ArchiveMember *members;
	int member_count;
	ArchiveSymbol *symbols;
	int symbol_count;
} Archive;

// String array for library paths
typedef struct {
	char **data;
	int len;
	int capacity;
} StringArray;

// Symbol table
#define SYMTAB_BUCKETS 4096

typedef struct Symbol Symbol;
struct Symbol {
	const char *name;
	uint64_t value;
	uint64_t size;
	uint8_t type;
	uint8_t binding;
	ObjectFile *file;
	uint16_t shndx;
	uint16_t output_shndx;
	Symbol *next;
};

typedef struct {
	Symbol *buckets[SYMTAB_BUCKETS];
	int count;
} SymbolTable;

// main.c
void error(char *fmt, ...);

// elf_read.c
ObjectFile *elf_read(const char *path);
ObjectFile *elf_read_memory(uint8_t *data, size_t size, const char *name);
void elf_free(ObjectFile *obj);
const char *section_name(ObjectFile *obj, int idx);
uint8_t *section_data(ObjectFile *obj, int idx);
const char *symbol_name(ObjectFile *obj, int idx);

// symbol.c
void symtab_init(SymbolTable *tab);
Symbol *symbol_lookup(SymbolTable *tab, const char *name);
Symbol *add_linker_symbol(SymbolTable *tab, const char *name, uint64_t value);
bool resolve_symbols(ObjectFile **objects, int count, SymbolTable *global);
void collect_definitions(ObjectFile **objects, int count, SymbolTable *global);
bool resolve_archives(ObjectFile ***objects, int *count, int *capacity, Archive **archives, int archive_count, SymbolTable *global, const char *entry_point, bool verbose);
bool check_undefined(ObjectFile **objects, int count, SymbolTable *global, const char *entry_point);
void dump_globals(SymbolTable *global);

// section.c

// Memory layout
#define TEXT_BASE     0x10000
#define SECTION_ALIGN 8

// Output section indices (userspace)
enum {
	OUT_NULL = 0,
	OUT_TEXT = 1,
	OUT_RODATA = 2,
	OUT_DATA = 3,
	OUT_BSS = 4,
	OUT_COUNT
};

// Output section indices (kernel)
enum {
	KOUT_NULL = 0,
	KOUT_TEXT_BOOT = 1,
	KOUT_TABLES = 2,
	KOUT_TEXT = 3,
	KOUT_RODATA = 4,
	KOUT_DATA = 5,
	KOUT_BSS = 6,
	KOUT_COUNT
};

// Output section indices (bootloader)
enum {
	BOUT_NULL = 0,
	BOUT_TEXT = 1,
	BOUT_RODATA = 2,
	BOUT_DATA = 3,
	BOUT_BSS = 4,
	BOUT_COUNT
};

// Bootloader memory layout
#define BOOT_TEXT_BASE 0x0ULL

// Kernel memory layout constants
#define KERNEL_PHYS_BASE  0x40080000ULL
#define KERNEL_VIRT_BASE  0xFFFF000040080000ULL
#define KERNEL_BOOT_SIZE  0x10000ULL
#define KERNEL_STACK_SIZE 0x10000ULL

// Section piece - tracks origin of merged content
typedef struct SectionPiece {
	ObjectFile *file;
	int input_shndx;
	uint64_t output_offset;
	uint64_t size;
} SectionPiece;

// Output section
typedef struct OutputSection {
	const char *name;
	uint64_t addr;
	uint64_t lma;
	uint64_t offset;
	uint64_t size;
	uint64_t alignment;
	uint32_t type;
	uint64_t flags;
	uint8_t *data;
	int piece_count;
	int piece_capacity;
	SectionPiece *pieces;
} OutputSection;

void output_section_init(OutputSection *sec, const char *name, uint32_t type, uint64_t flags);
int categorize_section(const char *name, uint64_t flags);
int categorize_section_kernel(const char *name, uint64_t flags);
void merge_sections(ObjectFile **objects, int count, OutputSection *sections, int section_count, int (*categorize)(const char *, uint64_t));
void assign_addresses(OutputSection *sections);
void assign_addresses_kernel(OutputSection *sections);
SectionPiece *find_piece(OutputSection *sections, int section_count, ObjectFile *file, int input_shndx);
void update_symbol_values(SymbolTable *global, OutputSection *sections, int section_count);
uint64_t resolve_local_symbol(ObjectFile *obj, int sym_idx, OutputSection *sections, int section_count);
void add_kernel_symbol_placeholders(SymbolTable *global);
void define_kernel_symbols(SymbolTable *global, OutputSection *sections);
void dump_output_sections(OutputSection *sections, int section_count);
int categorize_section_bootloader(const char *name, uint64_t flags);
void assign_addresses_bootloader(OutputSection *sections);

// reloc.c
bool apply_relocations(ObjectFile **objects, int count, SymbolTable *global, OutputSection *sections, int section_count);
const char *reloc_type_name(int type);

// output.c
bool write_executable(const char *path, OutputSection *sections, SymbolTable *global, const char *entry_point);
bool write_binary(const char *path, OutputSection *sections, int section_count);

// archive.c
Archive *archive_open(const char *path);
void archive_close(Archive *ar);
ObjectFile *archive_extract_member(Archive *ar, int member_idx);
int archive_find_symbol(Archive *ar, const char *name);

#endif
