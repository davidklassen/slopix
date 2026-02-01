# Slopix Self-Hosting Design

This document describes the path toward a self-hosted slopix system where the OS
can compile itself from within itself.

For build system design and implementation details, see [BUILD.md](BUILD.md).

## Vision

Boot slopix, modify source code on disk, recompile the kernel and userspace
programs, reboot into the new kernel. Full development cycle without leaving
the system.

## Current State

### What Already Works

**Userspace is self-hosting.** The disk image includes:

- `/bin/cc` - C compiler (chibicc port)
- `/bin/as` - AArch64 assembler
- `/bin/ld` - ELF linker
- `/bin/ar` - Archive tool
- `/bin/build` - Generic build tool (see BUILD.md)
- `/lib/libc.a` - C standard library
- `/src/` - Complete source tree (cmd/, libc/, lib/)

Running `/bin/build` in `/src/cmd/cc` recompiles the C compiler entirely within
the system. This proves the toolchain works end-to-end.

### What Requires Cross-Compilation

**The kernel** is built with the host cross-compiler:

```
aarch64-elf-gcc  → compile .c and .S files
aarch64-elf-ld   → link with linker script
aarch64-elf-objcopy → convert ELF to raw binary
```

The slopix toolchain cannot currently build the kernel due to missing features
(see Gaps section below).

## Architecture

### Build System

Slopix has no shell scripting. Build automation uses **C programs** that
fork/exec the toolchain. See [BUILD.md](BUILD.md) for the complete design.

**Key concepts:**

- `/bin/build` - Generic build tool that compiles and runs `build.c` files
- `build.c` - Per-directory build script (C program, not manifest)
- `.build/` - Intermediate artifacts (object files)
- `build/` - Output artifacts (binaries, libraries)
- Hierarchical builds with artifact bubbling

**Example build.c:**

```c
#include "build.h"

static const char *srcs[] = {"main", "tokenize", "parse", NULL};

int main() {
    for (int i = 0; srcs[i]; i++) {
        if (compile(srcs[i]) != 0) return 1;
    }
    return link_objs("build/bin/cc", srcs);
}
```

This replaces make/shell scripts with explicit C code, while providing
a uniform interface (`/bin/build`) across all components.

## Gaps: Kernel Compilation

### 1. Inline Assembly (CRITICAL)

**Problem:** Kernel code uses GCC inline assembly for system register access:

```c
// kernel/cpu.h
static inline uint64_t read_daif(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, daif" : "=r"(val));
    return val;
}
```

chibicc parses `asm` statements but doesn't generate code for them.

**Solutions:**

A. **Add inline asm support to chibicc** - Parse constraints, emit assembly
   directly. Significant work (~200-400 LOC).

B. **Use external assembly files** - Move all system register ops to .S files,
   call them as functions from C. Slower but avoids compiler changes.

C. **Intrinsic functions** - Add `__mrs()`, `__msr()` as compiler built-ins.
   Targeted solution for the specific operations needed.

**Affected code:**
- `kernel/cpu.h` - 14 inline asm statements (MRS/MSR, barriers, TLB ops)
- `kernel/psci.c` - HVC instruction for PSCI calls

### 2. Assembler Macros (CRITICAL)

**Problem:** Kernel assembly uses macros extensively:

```asm
// kernel/vectors.S
.macro vector_entry label
.align 7
\label:
    // 128 bytes per vector entry
.endm

vector_entry el1_sync
vector_entry el1_irq
...
```

slopix `as` doesn't support `.macro`/`.endm`.

**Solutions:**

A. **Add macro support to assembler** - Track macro definitions, expand on use.
   Moderate work (~150-250 LOC).

B. **Preprocess with C preprocessor** - Use `#define` macros instead:
   ```c
   #define VECTOR_ENTRY(label) \
       .align 7; \
   label:
   ```
   Run through `cc -E` before assembling.

