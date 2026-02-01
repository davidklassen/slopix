# Slopix Self-Hosting Design

This document describes the path toward a self-hosted slopix system where the OS
can compile itself from within itself.

For build system design, see the Build System section in [DESIGN.md](DESIGN.md).

## Vision

The end goal:

1. Start with clean source code on host
2. Cross-compile and create disk image
3. Boot slopix from disk (via bootloader)
4. Modify userspace source, rebuild → works immediately
5. Modify kernel source, rebuild, reboot → boots new kernel

Full development cycle without leaving the system.

## Architecture Overview

```
Host (one-time bootstrap)
├── Cross-compile toolchain to AArch64
├── Cross-compile kernel
├── Create disk image with sources
└── Create bootloader in pflash

QEMU boot (no -kernel flag)
├── pflash0: bootloader
├── virtio-blk: disk.img
└── DTB at 0x40000000 (QEMU provides)

Boot sequence
├── Bootloader runs from pflash
├── Reads /kernel.bin from disk
├── Passes DTB address in x0
└── Jumps to kernel

Self-hosting cycle
├── Edit /src/kernel/*.c
├── Run /bin/build in /src/kernel
├── Produces /kernel.bin
└── Reboot → new kernel runs
```

## Current State

### What Already Works

**Userspace is self-hosting.** The disk image includes:

- `/bin/cc` - C compiler (chibicc port)
- `/bin/as` - AArch64 assembler
- `/bin/ld` - ELF linker
- `/bin/ar` - Archive tool
- `/bin/build` - Generic build tool
- `/lib/libc.a` - C standard library
- `/src/` - Complete source tree

Running `/bin/build` in `/src/cmd/cc` recompiles the C compiler entirely within
the system.

### What Requires Work

| Component | Gap | Solution |
|-----------|-----|----------|
| **cc** | No inline assembly codegen | Add minimal constraint support |
| **as** | Missing `.equ`, `.fill`, `.balign` | Add directives |
| **as** | No macros | Add simple text-substitution macros |
| **ld** | No kernel memory layout | Add `-T kernel` mode |
| **ld** | No binary output | Add `--oformat=binary` |
| **boot** | QEMU `-kernel` flag | New bootloader component |

## Toolchain Gaps

### 1. C Compiler: Inline Assembly

**Current state:** Parser creates `ND_ASM` node but codegen ignores it.

**Kernel usage (21 occurrences in cpu.h + 1 in psci.c):**

```c
// Pattern A: No operands (6 uses)
__asm__ volatile("nop");
__asm__ volatile("tlbi vmalle1");

// Pattern B: Output register (10 uses)
__asm__ volatile("mrs %0, daif" : "=r"(val));

// Pattern C: Input register (5 uses)
__asm__ volatile("msr ttbr0_el1, %0" : : "r"(v));

// Pattern D: Register variable (1 use in psci.c)
register long x0 asm("x0") = PSCI_SYSTEM_OFF;
asm volatile("hvc #0" ::"r"(x0));
```

**Solution:** Implement minimal inline asm support (~100-150 LOC):

1. Parse constraint syntax: `: "=r"(var)` and `: : "r"(var)`
2. Allocate temp register (e.g., x9)
3. Substitute `%0` in asm string with register name
4. Emit: load before (input), asm string, store after (output)

Only `"r"` constraint needed. Single operand sufficient.

### 2. Assembler: Missing Directives

**Current state:** Supports `.text`, `.data`, `.global`, `.align`, `.byte`,
`.word`, `.xword`, `.ascii`, `.asciz`, `.zero`.

**Missing directives used by kernel:**

| Directive | Usage | Implementation |
|-----------|-------|----------------|
| `.equ NAME, value` | 28 uses | Store in symbol table as constant |
| `.fill count, size, value` | 18 uses | Emit `count * size` bytes |
| `.balign N` | 8 uses | Align to N-byte boundary |

**Estimated:** ~50-70 LOC total.

### 3. Assembler: Simple Macros

**Current state:** No macro support.

**Kernel usage (vectors.S):**

```asm
.macro vector_entry label
    .balign 0x80
\label:
.endm

.macro panic_vector id
    mov x0, #\id
    b   panic_entry
.endm
```

These are simple text substitution with `\param` replacement.

**Complex macro (tables.S) - will be manually expanded:**

