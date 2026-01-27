# Assembler Roadmap (Phase 3 of CC.md)

A detailed implementation plan for building an AArch64 assembler for Slopix.

## Overview

**Goal**: Build `cmd/as` that assembles AArch64 `.s` files into ELF `.o` relocatable objects.

**Scope**: This roadmap covers Phase 3 only - an assembler that:
- Parses the subset of AArch64 assembly that chibicc emits
- Encodes instructions into 32-bit machine code
- Emits ELF64 relocatable object files with proper relocations
- Runs on the host initially, later natively on Slopix

**Key References**:
- [ARM A64 Instruction Set Architecture](https://developer.arm.com/documentation/ddi0602/latest/)
- [ELF for the Arm 64-bit Architecture](docs/aaelf64/aaelf64.md)
- [CAS-Atlantic/AArch64-Encoding](https://github.com/CAS-Atlantic/AArch64-Encoding) - Instruction encoding data
- [Keystone Engine](https://www.keystone-engine.org/) - Multi-arch assembler framework
- [Two-pass assembler algorithm](http://users.cis.fiu.edu/~downeyt/cop3402/two-pass.htm)

---

## Chibicc Assembly Output Analysis

Analysis of `tools/cc/codegen.c` and generated `.s` files reveals the exact subset we must support.

### Instructions Required (~45 unique mnemonics)

| Category | Instructions |
|----------|-------------|
| **Load/Store** | ldr, str, ldrsw, ldrb, ldrsb, ldrh, ldrsh, strb, strh, stur, sturb, sturh, stp, ldp |
| **Arithmetic** | add, sub, mul, sdiv, udiv, msub, neg |
| **Logical** | and, orr, eor, mvn, bic |
| **Shift** | lsl, lsr, asr |
| **Compare** | cmp, cset, fcmp |
| **Branch** | b, b.eq, b.ne, b.lt, b.le, b.lo, b.ls, b.mi, bl, blr, ret, cbz, cbnz |
| **Extension** | sxtb, sxth, sxtw, uxtb, uxth |
| **Move** | mov, movz, movk, movn |
| **Address** | adrp, adr |
| **Float** | fmov, fneg, fcvtzs, fcvtzu, scvtf, ucvtf, fadd, fsub, fmul, fdiv, fcvt |

### Directives Required

| Directive | Usage | Description |
|-----------|-------|-------------|
| `.text` | Section | Switch to code section |
| `.data` | Section | Switch to data section |
| `.bss` | Section | Switch to uninitialized data section |
| `.globl` | Symbol | Export symbol globally |
| `.local` | Symbol | Mark symbol as local |
| `.type` | Symbol | Set symbol type (`%function`, `%object`) |
| `.size` | Symbol | Set symbol size |
| `.align` | Alignment | Align to N bytes |
| `.byte` | Data | Emit 1-byte value |
| `.word` | Data | Emit 4-byte value (32-bit) |
| `.xword` | Data | Emit 8-byte value (64-bit) |
| `.zero` | Data | Emit N zero bytes |
| `.file` | Debug | Source file annotation (can ignore) |
| `.loc` | Debug | Source location (can ignore) |

### Addressing Modes

1. **Immediate**: `mov x0, #42`
2. **Register**: `mov x0, x1`
3. **Register offset**: `ldr x0, [x1, #8]`
4. **Pre-indexed**: `str x0, [sp, #-16]!`
5. **Post-indexed**: `ldr x0, [sp], #16`
6. **PC-relative**: `adrp x0, symbol` + `add x0, x0, :lo12:symbol`
7. **Literal pool**: `ldr x0, =constant`

### Relocations Required

| Relocation | Code | Used For |
|------------|------|----------|
| `R_AARCH64_ABS64` | 257 | 64-bit data (.xword symbol) |
| `R_AARCH64_ADR_PREL_PG_HI21` | 275 | ADRP instruction |
| `R_AARCH64_ADD_ABS_LO12_NC` | 277 | ADD with :lo12: modifier |
| `R_AARCH64_CALL26` | 283 | BL instruction |
| `R_AARCH64_JUMP26` | 282 | B instruction |
| `R_AARCH64_LDST64_ABS_LO12_NC` | 286 | LDR/STR 64-bit with :lo12: |
| `R_AARCH64_LDST32_ABS_LO12_NC` | 285 | LDR/STR 32-bit with :lo12: |
| `R_AARCH64_LDST8_ABS_LO12_NC` | 278 | LDRB/STRB with :lo12: |

---

## Architecture

### Two-Pass Algorithm

**Pass 1 (Symbol Collection)**:
1. Scan through all lines
2. Track current section (.text, .data, .bss)
3. Track location counter for each section
4. Record label addresses in symbol table
5. Estimate instruction sizes (always 4 bytes for AArch64)
6. Record data sizes from directives

**Pass 2 (Code Generation)**:
1. Reset location counters
2. For each instruction:
   - Look up opcode in instruction table
   - Encode operands into 32-bit instruction
   - Generate relocations for symbolic references
3. For each directive:
   - Emit data bytes
   - Generate relocations for symbolic data

### Data Structures

```c
// Symbol table entry
typedef struct {
    char *name;
    int section;          // SECTION_TEXT, SECTION_DATA, SECTION_BSS
    uint64_t value;       // Offset within section
    int binding;          // STB_LOCAL, STB_GLOBAL
    int type;             // STT_NOTYPE, STT_FUNC, STT_OBJECT
    int size;             // Symbol size (from .size directive)
    int defined;          // 1 if defined, 0 if external reference
} Symbol;

// Relocation entry
typedef struct {
    int section;          // Section containing the relocation
    uint64_t offset;      // Offset within section
    int type;             // R_AARCH64_* relocation type
    int symbol_idx;       // Symbol table index
    int64_t addend;       // Relocation addend
} Reloc;

// Section data
typedef struct {
    uint8_t *data;        // Section content
    size_t size;          // Current size
    size_t capacity;      // Allocated capacity
    uint64_t alignment;   // Section alignment
} Section;
```

---

## Step 1: Project Setup and Lexer

**Goal**: Create project structure and tokenize assembly input.

### Work

1. Create `cmd/as/` directory structure:
   ```
   cmd/as/
   ├── Makefile
   ├── as.h           # Main header with types
   ├── main.c         # Entry point, file I/O
   ├── lexer.c        # Tokenizer
   ├── parser.c       # Two-pass parser
   ├── encode.c       # Instruction encoding
   ├── elf.c          # ELF output generation
   └── symtab.c       # Symbol table management
   ```

2. Implement lexer that recognizes:
   - Labels: `name:` or `.L.name:`
   - Instructions: `add`, `ldr`, etc.
   - Directives: `.text`, `.globl`, `.byte`, etc.
   - Registers: `x0-x30`, `w0-w30`, `sp`, `xzr`, `wzr`, `d0-d31`, `s0-s31`
   - Numbers: decimal, hex (`0x...`), negative
   - Strings: `"..."` with escape sequences
   - Operators: `#`, `,`, `[`, `]`, `!`, `:lo12:`, `=`
   - Comments: `//` and `/* */`

3. Token types:
   ```c
   typedef enum {
       TOK_EOF,
       TOK_NEWLINE,
       TOK_LABEL,
       TOK_IDENT,      // Instruction or directive name
       TOK_REGISTER,
       TOK_NUMBER,
       TOK_STRING,
       TOK_HASH,       // #
       TOK_COMMA,
       TOK_LBRACKET,   // [
       TOK_RBRACKET,   // ]
       TOK_BANG,       // ! (pre-index)
       TOK_COLON,      // :
       TOK_EQUALS,     // =
       TOK_LO12,       // :lo12:
   } TokenKind;
   ```

### Testing Strategy

```bash
# Test lexer on simple input
echo 'main:
    mov x0, #42
    ret' | cmd/as/as -
```

### Exit Criteria

1. Lexer tokenizes all chibicc output correctly
2. Handles all register names and addressing mode syntax
3. Recognizes `:lo12:` relocation modifier

---

## Step 2: Symbol Table and Pass 1

**Goal**: Implement symbol table and first pass to collect labels.

### Work

1. Implement symbol table (`symtab.c`):
   ```c
   void symtab_init(void);
   Symbol *symtab_lookup(const char *name);
   Symbol *symtab_add(const char *name);
   void symtab_set_binding(Symbol *sym, int binding);
   void symtab_set_type(Symbol *sym, int type);
   int symtab_count(void);
   Symbol *symtab_get(int index);
   ```

2. Implement Pass 1 in `parser.c`:
   - Track current section (text/data/bss)
   - Track location counter per section
   - For labels: add to symbol table with current address
   - For `.globl`/`.local`: set symbol binding
   - For `.type`: set symbol type
   - For `.size`: set symbol size
   - For instructions: advance LC by 4
   - For `.byte`: advance LC by 1
   - For `.word`: advance LC by 4
   - For `.xword`: advance LC by 8
   - For `.zero N`: advance LC by N
   - For `.align N`: align LC to N

3. Handle forward references:
   - Symbols may be used before definition
   - Pass 1 only records definitions
   - Pass 2 resolves references

### Testing Strategy

```c
// Test symbol collection
// Input:
//   .globl main
//   main:
//       bl helper
//   helper:
//       ret

// After Pass 1:
// main: section=text, offset=0, binding=GLOBAL, type=FUNC
// helper: section=text, offset=4, binding=LOCAL, type=FUNC
```

### Exit Criteria

1. Symbol table stores all labels with correct offsets
2. Handles multiple sections correctly
3. Forward references don't cause errors in Pass 1

---

## Step 3: Instruction Encoding Framework

**Goal**: Create encoding infrastructure for AArch64 instructions.

### AArch64 Instruction Format

All AArch64 instructions are 32 bits. The encoding is determined by bits [28:25]:

| Bits [28:25] | Category |
|--------------|----------|
| 100x | Data processing (immediate) |
| x101 | Data processing (register) |
| x1x0 | Loads and stores |
| 101x | Branches |

### Work

1. Create instruction table (`encode.c`):
   ```c
   typedef struct {
       const char *mnemonic;
       uint32_t opcode_base;
       int format;           // FMT_REG3, FMT_REG2_IMM, etc.
       int (*encode)(Instr *instr, uint32_t *out);
   } InstrDef;

   static InstrDef instr_table[] = {
       {"add",  0x8B000000, FMT_REG3,     encode_add_reg},
       {"add",  0x91000000, FMT_REG2_IMM, encode_add_imm},
       {"sub",  0xCB000000, FMT_REG3,     encode_sub_reg},
       {"sub",  0xD1000000, FMT_REG2_IMM, encode_sub_imm},
       {"mov",  0xAA0003E0, FMT_REG2,     encode_mov_reg},
       {"mov",  0xD2800000, FMT_REG_IMM,  encode_movz},
       // ... etc
   };
   ```

2. Implement register encoding:
   ```c
   // x0-x30 -> 0-30, sp -> 31 (context dependent), xzr -> 31
   int encode_gpr64(const char *name);
   // w0-w30 -> 0-30, wsp -> 31, wzr -> 31
   int encode_gpr32(const char *name);
   // d0-d31 -> 0-31
   int encode_fpr64(const char *name);
   // s0-s31 -> 0-31
   int encode_fpr32(const char *name);
   ```

3. Implement immediate encoding:
   ```c
   // 12-bit unsigned for ADD/SUB immediate
   int encode_imm12(int64_t val, uint32_t *out);
   // 16-bit for MOVZ/MOVK with shift
   int encode_imm16(int64_t val, int shift, uint32_t *out);
   // 26-bit signed offset for B/BL (divided by 4)
   int encode_branch26(int64_t offset, uint32_t *out);
   // 19-bit signed offset for conditional branch
   int encode_branch19(int64_t offset, uint32_t *out);
   ```

### Instruction Encoding Examples

```c
// ADD Xd, Xn, Xm (64-bit register)
// 31 30 29 28 27 26 25 24 23 22 21 20    16 15    10 9    5 4    0
// sf  0  0  0  1  0  1  1  sh  0  0  Rm      imm6     Rn      Rd
// sf=1 for 64-bit
uint32_t encode_add_reg(int rd, int rn, int rm) {
    return 0x8B000000 | (rm << 16) | (rn << 5) | rd;
}

// ADD Xd, Xn, #imm (64-bit immediate)
// 31 30 29 28 27 26 25 24 23 22 21        10 9    5 4    0
// sf  0  0  1  0  0  0  1  sh     imm12        Rn      Rd
uint32_t encode_add_imm(int rd, int rn, int imm12, int shift) {
    return 0x91000000 | (shift << 22) | (imm12 << 10) | (rn << 5) | rd;
}

// B.cond offset
// 31 30 29 28 27 26 25 24 23                5 4 3    0
//  0  1  0  1  0  1  0  0       imm19          0 cond
uint32_t encode_bcond(int cond, int32_t offset) {
    int imm19 = (offset >> 2) & 0x7FFFF;
    return 0x54000000 | (imm19 << 5) | cond;
}

// BL offset
// 31 30 29 28 27 26 25                                  0
//  1  0  0  1  0  1               imm26
uint32_t encode_bl(int32_t offset) {
    int imm26 = (offset >> 2) & 0x3FFFFFF;
    return 0x94000000 | imm26;
}
```

### Exit Criteria

1. Can encode basic data processing instructions
2. Register encoding handles all register names
3. Immediate encoding handles common ranges

---

## Step 4: Core Instruction Encoding

**Goal**: Implement encoding for all instructions chibicc generates.

### Work

1. **Data Movement**:
   ```c
   // MOV (register) - actually ORR with zero register
   // MOV Xd, Xn -> ORR Xd, XZR, Xn
   encode_mov_reg(rd, rn) -> 0xAA0003E0 | (rn << 16) | rd

   // MOVZ Xd, #imm{, LSL #shift}
   encode_movz(rd, imm16, shift) -> 0xD2800000 | (shift/16 << 21) | (imm16 << 5) | rd

   // MOVK Xd, #imm{, LSL #shift}
   encode_movk(rd, imm16, shift) -> 0xF2800000 | (shift/16 << 21) | (imm16 << 5) | rd

   // MOVN Xd, #imm{, LSL #shift}
   encode_movn(rd, imm16, shift) -> 0x92800000 | (shift/16 << 21) | (imm16 << 5) | rd
   ```

2. **Load/Store**:
   ```c
   // LDR Xt, [Xn, #offset] (unsigned offset)
   // Size=11, opc=01 for 64-bit load
   encode_ldr_uoff(rt, rn, offset) -> 0xF9400000 | ((offset/8) << 10) | (rn << 5) | rt

   // STR Xt, [Xn, #offset] (unsigned offset)
   encode_str_uoff(rt, rn, offset) -> 0xF9000000 | ((offset/8) << 10) | (rn << 5) | rt

   // LDR Xt, [Xn], #offset (post-index)
   encode_ldr_post(rt, rn, offset) -> 0xF8400400 | ((offset & 0x1FF) << 12) | (rn << 5) | rt

   // STR Xt, [Xn, #offset]! (pre-index)
   encode_str_pre(rt, rn, offset) -> 0xF8000C00 | ((offset & 0x1FF) << 12) | (rn << 5) | rt

   // STP Xt1, Xt2, [Xn, #offset]!
   encode_stp_pre(rt1, rt2, rn, offset) -> 0xA9800000 | ((offset/8 & 0x7F) << 15) | (rt2 << 10) | (rn << 5) | rt1

   // LDP Xt1, Xt2, [Xn], #offset
   encode_ldp_post(rt1, rt2, rn, offset) -> 0xA8C00000 | ((offset/8 & 0x7F) << 15) | (rt2 << 10) | (rn << 5) | rt1
   ```

3. **Arithmetic**:
   ```c
   // MUL Xd, Xn, Xm -> MADD Xd, Xn, Xm, XZR
   encode_mul(rd, rn, rm) -> 0x9B007C00 | (rm << 16) | (rn << 5) | rd

   // SDIV Xd, Xn, Xm
   encode_sdiv(rd, rn, rm) -> 0x9AC00C00 | (rm << 16) | (rn << 5) | rd

   // MSUB Xd, Xn, Xm, Xa -> Xd = Xa - Xn*Xm
   encode_msub(rd, rn, rm, ra) -> 0x9B008000 | (rm << 16) | (ra << 10) | (rn << 5) | rd

   // NEG Xd, Xm -> SUB Xd, XZR, Xm
   encode_neg(rd, rm) -> 0xCB0003E0 | (rm << 16) | rd
   ```

4. **Compare and Conditional**:
   ```c
   // CMP Xn, Xm -> SUBS XZR, Xn, Xm
   encode_cmp_reg(rn, rm) -> 0xEB00001F | (rm << 16) | (rn << 5)

   // CMP Xn, #imm -> SUBS XZR, Xn, #imm
   encode_cmp_imm(rn, imm12) -> 0xF100001F | (imm12 << 10) | (rn << 5)

   // CSET Xd, cond -> CSINC Xd, XZR, XZR, invert(cond)
   encode_cset(rd, cond) -> 0x9A9F07E0 | (invert_cond(cond) << 12) | rd
   ```

5. **Branch**:
   ```c
   // B offset
   encode_b(offset) -> 0x14000000 | ((offset >> 2) & 0x3FFFFFF)

   // BL offset
   encode_bl(offset) -> 0x94000000 | ((offset >> 2) & 0x3FFFFFF)

   // B.cond offset
   encode_bcond(cond, offset) -> 0x54000000 | (((offset >> 2) & 0x7FFFF) << 5) | cond

   // BLR Xn
   encode_blr(rn) -> 0xD63F0000 | (rn << 5)

   // RET {Xn} (default x30)
   encode_ret(rn) -> 0xD65F0000 | (rn << 5)

   // CBZ Xt, offset
   encode_cbz(rt, offset) -> 0xB4000000 | (((offset >> 2) & 0x7FFFF) << 5) | rt
   ```

6. **Extension**:
   ```c
   // SXTB Xd, Wn -> SBFM Xd, Xn, #0, #7
   encode_sxtb(rd, rn) -> 0x93401C00 | (rn << 5) | rd

   // SXTH Xd, Wn -> SBFM Xd, Xn, #0, #15
   encode_sxth(rd, rn) -> 0x93403C00 | (rn << 5) | rd

   // SXTW Xd, Wn -> SBFM Xd, Xn, #0, #31
   encode_sxtw(rd, rn) -> 0x93407C00 | (rn << 5) | rd
   ```

7. **Address Generation**:
   ```c
   // ADRP Xd, label (page address)
   // Relocation fills in immediate
   encode_adrp(rd) -> 0x90000000 | rd

   // ADR Xd, label
   encode_adr(rd) -> 0x10000000 | rd
   ```

### Condition Codes

```c
enum {
    COND_EQ = 0,   // Equal
    COND_NE = 1,   // Not equal
    COND_CS = 2,   // Carry set (HS - unsigned >=)
    COND_CC = 3,   // Carry clear (LO - unsigned <)
    COND_MI = 4,   // Minus/negative
    COND_PL = 5,   // Plus/positive
    COND_VS = 6,   // Overflow
    COND_VC = 7,   // No overflow
    COND_HI = 8,   // Unsigned higher
    COND_LS = 9,   // Unsigned lower or same
    COND_GE = 10,  // Signed >=
    COND_LT = 11,  // Signed <
    COND_GT = 12,  // Signed >
    COND_LE = 13,  // Signed <=
    COND_AL = 14,  // Always
};
```

### Exit Criteria

1. All data processing instructions encode correctly
2. All load/store variants work (register, immediate, pre/post-index)
3. All branch types encode correctly
4. Extension instructions work

---

## Step 5: Floating-Point Instructions

**Goal**: Implement encoding for floating-point instructions.

### Work

1. **Float Move**:
   ```c
   // FMOV Dd, Xn (general to float)
   encode_fmov_gpr_to_fpr(fd, rn) -> 0x9E670000 | (rn << 5) | fd

   // FMOV Xd, Dn (float to general)
   encode_fmov_fpr_to_gpr(rd, fn) -> 0x9E660000 | (fn << 5) | rd

   // FMOV Dd, Dm (float to float)
   encode_fmov_fpr(fd, fm) -> 0x1E604000 | (fm << 5) | fd
   ```

2. **Float Arithmetic**:
   ```c
   // FADD Dd, Dn, Dm
   encode_fadd_d(fd, fn, fm) -> 0x1E602800 | (fm << 16) | (fn << 5) | fd

   // FSUB Dd, Dn, Dm
   encode_fsub_d(fd, fn, fm) -> 0x1E603800 | (fm << 16) | (fn << 5) | fd

   // FMUL Dd, Dn, Dm
   encode_fmul_d(fd, fn, fm) -> 0x1E600800 | (fm << 16) | (fn << 5) | fd

   // FDIV Dd, Dn, Dm
   encode_fdiv_d(fd, fn, fm) -> 0x1E601800 | (fm << 16) | (fn << 5) | fd

   // FNEG Dd, Dn
   encode_fneg_d(fd, fn) -> 0x1E614000 | (fn << 5) | fd
   ```

3. **Float Compare**:
   ```c
   // FCMP Dn, Dm
   encode_fcmp_d(fn, fm) -> 0x1E602000 | (fm << 16) | (fn << 5)

   // FCMP Dn, #0.0
   encode_fcmp_zero_d(fn) -> 0x1E602008 | (fn << 5)
   ```

4. **Float Conversion**:
   ```c
   // SCVTF Dd, Xn (signed int to double)
   encode_scvtf_d(fd, rn) -> 0x9E620000 | (rn << 5) | fd

   // UCVTF Dd, Xn (unsigned int to double)
   encode_ucvtf_d(fd, rn) -> 0x9E630000 | (rn << 5) | fd

   // FCVTZS Xd, Dn (double to signed int, truncate)
   encode_fcvtzs_d(rd, fn) -> 0x9E780000 | (fn << 5) | rd

   // FCVTZU Xd, Dn (double to unsigned int, truncate)
   encode_fcvtzu_d(rd, fn) -> 0x9E790000 | (fn << 5) | rd

   // FCVT Sd, Dn (double to single)
   encode_fcvt_s_d(fd, fn) -> 0x1E624000 | (fn << 5) | fd

   // FCVT Dd, Sn (single to double)
   encode_fcvt_d_s(fd, fn) -> 0x1E22C000 | (fn << 5) | fd
   ```

5. **Float Load/Store**:
   ```c
   // LDR Dt, [Xn, #offset]
   encode_ldr_d(ft, rn, offset) -> 0xFD400000 | ((offset/8) << 10) | (rn << 5) | ft

   // STR Dt, [Xn, #offset]
   encode_str_d(ft, rn, offset) -> 0xFD000000 | ((offset/8) << 10) | (rn << 5) | ft
   ```

### Exit Criteria

1. Float arithmetic instructions encode correctly
2. Float conversion instructions work
3. Float load/store with various addressing modes

---

## Step 6: ELF Output Generation

**Goal**: Generate valid ELF64 relocatable object files.

### ELF Structure

```
+------------------+
| ELF Header       | 64 bytes
+------------------+
| Section Data     |
|   .text          | Code bytes
|   .data          | Initialized data
|   .bss           | (no data, just size)
|   .rodata        | Read-only data (strings)
+------------------+
| Section Headers  | 64 bytes each
|   NULL           | Required first entry
|   .text          |
|   .data          |
|   .bss           |
|   .rodata        |
|   .symtab        |
|   .strtab        |
|   .shstrtab      |
|   .rela.text     |
|   .rela.data     |
+------------------+
```

### Work

1. **ELF Header** (`elf.c`):
   ```c
   typedef struct {
       uint8_t  e_ident[16];  // Magic, class, endian, etc.
       uint16_t e_type;       // ET_REL (1)
       uint16_t e_machine;    // EM_AARCH64 (183)
       uint32_t e_version;    // 1
       uint64_t e_entry;      // 0 for relocatable
       uint64_t e_phoff;      // 0 for relocatable
       uint64_t e_shoff;      // Section header offset
       uint32_t e_flags;      // 0
       uint16_t e_ehsize;     // 64
       uint16_t e_phentsize;  // 0
       uint16_t e_phnum;      // 0
       uint16_t e_shentsize;  // 64
       uint16_t e_shnum;      // Number of sections
       uint16_t e_shstrndx;   // Index of .shstrtab
   } Elf64_Ehdr;

   void write_elf_header(FILE *out, int shnum, int shstrndx, uint64_t shoff);
   ```

2. **Section Headers**:
   ```c
   typedef struct {
       uint32_t sh_name;      // Offset in .shstrtab
       uint32_t sh_type;      // SHT_PROGBITS, SHT_SYMTAB, etc.
       uint64_t sh_flags;     // SHF_ALLOC, SHF_EXECINSTR, etc.
       uint64_t sh_addr;      // 0 for relocatable
       uint64_t sh_offset;    // File offset
       uint64_t sh_size;      // Section size
       uint32_t sh_link;      // Linked section index
       uint32_t sh_info;      // Extra info
       uint64_t sh_addralign; // Alignment
       uint64_t sh_entsize;   // Entry size (for symtab, rela)
   } Elf64_Shdr;
   ```

3. **Symbol Table**:
   ```c
   typedef struct {
       uint32_t st_name;      // Offset in .strtab
       uint8_t  st_info;      // Type and binding
       uint8_t  st_other;     // Visibility
       uint16_t st_shndx;     // Section index
       uint64_t st_value;     // Symbol value (offset in section)
       uint64_t st_size;      // Symbol size
   } Elf64_Sym;

   #define ELF64_ST_INFO(bind, type) (((bind) << 4) | ((type) & 0xF))
   #define STB_LOCAL  0
   #define STB_GLOBAL 1
   #define STT_NOTYPE 0
   #define STT_OBJECT 1
   #define STT_FUNC   2
   ```

4. **Relocation Entries**:
   ```c
   typedef struct {
       uint64_t r_offset;     // Offset in section
       uint64_t r_info;       // Symbol index and type
       int64_t  r_addend;     // Addend
   } Elf64_Rela;

   #define ELF64_R_INFO(sym, type) (((uint64_t)(sym) << 32) | (type))
   ```

5. **Mapping Symbols**:
   - Add `$x` at start of code sequences
   - Add `$d` at start of data sequences
   - Required for proper disassembly

### Exit Criteria

1. Generated .o files pass `aarch64-elf-readelf -a`
2. Symbol table contains all symbols with correct attributes
3. Relocation entries reference correct symbols
4. String tables are properly formatted

---

## Step 7: Relocation Handling

**Goal**: Generate correct relocations for symbolic references.

### Work

1. **ADRP + ADD Pair**:
   ```c
   // adrp x0, symbol
   // -> R_AARCH64_ADR_PREL_PG_HI21 at current offset, addend=0

   // add x0, x0, :lo12:symbol
   // -> R_AARCH64_ADD_ABS_LO12_NC at current offset, addend=0
   ```

2. **Branch Instructions**:
   ```c
   // bl function
   // -> R_AARCH64_CALL26 at current offset, addend=0

   // b label
   // -> If label is local and within range: encode directly
   // -> If label is external: R_AARCH64_JUMP26
   ```

3. **Data Relocations**:
   ```c
   // .xword symbol
   // -> R_AARCH64_ABS64 at current offset, addend=0

   // .xword symbol + 8
   // -> R_AARCH64_ABS64 at current offset, addend=8
   ```

4. **Local vs External Symbols**:
   - Local labels (`.L...`): resolve during assembly if possible
   - Global symbols: always generate relocation
   - Undefined symbols: generate relocation, linker resolves

5. **Relocation Calculation** (for reference):
   ```
   R_AARCH64_ADR_PREL_PG_HI21:
       X = Page(S + A) - Page(P)
       Encode bits [32:12] of X into ADRP immediate

   R_AARCH64_ADD_ABS_LO12_NC:
       X = S + A
       Encode bits [11:0] of X into ADD immediate

   R_AARCH64_CALL26:
       X = S + A - P
       Check -2^27 <= X < 2^27
       Encode bits [27:2] of X into BL immediate
   ```

### Exit Criteria

1. ADRP/ADD pairs generate correct relocation pair
2. Branch relocations generated for external symbols
3. Data relocations work for .xword directives
4. Linker can process generated relocations

---

## Step 8: Literal Pools

**Goal**: Implement `ldr reg, =constant` pseudo-instruction.

### Design

When chibicc generates `ldr x0, =constant`, the assembler must:
1. Allocate space in a literal pool (in .rodata or end of .text)
2. Emit `ldr x0, [pc, #offset]` instruction
3. Generate relocation if constant is a symbol

### Work

1. **Literal Pool Management**:
   ```c
   typedef struct {
       uint64_t value;        // Constant value
       int is_symbol;         // 1 if symbol reference
       char *symbol_name;     // Symbol name if is_symbol
       uint64_t offset;       // Offset in literal pool
   } LiteralEntry;

   void literal_pool_add(uint64_t value);
   void literal_pool_add_symbol(const char *name);
   void literal_pool_emit(void);
   ```

2. **LDR Literal Encoding**:
   ```c
   // LDR Xt, label (literal)
   // 31 30 29 28 27 26 25 24 23              5 4    0
   // 0  1  0  1  1  0  0  0       imm19         Rt
   encode_ldr_literal(rt, offset) -> 0x58000000 | (((offset >> 2) & 0x7FFFF) << 5) | rt
   ```

3. **Placement Strategy**:
   - Literal pool at end of function (before next function)
   - Or at end of .text section
   - Must be within ±1MB of LDR instruction

### Exit Criteria

1. `ldr x0, =42` works for small constants
2. `ldr x0, =0x123456789` works for large constants
3. `ldr x0, =symbol` generates relocation
4. Literal pools placed within range of LDR instructions

---

## Step 9: Directive Processing

**Goal**: Implement all assembler directives.

### Work

1. **Section Directives**:
   ```c
   void handle_text(void);    // Switch to .text
   void handle_data(void);    // Switch to .data
   void handle_bss(void);     // Switch to .bss
   ```

2. **Symbol Directives**:
   ```c
   void handle_globl(const char *name);   // Mark global
   void handle_local(const char *name);   // Mark local
   void handle_type(const char *name, const char *type);  // Set type
   void handle_size(const char *name, int size);          // Set size
   ```

3. **Data Directives**:
   ```c
   void handle_byte(int64_t value);       // Emit 1 byte
   void handle_word(int64_t value);       // Emit 4 bytes
   void handle_xword(int64_t value);      // Emit 8 bytes
   void handle_xword_symbol(const char *name, int64_t addend);
   void handle_zero(int count);           // Emit N zero bytes
   void handle_align(int alignment);      // Align to N bytes
   void handle_string(const char *str);   // Emit null-terminated string
   ```

4. **Debug Directives** (can ignore):
   ```c
   void handle_file(int num, const char *path);  // .file
   void handle_loc(int file, int line);          // .loc
   ```

### String Escape Sequences

```c
// chibicc emits strings as .byte sequences
// .byte 104, 101, 108, 108, 111, 0  // "hello"

// But may also use .string or .ascii
// Handle escape sequences: \n, \t, \r, \\, \", \0, \xNN
```

### Exit Criteria

1. Section switching works correctly
2. Symbol attributes set properly
3. Data emission with correct sizes
4. Alignment padding inserted correctly

---

## Step 10: Integration and Testing

**Goal**: Test assembler against chibicc output.

### Testing Strategy

1. **Differential Testing**:
   ```bash
   # Generate assembly with chibicc
   tools/cc/chibicc -S test.c -o test.s

   # Assemble with our assembler
   cmd/as/as test.s -o test-ours.o

   # Assemble with GNU assembler
   aarch64-elf-as test.s -o test-gnu.o

   # Compare object files
   aarch64-elf-objdump -d test-ours.o > ours.txt
   aarch64-elf-objdump -d test-gnu.o > gnu.txt
   diff ours.txt gnu.txt
   ```

2. **Relocation Comparison**:
   ```bash
   aarch64-elf-readelf -r test-ours.o
   aarch64-elf-readelf -r test-gnu.o
   ```

3. **Link Testing**:
   ```bash
   # Link with GNU linker
   aarch64-elf-ld -T cmd/link.ld -o test.elf test-ours.o libc/libc.a

   # Run on Slopix
   # (add to initramfs and boot)
   ```

### Test Cases

1. **Minimal Program**:
   ```c
   int main() { return 42; }
   ```

2. **Arithmetic**:
   ```c
   int add(int a, int b) { return a + b; }
   ```

3. **Control Flow**:
   ```c
   int abs(int x) { return x < 0 ? -x : x; }
   ```

4. **Function Calls**:
   ```c
   int fib(int n) {
       if (n <= 1) return n;
       return fib(n-1) + fib(n-2);
   }
   ```

5. **Global Variables**:
   ```c
   int counter = 0;
   void increment() { counter++; }
   ```

6. **Floating Point**:
   ```c
   double square(double x) { return x * x; }
   ```

### Exit Criteria

1. All cmd/ programs assemble correctly
2. Assembled objects link with aarch64-elf-ld
3. Linked executables run correctly on Slopix
4. No differences from GNU assembler output (for supported features)

---

## Step 11: Error Handling and Diagnostics

**Goal**: Provide helpful error messages.

### Work

1. **Syntax Errors**:
   ```
   test.s:10: error: expected register, got 'xyz'
   test.s:15: error: invalid immediate value: 0x10000000
   test.s:20: error: unknown instruction: 'addd'
   ```

2. **Semantic Errors**:
   ```
   test.s:25: error: undefined symbol: 'missing_function'
   test.s:30: error: branch offset out of range: 0x8000000
   test.s:35: error: immediate not encodable: 0x1234
   ```

3. **Warnings**:
   ```
   test.s:40: warning: unaligned access to 8-byte value
   ```

4. **Source Location Tracking**:
   - Track filename and line number during parsing
   - Include in all error messages

### Exit Criteria

1. Clear error messages for common mistakes
2. Line numbers point to correct source location
3. Assembler exits with non-zero status on error

---

## Step 12: cmd/as Integration

**Goal**: Package assembler as Slopix cmd/ program.

### Work

1. **Command-Line Interface**:
   ```bash
   as [options] input.s -o output.o

   Options:
     -o FILE    Output file (default: a.out)
     -v         Verbose output
     --version  Print version
     --help     Print usage
   ```

2. **Build System Integration**:
   ```makefile
   # In cmd/Makefile
   AS = ../cmd/as/as

   %.o: %.s
       $(AS) $< -o $@
   ```

3. **Cross-Compilation**:
   - Initially build with host compiler for development
   - Later compile with chibicc to run on Slopix

### Directory Structure

```
cmd/as/
├── Makefile
├── as.h
├── main.c
├── lexer.c
├── parser.c
├── encode.c
├── encode_fp.c     # Floating-point encoding
├── elf.c
├── symtab.c
├── literal.c       # Literal pool management
└── error.c         # Error handling
```

### Exit Criteria

1. Assembler builds as cmd/ program
2. Can replace aarch64-elf-as in build system
3. Full toolchain (`chibicc` + `cmd/as` + `aarch64-elf-ld`) works

---

## Phase 3 Summary

### Step-by-Step Progression

| Step | Component | Test Method | Exit Criteria |
|------|-----------|-------------|---------------|
| 1 | Lexer | Manual token dump | Tokenizes chibicc output |
| 2 | Symbol Table | Symbol dump | Correct addresses |
| 3 | Encoding Framework | Manual encoding tests | Basic instructions encode |
| 4 | Core Instructions | Differential vs GNU as | All data proc/load/store |
| 5 | Float Instructions | Differential testing | Float ops encode |
| 6 | ELF Output | readelf validation | Valid object files |
| 7 | Relocations | readelf -r comparison | Correct relocation types |
| 8 | Literal Pools | Link and run | Large constants work |
| 9 | Directives | Full programs | All directives work |
| 10 | Integration | cmd/ programs | Full toolchain works |
| 11 | Error Handling | Invalid input | Clear messages |
| 12 | Packaging | Build system | Replaces GNU as |

### Files to Create

| File | Description |
|------|-------------|
| `cmd/as/Makefile` | Build configuration |
| `cmd/as/as.h` | Main header with types |
| `cmd/as/main.c` | Entry point, CLI |
| `cmd/as/lexer.c` | Tokenizer |
| `cmd/as/parser.c` | Two-pass parser |
| `cmd/as/encode.c` | Integer instruction encoding |
| `cmd/as/encode_fp.c` | Floating-point encoding |
| `cmd/as/elf.c` | ELF file generation |
| `cmd/as/symtab.c` | Symbol table |
| `cmd/as/literal.c` | Literal pool management |
| `cmd/as/error.c` | Error reporting |

### Exit Criteria (Phase 3 Complete)

1. **Instruction Support**: All ~45 instructions chibicc generates
2. **Directive Support**: All directives in chibicc output
3. **ELF Output**: Valid relocatable objects
4. **Relocations**: All 7 required relocation types
5. **Integration**: Can assemble all cmd/ programs
6. **Linking**: Objects link with aarch64-elf-ld
7. **Execution**: Linked programs run on Slopix

---

## References

- [ARM A64 Instruction Set](https://developer.arm.com/documentation/ddi0602/latest/) - Official instruction reference
- [ELF for AArch64](docs/aaelf64/aaelf64.md) - ELF format specification
- [AAPCS64](docs/aapcs64/aapcs64.md) - Calling convention
- [CAS-Atlantic/AArch64-Encoding](https://github.com/CAS-Atlantic/AArch64-Encoding) - Encoding tables
- [Keystone Engine](https://www.keystone-engine.org/) - Reference assembler implementation
- [Two-pass assembler algorithm](http://users.cis.fiu.edu/~downeyt/cop3402/two-pass.htm) - Algorithm tutorial
- [AArch64 immediate encoding](http://dinfuehr.com/blog/encoding-of-immediate-values-on-aarch64/) - Immediate value encoding