C. **Manually expand macros** - One-time expansion, maintain expanded code.
   Ugly but works.

### 3. Assembler Directives (CRITICAL)

**Problem:** Missing assembler directives:

| Directive | Usage | Count |
|-----------|-------|-------|
| `.equ` | Constant definitions | 20+ |
| `.fill` | Zero-fill tables | 18 |
| `.balign` | Alignment with expression | 11 |

```asm
// kernel/tables.S
.equ PAGE_SIZE, 4096
.equ L0_SHIFT, 39
.fill 512 - 4, 8, 0  // Fill remaining table entries
```

**Solutions:**

A. **Add missing directives** - Straightforward implementation:
   - `.equ`: Store in symbol table as absolute
   - `.fill count, size, value`: Emit `count * size` bytes
   - Expression evaluation in `.balign`

B. **C preprocessor workaround** - For constants only:
   ```c
   #define PAGE_SIZE 4096
   ```

### 4. Linker Scripts (CRITICAL)

**Problem:** Kernel needs complex memory layout:

```ld
// kernel/linker.ld
MEMORY {
    boot : ORIGIN = 0x40080000, LENGTH = 64K
    kernel : ORIGIN = 0xFFFF000040090000, LENGTH = 16M
}

SECTIONS {
    .text.boot : { *(.text.boot) } > boot
    .text : { *(.text) } > kernel AT > boot
}
```

slopix `ld` uses hardcoded layout (base 0x10000, linear sections).

**Solutions:**

A. **Add linker script support** - Full parser for MEMORY/SECTIONS/AT.
   Significant work (~400-600 LOC).

B. **Special kernel linker mode** - Add `-T kernel` flag with hardcoded kernel
   layout. Less flexible but simpler.

C. **Two-stage linking** - Link boot code separately, concatenate binaries.
   Awkward but avoids linker changes.

### 5. Binary Output (HIGH)

**Problem:** QEMU loads raw binary, not ELF:

```makefile
$(OBJCOPY) -O binary kernel.elf kernel.bin
```

slopix has no objcopy equivalent.

**Solutions:**

A. **Add objcopy tool** - Read ELF, write raw sections. Simple for `-O binary`
   case (~100-150 LOC).

B. **Direct binary output in ld** - Add `-Ttext=ADDR --oformat=binary` flags.

C. **Bootloader that loads ELF** - Change boot process instead of tools.

### 6. Variadic Functions (MEDIUM)

**Problem:** `kprintf` uses `va_list`:

```c
void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    // ...
}
```

chibicc may not fully support `__builtin_va_*`.

**Solution:** Verify chibicc's variadic support. If broken, implement manually
using AArch64 calling convention knowledge.

## Gaps: Build Infrastructure

Most build infrastructure gaps are addressed by [BUILD.md](BUILD.md):

- **Temporary files** → Solved: `.build/` directory for intermediates
- **Output organization** → Solved: `build/` directory mirrors target filesystem
- **Cleanup** → Solved: `/bin/build clean` removes artifacts

**Remaining limitations (deferred):**

- **No dependency tracking** - Always rebuild everything (mtime not available)
- **No parallel builds** - Sequential execution (acceptable for small codebase)
- **exec() limitations** - Single command string, no quoting for spaces

## Implementation Plan

### Build System (see BUILD.md)

The build system migration is tracked in [BUILD.md](BUILD.md) Phases 1-6.
Once complete, userspace is fully self-hosting with `/bin/build`.

### Kernel Self-Hosting

After build system is in place, kernel self-hosting requires toolchain work:

**Phase K1: Assembler Enhancements**

Add missing directives to `cmd/as`:

1. `.equ` directive (constants)
2. `.fill` directive (padding)
3. Expression evaluation in directives
4. `.macro`/`.endm` support

**Validates:** Can assemble kernel boot code.

**Phase K2: Compiler Enhancements**

