# Linker Roadmap (Phase 4 of CC.md)

A detailed implementation plan for building an AArch64 static linker for Slopix.

## Overview

**Goal**: Build `cmd/ld` that links `.o` relocatable objects and `libc.a` into ELF executables.

**Scope**: This roadmap covers Phase 4 only - a static linker that:
- Parses ELF64 relocatable object files produced by `cmd/as`
- Resolves symbols across multiple object files and static archives
- Processes AArch64 relocations to bind symbols to addresses
- Emits ELF64 executable files ready to run on Slopix

**Key References**:
- [ELF for the Arm 64-bit Architecture](docs/aaelf64/aaelf64.md)
- [Linking ELF Files](https://1010labs.org/~ajaymt/linking-elf/) - Tutorial on ELF linking
- [Build Your Own Linker](https://github.com/andrewhalle/byo-linker) - Reference implementation
- [Oracle Linker Guide](https://docs.oracle.com/cd/E23824_01/html/819-0690/chapter3-29.html) - Relocation processing
- [Linkers and Loaders](https://www.iecc.com/linker/) by John Levine - Canonical reference

---

## Design Decision: No Linker Script

Slopix uses a **hardcoded memory layout** instead of linker scripts:

```c
#define TEXT_BASE   0x10000    // All programs load here
#define ENTRY_NAME  "_start"   // Entry point symbol
```

**Rationale**:
- All Slopix user programs have identical layout requirements
- No need for ROM/RAM splits or complex memory mapping
- Eliminates script parsing complexity
- The kernel uses a separate build with the host toolchain anyway

This matches the original `cmd/link.ld` which was trivial (just base address and section order).

---

## Prerequisites

Phase 4 depends on the following from earlier phases:

| Component | Status | Required For |
|-----------|--------|--------------|
| Assembler (`cmd/as`) | Complete | Produces `.o` files to link |
| ELF structures (`as.h`) | Complete | Reuse Elf64_* types |
| Relocation types | Complete | Same relocations assembler emits |

The assembler already defines all necessary ELF structures in `cmd/as/as.h`:
- `Elf64_Ehdr`, `Elf64_Shdr`, `Elf64_Sym`, `Elf64_Rela`
- Relocation constants: `R_AARCH64_*`
- Symbol binding/type constants: `STB_*`, `STT_*`

---

## Linker Algorithm Overview

### High-Level Flow

```
┌─────────────────┐
│  Input Files    │  .o files, .a archives
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Parse ELF      │  Read headers, sections, symbols, relocations
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Section        │  Merge .text, .data, .rodata, .bss from all inputs
│  Merging        │  Assign final addresses (hardcoded layout)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Symbol         │  Build global symbol table
│  Resolution     │  Resolve undefined references to definitions
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Relocation     │  Apply R_AARCH64_* relocations
│  Processing     │  Patch instructions and data with final addresses
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Output ELF     │  Write executable with program headers
└─────────────────┘
```

### Key Data Structures

```c
// Parsed object file
typedef struct {
    char *filename;
    uint8_t *data;              // mmap'd file content
    size_t size;
    Elf64_Ehdr *ehdr;
    Elf64_Shdr *shdrs;
    int shnum;
    char *shstrtab;             // Section name string table
    char *strtab;               // Symbol name string table
    Elf64_Sym *symtab;
    int symcount;
    int symtab_shndx;           // Section index of .symtab
} ObjectFile;

// Global symbol entry (after resolution)
typedef struct Symbol Symbol;
struct Symbol {
    char *name;
    uint64_t value;             // Final address
    int size;
    int type;                   // STT_FUNC, STT_OBJECT, etc.
    int binding;                // STB_LOCAL, STB_GLOBAL, STB_WEAK
    ObjectFile *file;           // Defining object file
    int shndx;                  // Original section index
    int output_shndx;           // Output section index
    Symbol *next;               // Hash chain
};

// Output section
typedef struct {
    char *name;
    uint64_t addr;              // Virtual address
    uint64_t offset;            // File offset
    uint64_t size;
    uint64_t alignment;
    uint32_t type;              // SHT_PROGBITS, SHT_NOBITS
    uint64_t flags;             // SHF_ALLOC, SHF_WRITE, SHF_EXECINSTR
    uint8_t *data;              // Merged section content
} OutputSection;

// Merged section piece (tracks origin of each chunk)
typedef struct {
    ObjectFile *file;
    int input_shndx;            // Original section index
    uint64_t input_offset;      // Offset in input section
    uint64_t output_offset;     // Offset in output section
    uint64_t size;
} SectionPiece;
```

---

## Relocations to Implement

The linker must process all relocations that `cmd/as` generates:

| Relocation | Code | Operation | Usage |
|------------|------|-----------|-------|
| `R_AARCH64_ABS64` | 257 | S + A | 64-bit data (`.xword symbol`) |
| `R_AARCH64_ADR_PREL_PG_HI21` | 275 | Page(S+A) - Page(P) | ADRP instruction |
| `R_AARCH64_ADD_ABS_LO12_NC` | 277 | S + A | ADD with `:lo12:` |
| `R_AARCH64_LDST8_ABS_LO12_NC` | 278 | S + A | LDRB/STRB with `:lo12:` |
| `R_AARCH64_JUMP26` | 282 | S + A - P | B instruction |
| `R_AARCH64_CALL26` | 283 | S + A - P | BL instruction |
| `R_AARCH64_LDST16_ABS_LO12_NC` | 284 | S + A | LDRH/STRH with `:lo12:` |
| `R_AARCH64_LDST32_ABS_LO12_NC` | 285 | S + A | LDR/STR 32-bit with `:lo12:` |
| `R_AARCH64_LDST64_ABS_LO12_NC` | 286 | S + A | LDR/STR 64-bit with `:lo12:` |

Where:
- **S** = Symbol value (final address)
- **A** = Addend from relocation entry
- **P** = Place being relocated (address in output)
- **Page(x)** = x & ~0xFFF (4KB page alignment)

---

## Step 1: Project Setup and ELF Parser

**Goal**: Create project structure and parse ELF relocatable objects.

### Work

1. Create `cmd/ld/` directory structure:
   ```
   cmd/ld/
   ├── Makefile
   ├── ld.h           # Main header, shared with as.h structures
   ├── main.c         # Entry point, CLI
   ├── elf_read.c     # ELF parsing
   ├── symbol.c       # Symbol table management
   ├── section.c      # Section merging
   ├── reloc.c        # Relocation processing
   ├── output.c       # ELF executable output
   └── archive.c      # .a static archive handling
   ```

2. Copy/share ELF structures from `cmd/as/as.h`:
   - Can either include directly or create shared `elf.h`
   - All `Elf64_*` types and constants already defined

3. Implement ELF parser (`elf_read.c`):
   ```c
   // Validate ELF header
   int elf_check_header(Elf64_Ehdr *ehdr) {
       if (memcmp(ehdr->e_ident, "\x7fELF", 4) != 0)
           return 0;  // Not ELF
       if (ehdr->e_ident[4] != ELFCLASS64)
           return 0;  // Not 64-bit
       if (ehdr->e_type != ET_REL)
           return 0;  // Not relocatable
       if (ehdr->e_machine != EM_AARCH64)
           return 0;  // Not AArch64
       return 1;
   }

   // Parse object file
   ObjectFile *elf_read(const char *path) {
       ObjectFile *obj = calloc(1, sizeof(ObjectFile));
       obj->filename = strdup(path);

       // Memory-map file
       int fd = open(path, O_RDONLY);
       struct stat st;
       fstat(fd, &st);
       obj->size = st.st_size;
       obj->data = mmap(NULL, obj->size, PROT_READ, MAP_PRIVATE, fd, 0);
       close(fd);

       // Parse header
       obj->ehdr = (Elf64_Ehdr *)obj->data;
       if (!elf_check_header(obj->ehdr))
           error("not a valid AArch64 relocatable: %s", path);

       // Parse section headers
       obj->shdrs = (Elf64_Shdr *)(obj->data + obj->ehdr->e_shoff);
       obj->shnum = obj->ehdr->e_shnum;

       // Find string tables
       obj->shstrtab = (char *)(obj->data +
           obj->shdrs[obj->ehdr->e_shstrndx].sh_offset);

       // Find symbol table
       for (int i = 0; i < obj->shnum; i++) {
           if (obj->shdrs[i].sh_type == SHT_SYMTAB) {
               obj->symtab_shndx = i;
               obj->symtab = (Elf64_Sym *)(obj->data + obj->shdrs[i].sh_offset);
               obj->symcount = obj->shdrs[i].sh_size / sizeof(Elf64_Sym);
               // String table is in sh_link
               obj->strtab = (char *)(obj->data +
                   obj->shdrs[obj->shdrs[i].sh_link].sh_offset);
               break;
           }
       }

       return obj;
   }
   ```

4. Implement section access helpers:
   ```c
   // Get section name
   const char *section_name(ObjectFile *obj, int idx) {
       return obj->shstrtab + obj->shdrs[idx].sh_name;
   }

   // Get section data
   uint8_t *section_data(ObjectFile *obj, int idx) {
       if (obj->shdrs[idx].sh_type == SHT_NOBITS)
           return NULL;  // .bss has no data
       return obj->data + obj->shdrs[idx].sh_offset;
   }

   // Get symbol name
   const char *symbol_name(ObjectFile *obj, int idx) {
       return obj->strtab + obj->symtab[idx].st_name;
   }
   ```

### Testing Strategy

```bash
# Test ELF parsing
echo 'int main() { return 0; }' > /tmp/test.c
tools/cc/chibicc -c /tmp/test.c -o /tmp/test.o
cmd/ld/ld --dump-sections /tmp/test.o
# Should list: .text, .rodata, .data, .bss, .symtab, etc.

cmd/ld/ld --dump-symbols /tmp/test.o
# Should list: main, _start, etc.
```

### Exit Criteria

1. Parser reads and validates ELF headers
2. Section headers correctly parsed
3. Symbol table accessible with names
4. Handles multiple input files

---

## Step 2: Symbol Table and Resolution

**Goal**: Build global symbol table and resolve references across objects.

### Symbol Resolution Algorithm

```
For each input object file:
    For each symbol in the object's symbol table:
        If symbol is LOCAL (STB_LOCAL):
            Skip (local symbols don't participate in linking)
        If symbol is UNDEFINED (st_shndx == SHN_UNDEF):
            Add to unresolved set
        If symbol is DEFINED (st_shndx != SHN_UNDEF):
            If symbol already exists in global table:
                If existing is WEAK and new is GLOBAL:
                    Replace with new definition
                If both are GLOBAL:
                    Error: multiple definition
            Else:
                Add to global symbol table

After all files processed:
    For each unresolved symbol:
        Look up in global table
        If not found and not WEAK:
            Error: undefined reference
        If WEAK and not found:
            Value = 0 (or handle per-relocation)
```

### Work

1. Implement symbol hash table (`symbol.c`):
   ```c
   #define SYMTAB_SIZE 4096

   typedef struct {
       Symbol *buckets[SYMTAB_SIZE];
       int count;
   } SymbolTable;

   static uint32_t hash_string(const char *s) {
       uint32_t h = 0;
       for (; *s; s++)
           h = h * 33 + *s;
       return h;
   }

   Symbol *symbol_lookup(SymbolTable *tab, const char *name) {
       uint32_t h = hash_string(name) % SYMTAB_SIZE;
       for (Symbol *s = tab->buckets[h]; s; s = s->next)
           if (strcmp(s->name, name) == 0)
               return s;
       return NULL;
   }

   void symbol_add(SymbolTable *tab, Symbol *sym) {
       uint32_t h = hash_string(sym->name) % SYMTAB_SIZE;
       sym->next = tab->buckets[h];
       tab->buckets[h] = sym;
       tab->count++;
   }
   ```

2. Implement symbol resolution:
   ```c
   void resolve_symbols(ObjectFile **files, int nfiles, SymbolTable *global) {
       // First pass: collect all definitions
       for (int f = 0; f < nfiles; f++) {
           ObjectFile *obj = files[f];
           for (int i = 1; i < obj->symcount; i++) {  // Skip index 0 (null)
               Elf64_Sym *sym = &obj->symtab[i];
               int bind = ELF64_ST_BIND(sym->st_info);
               int type = ELF64_ST_TYPE(sym->st_info);

               if (bind == STB_LOCAL)
                   continue;
               if (sym->st_shndx == SHN_UNDEF)
                   continue;

               const char *name = symbol_name(obj, i);
               Symbol *existing = symbol_lookup(global, name);

               if (existing) {
                   if (existing->binding == STB_WEAK && bind == STB_GLOBAL) {
                       // Replace weak with strong
                       existing->value = sym->st_value;
                       existing->file = obj;
                       existing->binding = bind;
                   } else if (existing->binding == STB_GLOBAL && bind == STB_GLOBAL) {
                       error("multiple definition of '%s'", name);
                   }
               } else {
                   Symbol *s = calloc(1, sizeof(Symbol));
                   s->name = strdup(name);
                   s->value = sym->st_value;
                   s->size = sym->st_size;
                   s->type = type;
                   s->binding = bind;
                   s->file = obj;
                   s->shndx = sym->st_shndx;
                   symbol_add(global, s);
               }
           }
       }

       // Second pass: check undefined references
       for (int f = 0; f < nfiles; f++) {
           ObjectFile *obj = files[f];
           for (int i = 1; i < obj->symcount; i++) {
               Elf64_Sym *sym = &obj->symtab[i];
               int bind = ELF64_ST_BIND(sym->st_info);

               if (bind == STB_LOCAL)
                   continue;
               if (sym->st_shndx != SHN_UNDEF)
                   continue;

               const char *name = symbol_name(obj, i);
               Symbol *def = symbol_lookup(global, name);

               if (!def && bind != STB_WEAK)
                   error("undefined reference to '%s' in %s",
                         name, obj->filename);
           }
       }
   }
   ```

3. Handle special symbols:
   ```c
   // Add linker-defined symbols
   void add_linker_symbols(SymbolTable *global, OutputSection *sections, int nsections) {
       // _start - entry point (must exist)
       Symbol *start = symbol_lookup(global, "_start");
       if (!start)
           error("undefined reference to '_start' (no entry point)");

       // Optional: __bss_start, __bss_end, _end
       // These can be defined by the linker if needed
   }
   ```

### Testing Strategy

```bash
# Test symbol resolution
cat > /tmp/a.c << 'EOF'
extern int foo(void);
int main() { return foo(); }
EOF
cat > /tmp/b.c << 'EOF'
int foo(void) { return 42; }
EOF

tools/cc/chibicc -c /tmp/a.c -o /tmp/a.o
tools/cc/chibicc -c /tmp/b.c -o /tmp/b.o
cmd/ld/ld --dump-globals /tmp/a.o /tmp/b.o
# Should show: main (a.o), foo (b.o)

# Test undefined error
cmd/ld/ld /tmp/a.o  # Should error: undefined reference to 'foo'
```

### Exit Criteria

1. Global symbol table collects all definitions
2. Weak symbols replaced by strong definitions
3. Multiple definition errors detected
4. Undefined reference errors detected

---

## Step 3: Section Merging

**Goal**: Merge input sections into output sections with final addresses.

### Section Merging Algorithm

```
For each input object:
    For each section (.text, .rodata, .data, .bss):
        Align output section to input section alignment
        Append input section data to output section
        Record mapping: (input file, input section, input offset) -> output offset

Update symbol values:
    For each global symbol:
        new_value = output_section_base + (original_value + section_piece_offset)
```

### Work

1. Implement section merging (`section.c`):
   ```c
   // Collect and merge sections
   void merge_sections(ObjectFile **files, int nfiles,
                       OutputSection *out_text,
                       OutputSection *out_rodata,
                       OutputSection *out_data,
                       OutputSection *out_bss) {

       // Initialize output sections
       out_text->name = ".text";
       out_text->flags = SHF_ALLOC | SHF_EXECINSTR;
       out_text->type = SHT_PROGBITS;
       out_text->alignment = 4;

       out_rodata->name = ".rodata";
       out_rodata->flags = SHF_ALLOC;
       out_rodata->type = SHT_PROGBITS;
       out_rodata->alignment = 8;

       out_data->name = ".data";
       out_data->flags = SHF_ALLOC | SHF_WRITE;
       out_data->type = SHT_PROGBITS;
       out_data->alignment = 8;

       out_bss->name = ".bss";
       out_bss->flags = SHF_ALLOC | SHF_WRITE;
       out_bss->type = SHT_NOBITS;
       out_bss->alignment = 8;

       // Merge each input file's sections
       for (int f = 0; f < nfiles; f++) {
           ObjectFile *obj = files[f];
           for (int i = 0; i < obj->shnum; i++) {
               Elf64_Shdr *shdr = &obj->shdrs[i];
               const char *name = section_name(obj, i);

               OutputSection *out = NULL;
               if (strncmp(name, ".text", 5) == 0)
                   out = out_text;
               else if (strncmp(name, ".rodata", 7) == 0)
                   out = out_rodata;
               else if (strncmp(name, ".data", 5) == 0)
                   out = out_data;
               else if (strncmp(name, ".bss", 4) == 0)
                   out = out_bss;
               else
                   continue;  // Skip other sections

               // Align and append
               uint64_t align = shdr->sh_addralign;
               if (align < 1) align = 1;
               out->size = (out->size + align - 1) & ~(align - 1);

               // Record piece for relocation adjustment
               SectionPiece piece = {
                   .file = obj,
                   .input_shndx = i,
                   .input_offset = 0,
                   .output_offset = out->size,
                   .size = shdr->sh_size,
               };
               // Store piece mapping (implementation detail)

               // Copy data (or just track size for .bss)
               if (shdr->sh_type != SHT_NOBITS) {
                   out->data = realloc(out->data, out->size + shdr->sh_size);
                   memcpy(out->data + out->size,
                          obj->data + shdr->sh_offset,
                          shdr->sh_size);
               }
               out->size += shdr->sh_size;
           }
       }
   }
   ```

3. Assign final addresses (hardcoded layout):
   ```c
   #define TEXT_BASE 0x10000

   void assign_addresses(OutputSection *text,
                         OutputSection *rodata,
                         OutputSection *data,
                         OutputSection *bss) {
       uint64_t addr = TEXT_BASE;

       // .text
       text->addr = addr;
       addr += text->size;

       // .rodata (page-aligned typically, but can be adjacent)
       addr = (addr + 7) & ~7;  // 8-byte align
       rodata->addr = addr;
       addr += rodata->size;

       // .data
       addr = (addr + 7) & ~7;
       data->addr = addr;
       addr += data->size;

       // .bss
       addr = (addr + 7) & ~7;
       bss->addr = addr;
       // bss->size is just tracked, no data
   }
   ```

4. Update symbol values:
   ```c
   void update_symbol_values(SymbolTable *global,
                             OutputSection *text,
                             OutputSection *rodata,
                             OutputSection *data,
                             OutputSection *bss) {
       for (int i = 0; i < SYMTAB_SIZE; i++) {
           for (Symbol *sym = global->buckets[i]; sym; sym = sym->next) {
               // Find which output section this symbol belongs to
               const char *secname = section_name(sym->file, sym->shndx);
               OutputSection *out = NULL;
               uint64_t piece_offset = 0;

               // Look up the section piece for this symbol's section
               // (would need to track piece mappings from merge step)

               if (strncmp(secname, ".text", 5) == 0)
                   out = text;
               else if (strncmp(secname, ".rodata", 7) == 0)
                   out = rodata;
               else if (strncmp(secname, ".data", 5) == 0)
                   out = data;
               else if (strncmp(secname, ".bss", 4) == 0)
                   out = bss;

               if (out) {
                   sym->value = out->addr + piece_offset + sym->value;
                   sym->output_shndx = /* output section index */;
               }
           }
       }
   }
   ```

### Exit Criteria

1. Sections merged in correct order: .text, .rodata, .data, .bss
2. Alignment requirements preserved
3. Symbol values updated to final addresses
4. Section addresses follow hardcoded layout

---

## Step 4: Relocation Processing

**Goal**: Apply relocations to patch instructions and data with final addresses.

### Relocation Processing Algorithm

```
For each input object file:
    For each .rela.text section:
        For each relocation entry:
            P = output address of place being relocated
            S = resolve symbol to final address
            A = addend from relocation entry
            Calculate X based on relocation type
            Apply X to the instruction/data at P
```

### Work

1. Implement relocation application (`reloc.c`):
   ```c
   // Get relocations for a section
   Elf64_Rela *get_relas(ObjectFile *obj, int sec_idx, int *count) {
       for (int i = 0; i < obj->shnum; i++) {
           Elf64_Shdr *shdr = &obj->shdrs[i];
           if (shdr->sh_type == SHT_RELA && shdr->sh_info == sec_idx) {
               *count = shdr->sh_size / sizeof(Elf64_Rela);
               return (Elf64_Rela *)(obj->data + shdr->sh_offset);
           }
       }
       *count = 0;
       return NULL;
   }

   // Apply a single relocation
   void apply_reloc(uint8_t *output, uint64_t place_addr,
                    Elf64_Rela *rela, Symbol *sym) {
       uint64_t S = sym ? sym->value : 0;
       int64_t A = rela->r_addend;
       uint64_t P = place_addr;
       int type = ELF64_R_TYPE(rela->r_info);

       switch (type) {
       case R_AARCH64_ABS64:
           // S + A, write 64 bits
           *(uint64_t *)output = S + A;
           break;

       case R_AARCH64_ADR_PREL_PG_HI21: {
           // Page(S+A) - Page(P)
           int64_t X = ((S + A) & ~0xFFF) - (P & ~0xFFF);
           // Check range: -2^32 <= X < 2^32
           if (X < -(1LL << 32) || X >= (1LL << 32))
               error("ADRP offset out of range");
           // Encode into ADRP instruction
           uint32_t insn = *(uint32_t *)output;
           int64_t imm = X >> 12;
           insn &= 0x9F00001F;  // Clear immlo and immhi
           insn |= (imm & 3) << 29;        // immlo
           insn |= ((imm >> 2) & 0x7FFFF) << 5;  // immhi
           *(uint32_t *)output = insn;
           break;
       }

       case R_AARCH64_ADD_ABS_LO12_NC: {
           // S + A, bits [11:0]
           uint64_t X = (S + A) & 0xFFF;
           uint32_t insn = *(uint32_t *)output;
           insn &= ~(0xFFF << 10);  // Clear imm12
           insn |= X << 10;
           *(uint32_t *)output = insn;
           break;
       }

       case R_AARCH64_CALL26:
       case R_AARCH64_JUMP26: {
           // S + A - P
           int64_t X = S + A - P;
           // Check range: -2^27 <= X < 2^27
           if (X < -(1 << 27) || X >= (1 << 27))
               error("branch offset out of range");
           // Check alignment
           if (X & 3)
               error("branch target not aligned");
           // Encode imm26
           uint32_t insn = *(uint32_t *)output;
           insn &= 0xFC000000;  // Keep opcode
           insn |= (X >> 2) & 0x3FFFFFF;
           *(uint32_t *)output = insn;
           break;
       }

       case R_AARCH64_LDST8_ABS_LO12_NC: {
           // S + A, bits [11:0]
           uint64_t X = (S + A) & 0xFFF;
           uint32_t insn = *(uint32_t *)output;
           insn &= ~(0xFFF << 10);
           insn |= X << 10;
           *(uint32_t *)output = insn;
           break;
       }

       case R_AARCH64_LDST16_ABS_LO12_NC: {
           // S + A, bits [11:1]
           uint64_t X = (S + A) & 0xFFF;
           if (X & 1) error("unaligned LDST16");
           uint32_t insn = *(uint32_t *)output;
           insn &= ~(0xFFF << 10);
           insn |= (X >> 1) << 10;
           *(uint32_t *)output = insn;
           break;
       }

       case R_AARCH64_LDST32_ABS_LO12_NC: {
           // S + A, bits [11:2]
           uint64_t X = (S + A) & 0xFFF;
           if (X & 3) error("unaligned LDST32");
           uint32_t insn = *(uint32_t *)output;
           insn &= ~(0xFFF << 10);
           insn |= (X >> 2) << 10;
           *(uint32_t *)output = insn;
           break;
       }

       case R_AARCH64_LDST64_ABS_LO12_NC: {
           // S + A, bits [11:3]
           uint64_t X = (S + A) & 0xFFF;
           if (X & 7) error("unaligned LDST64");
           uint32_t insn = *(uint32_t *)output;
           insn &= ~(0xFFF << 10);
           insn |= (X >> 3) << 10;
           *(uint32_t *)output = insn;
           break;
       }

       default:
           error("unsupported relocation type: %d", type);
       }
   }
   ```

2. Process all relocations:
   ```c
   void process_relocations(ObjectFile **files, int nfiles,
                            SymbolTable *global,
                            OutputSection *out_text) {
       for (int f = 0; f < nfiles; f++) {
           ObjectFile *obj = files[f];
           for (int i = 0; i < obj->shnum; i++) {
               if (strncmp(section_name(obj, i), ".text", 5) != 0)
                   continue;

               int relcount;
               Elf64_Rela *relas = get_relas(obj, i, &relcount);

               for (int r = 0; r < relcount; r++) {
                   Elf64_Rela *rela = &relas[r];
                   int sym_idx = ELF64_R_SYM(rela->r_info);

                   // Resolve symbol
                   Symbol *sym = NULL;
                   if (sym_idx != 0) {
                       const char *name = symbol_name(obj, sym_idx);
                       int bind = ELF64_ST_BIND(obj->symtab[sym_idx].st_info);
                       if (bind == STB_LOCAL) {
                           // Local symbol - use value from this object
                           // (needs section offset adjustment)
                       } else {
                           sym = symbol_lookup(global, name);
                       }
                   }

                   // Calculate output position
                   uint64_t piece_offset = /* from section piece mapping */;
                   uint64_t output_offset = piece_offset + rela->r_offset;
                   uint64_t place_addr = out_text->addr + output_offset;

                   apply_reloc(out_text->data + output_offset,
                               place_addr, rela, sym);
               }
           }
       }
   }
   ```

### Exit Criteria

1. All relocation types correctly implemented
2. Range checking for branch offsets
3. Alignment checking for load/store
4. Local and global symbols resolved correctly

---

## Step 5: Static Archive Handling

**Goal**: Process `.a` static libraries to resolve undefined symbols.

### Archive Format

```
!<arch>\n                    Magic (8 bytes)
<member header>              Each member is 60-byte header + file content
<member content>
...
```

Member header format:
```
Name[16] Date[12] UID[6] GID[6] Mode[8] Size[10] End[2]
```

### Work

1. Implement archive parser (`archive.c`):
   ```c
   typedef struct {
       char *name;
       uint8_t *data;
       size_t size;
   } ArchiveMember;

   typedef struct {
       char *path;
       ArchiveMember *members;
       int nmembers;
       char **symbols;         // Symbol index (from __.SYMDEF or /)
       int *symbol_members;    // Which member defines each symbol
       int nsymbols;
   } Archive;

   Archive *archive_open(const char *path) {
       Archive *ar = calloc(1, sizeof(Archive));
       ar->path = strdup(path);

       int fd = open(path, O_RDONLY);
       struct stat st;
       fstat(fd, &st);
       uint8_t *data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
       close(fd);

       // Check magic
       if (memcmp(data, "!<arch>\n", 8) != 0)
           error("not a valid archive: %s", path);

       // Parse members
       size_t pos = 8;
       while (pos < st.st_size) {
           // Parse 60-byte header
           char name[17] = {0};
           memcpy(name, data + pos, 16);
           // Trim trailing spaces
           for (int i = 15; i >= 0 && name[i] == ' '; i--)
               name[i] = 0;

           char size_str[11] = {0};
           memcpy(size_str, data + pos + 48, 10);
           size_t size = atoi(size_str);

           pos += 60;  // Header size

           // Handle special members
           if (strcmp(name, "/") == 0 || strcmp(name, "__.SYMDEF") == 0) {
               // Symbol table - parse for fast lookup
               parse_symbol_table(ar, data + pos, size);
           } else if (name[0] != '/') {
               // Regular object file member
               ar->members = realloc(ar->members,
                   (ar->nmembers + 1) * sizeof(ArchiveMember));
               ar->members[ar->nmembers].name = strdup(name);
               ar->members[ar->nmembers].data = data + pos;
               ar->members[ar->nmembers].size = size;
               ar->nmembers++;
           }

           pos += size;
           if (pos & 1) pos++;  // Align to even
       }

       return ar;
   }
   ```

2. Implement archive symbol resolution:
   ```c
   // Find archive members that resolve undefined symbols
   void resolve_from_archive(Archive *ar, SymbolTable *undefined,
                             ObjectFile ***out_files, int *nfiles) {
       bool changed = true;
       while (changed) {
           changed = false;
           for (int i = 0; i < ar->nsymbols; i++) {
               Symbol *undef = symbol_lookup(undefined, ar->symbols[i]);
               if (undef && !undef->file) {
                   // This archive member defines an undefined symbol
                   int member_idx = ar->symbol_members[i];
                   ArchiveMember *m = &ar->members[member_idx];

                   // Parse the member as an object file
                   ObjectFile *obj = elf_read_memory(m->data, m->size, m->name);

                   // Add to output files
                   *out_files = realloc(*out_files,
                       (*nfiles + 1) * sizeof(ObjectFile *));
                   (*out_files)[*nfiles] = obj;
                   (*nfiles)++;

                   // Re-run symbol resolution
                   // (This object may have new undefined symbols)
                   changed = true;
               }
           }
       }
   }
   ```

### Testing Strategy

```bash
# Test archive handling
cmd/ld/ld main.o libc/libc.a -o test.elf
# Should pull in only needed members from libc.a
```

### Exit Criteria

1. Archive magic validated
2. Member headers parsed correctly
3. Symbol table used for fast lookup
4. Only needed members extracted
5. Iterative resolution until fixed point

---

## Step 6: ELF Executable Output

**Goal**: Generate valid ELF64 executable files.

### Executable vs Relocatable

| Aspect | Relocatable (ET_REL) | Executable (ET_EXEC) |
|--------|---------------------|---------------------|
| e_type | 1 | 2 |
| Program headers | None | Required |
| e_entry | 0 | Entry point address |
| Section addresses | 0 | Virtual addresses |
| Relocations | Present | Resolved (none) |

### Work

1. Define program header structure:
   ```c
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

   #define PT_NULL    0
   #define PT_LOAD    1
   #define PF_X       1  // Execute
   #define PF_W       2  // Write
   #define PF_R       4  // Read
   ```

2. Implement executable output (`output.c`):
   ```c
   void write_executable(const char *path,
                         OutputSection *text,
                         OutputSection *rodata,
                         OutputSection *data,
                         OutputSection *bss,
                         uint64_t entry) {
       FILE *out = fopen(path, "wb");

       // Calculate layout
       uint64_t ehdr_size = sizeof(Elf64_Ehdr);
       uint64_t phdr_size = sizeof(Elf64_Phdr);
       int phnum = 2;  // One for code (text+rodata), one for data

       uint64_t headers_size = ehdr_size + phnum * phdr_size;
       uint64_t first_section_offset = (headers_size + 0xFFF) & ~0xFFF;  // Page align

       // Adjust section offsets
       text->offset = first_section_offset;
       rodata->offset = text->offset + text->size;
       data->offset = rodata->offset + rodata->size;
       // bss has no file content

       // Write ELF header
       Elf64_Ehdr ehdr = {0};
       memcpy(ehdr.e_ident, "\x7f""ELF", 4);
       ehdr.e_ident[4] = ELFCLASS64;
       ehdr.e_ident[5] = ELFDATA2LSB;
       ehdr.e_ident[6] = EV_CURRENT;
       ehdr.e_type = 2;  // ET_EXEC
       ehdr.e_machine = EM_AARCH64;
       ehdr.e_version = 1;
       ehdr.e_entry = entry;
       ehdr.e_phoff = ehdr_size;
       ehdr.e_shoff = 0;  // No section headers needed for execution
       ehdr.e_ehsize = ehdr_size;
       ehdr.e_phentsize = phdr_size;
       ehdr.e_phnum = phnum;
       fwrite(&ehdr, ehdr_size, 1, out);

       // Write program headers
       // Segment 1: text + rodata (RX)
       Elf64_Phdr phdr1 = {
           .p_type = PT_LOAD,
           .p_flags = PF_R | PF_X,
           .p_offset = text->offset,
           .p_vaddr = text->addr,
           .p_paddr = text->addr,
           .p_filesz = text->size + rodata->size,
           .p_memsz = text->size + rodata->size,
           .p_align = 0x1000,
       };
       fwrite(&phdr1, phdr_size, 1, out);

       // Segment 2: data + bss (RW)
       Elf64_Phdr phdr2 = {
           .p_type = PT_LOAD,
           .p_flags = PF_R | PF_W,
           .p_offset = data->offset,
           .p_vaddr = data->addr,
           .p_paddr = data->addr,
           .p_filesz = data->size,
           .p_memsz = data->size + bss->size,  // bss extends memsz
           .p_align = 0x1000,
       };
       fwrite(&phdr2, phdr_size, 1, out);

       // Pad to first section
       while (ftell(out) < first_section_offset)
           fputc(0, out);

       // Write section data
       fwrite(text->data, text->size, 1, out);
       fwrite(rodata->data, rodata->size, 1, out);
       fwrite(data->data, data->size, 1, out);
       // bss is not written (only in memory)

       fclose(out);
       chmod(path, 0755);  // Make executable
   }
   ```

### Testing Strategy

```bash
# Link and run on Slopix
cmd/ld/ld -o tests.elf tests.o libc/libc.a
aarch64-elf-readelf -l tests.elf  # Check program headers
# Add to initramfs, boot, verify execution
```

### Exit Criteria

1. Valid ELF header with e_type = ET_EXEC
2. Program headers describe loadable segments
3. Entry point set correctly
4. Executable runs on Slopix

---

## Step 7: CLI and Integration

**Goal**: Command-line interface and full toolchain validation.

### Command-Line Interface

### Work

```c
// main.c
void print_usage(void) {
    fprintf(stderr,
        "Usage: ld [options] files...\n"
        "\n"
        "Options:\n"
        "  -o FILE        Output file (default: a.out)\n"
        "  -L DIR         Add library search path\n"
        "  -l NAME        Link with libNAME.a\n"
        "  -e SYMBOL      Set entry point (default: _start)\n"
        "  --verbose      Verbose output\n"
        "  --help         Show this help\n"
    );
}

int main(int argc, char **argv) {
    char *output = "a.out";
    char *entry = "_start";
    StringArray lib_paths = {0};
    StringArray input_files = {0};
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(argv[i], "-L") == 0 && i + 1 < argc) {
            strarray_push(&lib_paths, argv[++i]);
        } else if (strncmp(argv[i], "-l", 2) == 0) {
            // Convert -lfoo to libfoo.a and search lib paths
            char *libname = format("lib%s.a", argv[i] + 2);
            strarray_push(&input_files, find_library(libname, &lib_paths));
        } else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            entry = argv[++i];
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else if (argv[i][0] != '-') {
            strarray_push(&input_files, argv[i]);
        } else {
            error("unknown option: %s", argv[i]);
        }
    }

    if (input_files.len == 0)
        error("no input files");

    link(output, entry, &input_files, verbose);
    return 0;
}
```

### Exit Criteria

1. Basic options work (`-o`, `-L`, `-l`, `-e`)
2. Library search paths functional
3. Handles mixed .o and .a files

---

## Phase 4 Summary

### Step-by-Step Progression

| Step | Component | Test Method | Exit Criteria |
|------|-----------|-------------|---------------|
| 1 | ELF Parser | Dump sections/symbols | Parses assembler output |
| 2 | Symbol Resolution | Check undefined errors | Global table correct |
| 3 | Section Merging | Check addresses | Sections at correct locations |
| 4 | Relocations | Differential vs GNU ld | All types work |
| 5 | Archives | Link with libc.a | Pulls needed members |
| 6 | Executable Output | readelf validation | Valid ELF executable |
| 7 | CLI & Integration | Full pipeline | cmd/ programs work |

### Files to Create

| File | Description |
|------|-------------|
| `cmd/ld/Makefile` | Build configuration |
| `cmd/ld/ld.h` | Main header (shares ELF types with as.h) |
| `cmd/ld/main.c` | Entry point, CLI |
| `cmd/ld/elf_read.c` | ELF relocatable parser |
| `cmd/ld/symbol.c` | Symbol table and resolution |
| `cmd/ld/section.c` | Section merging |
| `cmd/ld/reloc.c` | Relocation processing |
| `cmd/ld/archive.c` | Static archive handling |
| `cmd/ld/output.c` | Executable output |

### Exit Criteria (Phase 4 Complete)

1. **ELF Parsing**: Reads relocatable objects from assembler
2. **Symbol Resolution**: Resolves all symbols across multiple files
3. **Relocations**: Processes all 9 AArch64 relocation types
4. **Archives**: Extracts needed members from `libc.a`
5. **Output**: Generates valid ELF64 executables
6. **Integration**: Full toolchain (`chibicc` + `cmd/as` + `cmd/ld`) works
7. **Testing**: All cmd/ programs link and run correctly on Slopix

---

## References

- [ELF for AArch64](docs/aaelf64/aaelf64.md) - Official specification
- [Linking ELF Files](https://1010labs.org/~ajaymt/linking-elf/) - Step-by-step tutorial
- [Build Your Own Linker](https://github.com/andrewhalle/byo-linker) - Reference implementation
- [Oracle Linker Guide](https://docs.oracle.com/cd/E23824_01/html/819-0690/chapter3-29.html) - Relocation processing
- [Symbol Resolution](https://docs.oracle.com/cd/E19120-01/open.solaris/819-0690/chapter2-93321/index.html) - Algorithm details
- [ELF Binaries and Relocations](https://stffrdhrn.github.io/hardware/embedded/openrisc/2019/11/29/relocs.html) - Practical examples
- [Linkers and Loaders](https://www.iecc.com/linker/) - John Levine's book
