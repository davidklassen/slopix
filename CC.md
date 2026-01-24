# C Compiler for Slopix

A guide to porting chibicc to Slopix for self-hosting capability.

## Overview

[chibicc](https://github.com/rui314/chibicc) is a small, self-hosting C compiler by Rui Ueyama. It implements most of C11 in ~8,500 lines of C and can compile real-world programs like Git, SQLite, and itself.

Key characteristics:
- **Self-hosting**: compiles its own source code
- **Multi-pass design**: lexer → parser → codegen (not one-pass like tcc)
- **No optimizer**: generates correct but unoptimized code
- **x86-64 target**: emits AT&T syntax assembly text
- **Never calls free()**: relies on process exit for cleanup

Existing AArch64 ports for reference:
- [xhackerustc/chibicc-aarch64](https://github.com/xhackerustc/chibicc-aarch64)
- [derbuihan/chibicc_arm64](https://github.com/derbuihan/chibicc_arm64)
- [ncihnegn/chibicc-arm64](https://github.com/ncihnegn/chibicc-arm64)

---

## chibicc Source Structure

| File | Lines | Description | Arch-specific |
|------|-------|-------------|---------------|
| parse.c | 3,368 | Recursive descent parser, AST construction | No |
| codegen.c | 1,595 | x86-64 assembly generation | **Yes** |
| preprocess.c | 1,208 | C preprocessor (#include, #define, etc.) | No |
| tokenize.c | 805 | Lexer/tokenizer | No |
| main.c | 791 | Driver, command-line parsing, subprocess control | Paths only |
| type.c | 307 | Type system | No |
| unicode.c | 189 | UTF-8 encoding/decoding | No |
| hashmap.c | 165 | Hash table implementation | No |
| strings.c | 31 | String array utilities | No |
| **Total** | **~8,500** | | |

Only `codegen.c` requires a full rewrite. `main.c` needs minor path/linker updates.

---

## Implementation Phases

### Phase 1: Cross-Compiler (`tools/cc`)

**Goal**: Replace `aarch64-elf-gcc` for compiling cmd/ programs.

**Work**:
- Copy chibicc source to `tools/cc/`
- Rewrite `codegen.c` for AArch64
- Update `main.c` default include paths (hardcoded in `add_default_include_paths()`)
  - Change from `/usr/include/x86_64-linux-gnu` etc. to Slopix's `libc/include/`
- Uses host libc, host `aarch64-elf-as`, host `aarch64-elf-ld`

**Testing**:
- Adapt chibicc's test suite to run on Slopix:
  - chibicc tests are simple: `test/*.c` files using `ASSERT(expected, actual)` macro
  - `test/common` provides `assert()` function - create Slopix-compatible version
  - Build test runner that executes all tests sequentially
  - Package tests into initramfs, run with `psci_system_off()` at end
  - Reuses `cmd/tests/` infrastructure pattern
- Compile cmd/ programs, run on Slopix via `make run`

**Test structure**:
```
tools/cc/test/
├── common.c      # Slopix-compatible assert() using our libc
├── test.h        # ASSERT macro (same as original)
├── arith.c       # Arithmetic tests (from chibicc)
├── control.c     # Control flow tests
├── ...           # Other chibicc tests
├── runner.c      # Runs all tests, calls poweroff()
└── Makefile      # Builds tests, creates initramfs
```

**Deliverables**:
- `tools/cc/` with AArch64 codegen
- Slopix-compatible test suite in `tools/cc/test/`
- Makefile integration (`make cmd CC=tools/cc/chibicc`)

**Exit Criteria**:
1. chibicc's test suite passes on Slopix (via initramfs + QEMU)
2. All cmd/ programs build with `tools/cc`
3. Built cmd/ programs run correctly on Slopix

---

### Phase 2: Libc Extensions

**Goal**: Slopix libc sufficient to run chibicc natively.

**Work**:

| Category | Functions |
|----------|-----------|
| Memory | `malloc`, `calloc`, `realloc`, `free` (on sbrk) |
| FILE* | `FILE` struct, `stdin`, `stdout`, `stderr` |
| File I/O | `fopen`, `fclose`, `fread`, `fwrite`, `fflush`, `fputc`, `fgetc` |
| Formatted I/O | `fprintf`, `vfprintf`, `sprintf`, `snprintf` |
| Streams | `open_memstream` (dynamic buffer FILE*) |
| Strings | `strdup`, `strndup`, `strrchr`, `strtok`, `strncasecmp`, `memcmp` |
| Numeric | `strtoul`, `strtol` |
| ctype | `isalnum`, `ispunct`, `isxdigit`, `isupper`, `islower` |
| Error | `errno`, `strerror`, `assert` macro |
| Process | `_exit` |
| Filesystem | `mkstemp`, `dirname`, `basename` |
| Time | `time`, `localtime` (can stub initially) |

**open_memstream details**: Creates a `FILE*` that writes to a dynamically growing buffer. On `fflush`/`fclose`, updates caller's pointer and size. chibicc uses this ~20 times for building strings. Requires full `FILE*` abstraction + `realloc`.

**Testing**:
- Extend `cmd/tests/` with new test files:
  - `test_malloc.c` - malloc/calloc/realloc/free
  - `test_stdio.c` - FILE*, fopen, fread, fwrite, fprintf, open_memstream
  - `test_string2.c` - strdup, strtoul, memcmp, etc.

**Deliverables**:
- Extended `libc/` with all required functions
- New test suites in `cmd/tests/`

**Exit Criteria**:
1. All new test suites pass on Slopix
2. Can compile and run a test program on Slopix that exercises: calloc, fopen, fprintf, open_memstream, strdup, strtoul

---

### Phase 3: Assembler (`cmd/as`)

**Goal**: Assemble AArch64 `.s` files into ELF `.o` relocatable objects.

**Work**:
- Parse AArch64 assembly syntax (subset that chibicc emits)
- Encode instructions into binary
- Emit ELF relocatable object files
- Handle labels, relocations (`R_AARCH64_CALL26`, `R_AARCH64_ADR_PREL_PG_HI21`, etc.)

**Testing**:
- Use host `aarch64-elf-objdump` to inspect output
- Differential testing: compare `cmd/as` output with `aarch64-elf-as` output
  ```bash
  tools/cc -S test.c -o test.s
  aarch64-elf-as test.s -o expected.o
  cmd/as test.s -o actual.o
  diff <(aarch64-elf-objdump -d expected.o) <(aarch64-elf-objdump -d actual.o)
  ```
- Integration: assemble chibicc output, link with host `ld`, run result

**Deliverables**:
- `cmd/as` that handles all instructions/directives chibicc emits
- Outputs valid ELF relocatable objects

**Exit Criteria**:
1. Can assemble all `.s` files produced by `tools/cc` for cmd/ programs
2. Resulting `.o` files link successfully with host `aarch64-elf-ld`
3. Linked executables run correctly on Slopix

---

### Phase 4: Linker (`cmd/ld`)

**Goal**: Link `.o` files + `libc.a` into ELF executables.

**Work**:
- Parse ELF relocatable objects
- Symbol resolution across multiple `.o` files and archives
- Process relocations
- Emit ELF executable with proper segments

**Testing**:
- Use host `aarch64-elf-objdump`/`readelf` to inspect output
- Differential testing: compare with `aarch64-elf-ld` output
- Full pipeline: `tools/cc -c` → `cmd/as` → `cmd/ld` → run on Slopix

**Deliverables**:
- `cmd/ld` that links `.o` + `libc.a` → ELF executable
- Handles all relocations chibicc/as generate

**Exit Criteria**:
1. Full toolchain (`tools/cc` + `cmd/as` + `cmd/ld`) builds all cmd/ programs
2. Built programs run correctly on Slopix
3. No dependency on host assembler/linker for producing Slopix binaries

---

### Phase 5: Native Compiler (`cmd/cc`)

**Goal**: Self-hosted C compiler running on Slopix.

**Work**:
- Build chibicc for Slopix using Phase 4 toolchain
- Modify `main.c`: `execvp()` → `exec()`, Slopix path conventions
- Link with Slopix libc instead of host libc

**Testing**:
1. Cross-compile `cmd/cc` using `tools/cc` + `cmd/as` + `cmd/ld`
2. Run `cmd/cc` on Slopix, compile test programs
3. Self-hosting test:
   ```
   cc0 = cross-compiled chibicc (from Phase 4)
   cc1 = cc0 compiles chibicc source on Slopix
   cc2 = cc1 compiles chibicc source on Slopix
   Verify: cc1 == cc2 (binary reproducibility)
   ```

**Deliverables**:
- `cmd/cc` that runs natively on Slopix
- Self-hosting capability

**Exit Criteria**:
1. `cmd/cc` compiles all cmd/ programs on Slopix
2. Self-hosting: cc compiles itself, second generation produces identical binary
3. Full native toolchain: `cmd/cc` + `cmd/as` + `cmd/ld` works entirely on Slopix

---

## Phase Summary

| Phase | Location | Key Deliverable | Test Method | Exit Criteria |
|-------|----------|-----------------|-------------|---------------|
| 1 | tools/cc | AArch64 cross-compiler | chibicc tests on Slopix (initramfs) | cmd/ builds & runs on Slopix |
| 2 | libc/ | Extended libc | cmd/tests suites | chibicc deps satisfied |
| 3 | cmd/as | Assembler | Differential vs aarch64-elf-as | .o files link & work |
| 4 | cmd/ld | Linker | Differential vs aarch64-elf-ld | Full pipeline works |
| 5 | cmd/cc | Native compiler | Self-hosting test | cc compiles cc compiles cc |

---

## libc Requirements

### Currently Available in Slopix
```
strcmp, strncmp, strlen, strcpy, strncpy, strcat, strchr, strstr
memcpy, memset, memmove
isspace, isdigit, isalpha
atoi, itoa
printf, puts
fork, wait, exec, exit
open, close, read, write, lseek, stat, unlink, dup, pipe
sbrk, mmap, munmap
```

### Must Implement (Phase 2)

**Memory** (build on sbrk):
- `malloc`, `calloc`, `realloc`, `free`

**Strings**:
- `strdup`, `strndup`, `strrchr`, `strtok`, `strncasecmp`, `memcmp`

**Numeric**:
- `strtoul`, `strtol`

**ctype**:
- `isalnum`, `ispunct`, `isxdigit`, `isupper`, `islower`

**stdio** (FILE* abstraction):
- `FILE` struct, `stdin`, `stdout`, `stderr`
- `fopen`, `fclose`, `fread`, `fwrite`, `fflush`, `fputc`, `fgetc`
- `fprintf`, `vfprintf`, `sprintf`, `snprintf`
- `open_memstream`

**Error**:
- `errno`, `strerror`
- `assert` macro

**Process**:
- `_exit`

**Filesystem**:
- `mkstemp`
- `dirname`, `basename`

**Time** (can stub for MVP):
- `time`, `localtime`

---

## AArch64 Code Generation

The main porting effort is rewriting `codegen.c` from x86-64 to AArch64.

### Register Comparison

| Role | x86-64 | AArch64 |
|------|--------|---------|
| Arguments | rdi, rsi, rdx, rcx, r8, r9 (6) | x0-x7 (8) |
| Return value | rax, rdx | x0, x1 |
| Temporaries | rax, rcx, rdx, r8-r11 | x9-x15 |
| Callee-saved | rbx, rbp, r12-r15 | x19-x28 |
| Frame pointer | rbp | x29 |
| Link register | (on stack) | x30 |
| Stack pointer | rsp | sp |

### Key Instruction Mappings

| Operation | x86-64 | AArch64 |
|-----------|--------|---------|
| Push | `push %rax` | `str x0, [sp, #-16]!` |
| Pop | `pop %rax` | `ldr x0, [sp], #16` |
| Load | `mov (%rax), %rbx` | `ldr x1, [x0]` |
| Store | `mov %rbx, (%rax)` | `str x1, [x0]` |
| Add | `add %rbx, %rax` | `add x0, x0, x1` |
| Multiply | `imul %rbx, %rax` | `mul x0, x0, x1` |
| Divide | `cqo; idiv %rbx` | `sdiv x0, x0, x1` |
| Compare | `cmp %rbx, %rax` | `cmp x0, x1` |
| Set if equal | `sete %al; movzx %al, %eax` | `cset x0, eq` |
| Branch | `je .L1` | `b.eq .L1` |
| Call | `call func` | `bl func` |
| Return | `ret` | `ret` |

### Function Prologue/Epilogue

```asm
// x86-64                      // AArch64
push %rbp                      stp x29, x30, [sp, #-16]!
mov %rsp, %rbp                 mov x29, sp
sub $N, %rsp                   sub sp, sp, #N
...                            ...
mov %rbp, %rsp                 mov sp, x29
pop %rbp                       ldp x29, x30, [sp], #16
ret                            ret
```

---

## References

- [chibicc source](https://github.com/rui314/chibicc)
- [chibicc internals book](https://www.sigbus.info/compilerbook) (Japanese)
- [AAPCS64](docs/aapcs64/aapcs64.md) - AArch64 calling convention
- [ELF for AArch64](docs/aaelf64/aaelf64.md) - Executable format
- [ARM64 instruction set](https://developer.arm.com/documentation/ddi0602/latest/)