```asm
.macro ram_blocks base, flags, count
    .set ADDR, \base
    .rept \count
        .quad ADDR + \flags
        .set ADDR, ADDR + 0x200000
    .endr
.endm
```

The `.rept`/`.set` loop requires expression evaluation and mutable state.
Instead of implementing this, manually expand the 64 entries in tables.S.

**Solution:**
- Implement simple macros for vectors.S (~50-80 LOC)
- Manually expand tables.S loop (one-time source edit)

### 4. Linker: Kernel Memory Layout

**Current state:** Hardcoded `TEXT_BASE = 0x10000`, four sections (text, rodata,
data, bss), ELF output only.

**Kernel requirements:**

```
.text.boot  @ 0x40080000 (physical, entry point)
.tables     @ 0x40080000 + boot_size, 4KB aligned
.text       @ 0xFFFF000040090000 (virtual), load at 0x40090000 (physical)
.rodata, .data, .bss follow
Symbols: __bss_start, __bss_end, __stack_top, __phys_base, __virt_base
```

**Solution:** Add `-T kernel` flag with hardcoded kernel layout (~150-200 LOC):

1. Recognize section names: `.text.boot`, `.tables`, `.text*`, etc.
2. Assign addresses per kernel memory map
3. Handle VMA != LMA for high kernel sections
4. Define linker symbols
5. Support `--oformat=binary` for raw output

No full linker script parser needed.

### 5. Variadic Functions

**Status:** Already works! chibicc has full va_list support with 224-byte
`__va_area__` buffer, register save area, and proper AArch64 ABI handling.

## Bootloader

### Purpose

The bootloader replaces QEMU's `-kernel` flag, enabling true disk-based boot:

1. Runs from pflash0 (address 0x0)
2. Initializes UART (debug output) and virtio-blk
3. Mounts slopix filesystem, finds `/kernel.bin`
4. Loads kernel to 0x40080000
5. Passes DTB address in x0, jumps to kernel

### QEMU Configuration

```bash
# Current (cross-compiled kernel)
qemu-system-aarch64 -M virt -kernel kernel.bin -drive file=disk.img ...

# Self-hosting (bootloader in pflash)
qemu-system-aarch64 -M virt \
  -drive if=pflash,format=raw,file=bootloader.bin \
  -drive if=virtio,format=raw,file=disk.img \
  -nographic
```

### DTB Handling

QEMU virt machine behavior:
- With `-kernel`: passes DTB address in x0
- With pflash boot: places DTB at RAM base (0x40000000)

The bootloader passes 0x40000000 in x0. Kernel code unchanged.

### Boot Flow

```
CPU reset
    │
    ▼
pflash0 @ 0x0 (bootloader)
    │
    ├─ Initialize UART
    ├─ Initialize virtio-blk
    ├─ Read superblock from disk
    ├─ Find root inode
    ├─ Traverse path: / → kernel.bin
    ├─ Read file blocks → 0x40080000
    │
    ▼
    ldr x0, =0x40000000    // DTB (QEMU placed it here)
    ldr x1, =0x40080000    // kernel entry
    br  x1
    │
    ▼
kernel _start @ 0x40080000
    │
    ├─ mov x19, x0         // Save DTB (existing code)
    ├─ Enable MMU
    ├─ Jump to kernel_main
    └─ ... (normal boot)
```

### Reboot Cycle

Add PSCI_SYSTEM_RESET (0x84000009) alongside existing PSCI_SYSTEM_OFF:

```c
void psci_system_reset(void) {
    register long x0 asm("x0") = PSCI_SYSTEM_RESET;
    asm volatile("hvc #0" ::"r"(x0));
}
```

Reboot triggers CPU reset → bootloader runs → loads (new) kernel from disk.

### Bootloader Size Estimate

| Component | Lines |
|-----------|-------|
| Entry, UART init | ~50 |
| Virtio-blk driver | ~200 |
| Filesystem read | ~300 |
| Kernel load, jump | ~50 |
| **Total** | ~600 |

Can reuse patterns from kernel virtio.c and fs.c but must be standalone.

## Implementation Plan

### Phase 1: Assembler Directives

Add to `cmd/as`:
- `.equ NAME, value` - constant in symbol table
- `.fill count, size, value` - emit bytes
- `.balign N` - align to N bytes

**Test:** Assemble kernel/boot.S (uses .equ)