Add inline assembly support to `cmd/cc`:

1. Parse asm constraints (input/output operands)
2. Generate assembly for asm statements
3. Test with kernel/cpu.h patterns

**Validates:** Can compile kernel C code.

**Phase K3: Linker Enhancements**

Extend `cmd/ld` for kernel linking:

1. Add `-T script` option for linker scripts
2. Parse MEMORY regions
3. Parse SECTIONS with AT() load addresses
4. Symbol definitions (__bss_start, etc.)

**Or:** Simpler `-T kernel` mode with hardcoded layout.

**Phase K4: Binary Output**

Add objcopy functionality:

1. New `objcopy` tool, or
2. Add `--oformat=binary` to ld

**Validates:** Can produce bootable kernel.bin.

**Phase K5: Kernel Build**

Create `kernel/build.c`:

1. Compile kernel .c files
2. Assemble kernel .S files
3. Link with kernel layout
4. Convert to binary

After this phase, `/bin/build` in `/src` builds the entire system.

## Testing Strategy

### Compiler/Assembler Tests

Add test cases for new features:

```c
// Test inline asm
uint64_t test_mrs(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}
```

```asm
// Test assembler directives
.equ TEST_CONST, 42
.fill 8, 1, 0xFF
```

### Build Verification

Each build program should verify its output:

1. Build completes without error
2. Output file exists and has expected size
3. For executables: can be loaded and run
4. For kernel: boots in QEMU

### Self-Hosting Verification

Ultimate test: triple compilation

1. Build cc with host toolchain → cc1
2. Build cc with cc1 → cc2
3. Build cc with cc2 → cc3
4. Compare cc2 and cc3 (should be identical)

## Alternatives Considered

### Keep Kernel Cross-Compiled

Accept that kernel always needs host toolchain. Focus self-hosting on
userspace only.

**Pros:** Much simpler, already works
**Cons:** Not "true" self-hosting, still need Linux host

### Rewrite Kernel in Pure C

Eliminate assembly files, use only C with function calls to small asm stubs.

**Pros:** Simplifies toolchain requirements
**Cons:** Major kernel rewrite, performance impact, some things can't be C

### Use Different Bootloader

UEFI boot instead of raw binary. UEFI can load ELF directly.

**Pros:** Eliminates objcopy requirement
**Cons:** UEFI is complex, adds dependencies

## Open Questions

1. **Linker script complexity** - How much of GNU ld script syntax do we need?
   Minimal subset vs. compatibility?

2. **Inline asm syntax** - Follow GCC exactly or simplified subset?

Build infrastructure questions (parallelism, incremental builds, error recovery)
are addressed in [BUILD.md](BUILD.md).

## Current File Inventory

### Source Files on Disk Image

With BUILD.md's `-m` flag, the full source tree is copied:

```
/src/lib/          - Build utilities (build.h)
/src/libc/         - C library sources and headers
/src/cmd/          - All command sources
  /src/cmd/cc/     - Compiler
  /src/cmd/as/     - Assembler
  /src/cmd/ld/     - Linker
  /src/cmd/ar/     - Archive tool
  /src/cmd/build/  - Build tool
  /src/cmd/*/      - Utilities (cat, ls, shell, etc.)
/src/build.c       - Root build script
```

### What's Missing (for kernel self-hosting)

```
/src/kernel/       - All kernel sources (.c and .S)
```

Kernel sources can be added to the sync once the toolchain supports
building the kernel (inline asm, linker scripts, etc.).

## References

- [BUILD.md](BUILD.md) - Build system design
- chibicc: https://github.com/rui314/chibicc
- ARM64 ABI: https://github.com/ARM-software/abi-aa
- ELF specification: https://refspecs.linuxfoundation.org/elf/elf.pdf
- GNU as manual: https://sourceware.org/binutils/docs/as/
- GNU ld manual: https://sourceware.org/binutils/docs/ld/
