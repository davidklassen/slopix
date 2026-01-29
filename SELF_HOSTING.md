# Slopix Self-Hosting Design

This document describes the path toward a self-hosted slopix system where the OS
can compile itself from within itself.

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
- `/bin/buildcc` - Bootstrap helper that recompiles cc using cc
- `/lib/libc.a` - C standard library
- `/src/cc/` - Complete compiler source code
- `/src/libc/` - Complete libc source code
- `/src/*.c` - Utility source files

Running `/bin/buildcc` inside slopix compiles a new C compiler entirely within
the system. This proves the toolchain works end-to-end.

### What Requires Cross-Compilation

**The kernel** is built with the host cross-compiler:

```
aarch64-elf-gcc  → compile .c and .S files
aarch64-elf-ld   → link with linker script
aarch64-elf-objcopy → convert ELF to raw binary
```

The slopix toolchain cannot currently build the kernel.

## Architecture

### Build Script Approach

Slopix has no shell scripting. Build automation uses **C programs** that
fork/exec the toolchain:

```c
// Pattern from buildcc.c
for (int i = 0; srcs[i]; i++) {
    snprintf(cmd, sizeof(cmd), "/bin/cc -S -o /tmp/%s.s /src/%s.c",
             srcs[i], srcs[i]);
    run(cmd);  // fork + exec + wait

    snprintf(cmd, sizeof(cmd), "/bin/as -o /tmp/%s.o /tmp/%s.s",
             srcs[i], srcs[i]);
    run(cmd);
}
snprintf(cmd, sizeof(cmd), "/bin/ld -o %s /tmp/*.o /lib/libc.a", output);
run(cmd);
```

This replaces make/shell scripts with explicit C code.

### Proposed Build Programs

| Program | Purpose |
|---------|---------|
| `buildcc` | Recompile the C compiler (exists) |
| `buildas` | Recompile the assembler |
| `buildld` | Recompile the linker |
| `buildlibc` | Recompile the C library |
| `buildcmd` | Recompile userspace utilities |
| `buildkernel` | Recompile the kernel (requires toolchain work) |
| `buildall` | Orchestrate full system rebuild |

Each program knows its source files and build flags. No configuration files
needed - the build logic is the program.

### Alternative: Generic Build Tool

A single `build` program that reads a simple manifest:

```
# kernel.build
cc -I. -ffreestanding boot.S -o boot.o
cc -I. -ffreestanding kernel.c -o kernel.o
...
ld -T linker.ld -o kernel.elf *.o
objcopy -O binary kernel.elf kernel.bin
```

Pros: More flexible, single tool to maintain
Cons: Need to implement a parser, another mini-language

**Recommendation:** Start with dedicated build programs (buildcc pattern).
They're simpler and sufficient for bootstrapping. A generic tool can come later.

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

### No Dependency Tracking

Build programs always rebuild everything. For small codebase this is acceptable.
Later optimization: stat() files, compare mtimes, skip unchanged.

### No Parallel Builds

Single-threaded fork/exec/wait loop. Could parallelize with multiple children
but adds complexity. Not critical for bootstrap.

### Temporary File Management

Build programs use `/tmp`. Need to clean up on failure. Consider:
- Dedicated `/build` directory
- Cleanup on program start
- Unique subdirs per build

## Implementation Plan

### Phase 1: Userspace Build Programs

Create build programs for components that already work:

1. `buildas` - Recompile assembler
2. `buildld` - Recompile linker
3. `buildlibc` - Recompile C library
4. `buildcmd` - Recompile utilities

**Validates:** Build program pattern works for all userspace.

### Phase 2: Assembler Enhancements

Add missing directives to `cmd/as`:

1. `.equ` directive (constants)
2. `.fill` directive (padding)
3. Expression evaluation in directives
4. `.macro`/`.endm` support

**Validates:** Can assemble kernel boot code.

### Phase 3: Compiler Enhancements

Add inline assembly support to `cmd/cc`:

1. Parse asm constraints (input/output operands)
2. Generate assembly for asm statements
3. Test with kernel/cpu.h patterns

**Validates:** Can compile kernel C code.

### Phase 4: Linker Enhancements

Extend `cmd/ld` for kernel linking:

1. Add `-T script` option for linker scripts
2. Parse MEMORY regions
3. Parse SECTIONS with AT() load addresses
4. Symbol definitions (__bss_start, etc.)

**Or:** Simpler `-T kernel` mode with hardcoded layout.

### Phase 5: Binary Output

Add objcopy functionality:

1. New `objcopy` tool, or
2. Add `--oformat=binary` to ld

**Validates:** Can produce bootable kernel.bin.

### Phase 6: Kernel Build Program

Create `buildkernel`:

1. Compile kernel .c files
2. Assemble kernel .S files
3. Link with kernel layout
4. Convert to binary
5. Install to /boot/kernel.bin

### Phase 7: Integration

Create `buildall`:

1. Build toolchain (cc, as, ld)
2. Build libc
3. Build userspace
4. Build kernel
5. Report results

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

3. **Build parallelism** - Worth the complexity for faster builds?

4. **Incremental builds** - Dependency tracking worth implementing?

5. **Error recovery** - What happens if build fails mid-way? Rollback?

## Current File Inventory

### Source Files on Disk Image

```
/src/cc/           - Compiler (9 .c files, 1 .h, 13 headers in include/)
/src/libc/         - C library (4 .c files, 1 .S, 15 headers)
/src/ld/           - Linker test program
/src/*.c           - Utility sources (cat, cp, grep, sed, etc.)
```

### What's Missing from Disk

For full kernel self-hosting, need to add:

```
/src/kernel/       - All kernel sources (.c and .S)
/src/kernel/tests/ - Kernel test sources
/src/as/           - Assembler sources
/src/ld/           - Linker sources (full, not just test)
```

## References

- chibicc: https://github.com/rui314/chibicc
- ARM64 ABI: https://github.com/ARM-software/abi-aa
- ELF specification: https://refspecs.linuxfoundation.org/elf/elf.pdf
- GNU as manual: https://sourceware.org/binutils/docs/as/
- GNU ld manual: https://sourceware.org/binutils/docs/ld/