### Phase 2: Assembler Macros

Add to `cmd/as`:
- `.macro NAME [params]` / `.endm` - define macro
- `\param` substitution in macro body
- Macro invocation

**Test:** Assemble kernel/vectors.S

### Phase 3: Manual Expansion

Edit `kernel/tables.S`:
- Expand `ram_blocks` macro to 64 `.quad` entries
- Remove `.rept`/`.set` usage

**Test:** Assemble kernel/tables.S

### Phase 4: Inline Assembly

Add to `cmd/cc`:
- Parse `: "=r"(var)` output constraint
- Parse `: : "r"(var)` input constraint
- Emit load/store around asm string
- Register substitution for `%0`

**Test:** Compile kernel/cpu.h functions

### Phase 5: Kernel Linker Mode

Add to `cmd/ld`:
- `-T kernel` flag
- Kernel section layout (boot, tables, text at high VA)
- Symbol definitions
- `--oformat=binary` for raw output

**Test:** Link kernel, produce kernel.bin

### Phase 6: Kernel Build Script

Create `kernel/build.c`:
- Compile all .c files
- Assemble all .S files
- Link with `-T kernel`
- Output to `/kernel.bin`

**Test:** Build kernel within slopix

### Phase 7: Bootloader

Create `boot/` directory:
- Standalone bootloader binary
- Virtio-blk driver (simplified)
- Filesystem traversal
- Kernel loading

**Test:** Boot slopix without -kernel flag

### Phase 8: Integration

- Update Makefile for pflash bootloader
- Test full cycle: boot → edit → rebuild → reboot

## Build System

Already complete. Key points:

**Cross-compilation (host):**
```makefile
CC=.bin/cc AS=.bin/as LD=.bin/ld .bin/build
```

Builds userspace with slopix toolchain running on host.

**Native compilation (slopix):**
```
cd /src && /bin/build
```

Same build.c files work in both environments.

**Kernel build (after toolchain enhancements):**
```
cd /src/kernel && /bin/build
# Produces /kernel.bin
```

## Testing Strategy

### Incremental Toolchain Tests

Each phase has concrete validation:

| Phase | Test |
|-------|------|
| Directives | `as` accepts kernel/boot.S |
| Macros | `as` accepts kernel/vectors.S |
| Inline asm | `cc` compiles test with mrs/msr |
| Linker | `ld -T kernel` produces correct layout |
| Binary | Output boots in QEMU |

### Triple Compilation

Verify toolchain correctness:

1. Build cc with host toolchain → cc1
2. Build cc with cc1 → cc2
3. Build cc with cc2 → cc3
4. Verify: cc2 == cc3 (bit-identical)

### Full Cycle Test

1. Boot slopix from disk
2. `cd /src/kernel`
3. Edit a file (e.g., add kprintf in kernel_main)
4. `/bin/build`
5. `reboot`
6. Verify change appears

## File Inventory

### Source on Disk

```
/src/
├── build.c              Root build script
├── lib/build.h          Build utilities
├── libc/                C library
├── cmd/                 All commands
│   ├── cc/              Compiler
│   ├── as/              Assembler
│   ├── ld/              Linker
│   ├── ar/              Archive tool
│   ├── build/           Build tool
│   └── .../             Utilities
└── kernel/              Kernel sources
    ├── build.c          Kernel build script (new)
    ├── *.c              C sources
    └── *.S              Assembly sources
```

### Runtime

```
/bin/                    Compiled binaries
/lib/libc.a             C library
/include/               Headers
/kernel.bin             Kernel image (rebuilt in place)
```

## Effort Summary

| Component | Estimated LOC |
|-----------|---------------|
| as: directives | 50-70 |
| as: simple macros | 50-80 |
| cc: inline asm | 100-150 |
| ld: kernel mode | 150-200 |
| kernel/build.c | 50-100 |
| bootloader | 500-800 |
| **Total** | ~900-1400 |

Plus one-time manual expansion of tables.S (~64 lines).

## References

- [DESIGN.md](DESIGN.md) - System design including build system
- [QEMU ARM virt docs](https://qemu-project.gitlab.io/qemu/system/arm/virt.html)
- chibicc: https://github.com/rui314/chibicc
- ARM64 ABI: https://github.com/ARM-software/abi-aa
- GNU as manual: https://sourceware.org/binutils/docs/as/
