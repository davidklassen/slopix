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
├── Reads /boot/kernel.bin from disk
├── Passes DTB address in x0
└── Jumps to kernel

Self-hosting cycle
├── Edit /src/kernel/*.c
├── Run /bin/build in /src/kernel
├── Produces /boot/kernel.bin
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
| **ld** | No kernel memory layout | Add `-T kernel` mode |
| **ld** | No binary output | Add `--oformat=binary` |
| **boot** | QEMU `-kernel` flag | New bootloader component |

### Recently Completed

- **as**: Directives (`.equ`, `.fill`, `.balign`, `.quad`, `.section`)
- **as**: System instructions (`msr`, `mrs`, `isb`, `dsb`, `tlbi`, `eret`, `wfe`, `wfi`, `hvc`)
- **as**: Macro support (`.macro`/`.endm` with `\param` substitution)
- **as**: Support for `adr Xn, .` (current location in inline asm)
- **as**: Timer/interrupt registers (`daif`, `cntfrq_el0`, `cntp_tval_el0`, `cntp_ctl_el0`)
- **as**: TLB invalidation (`tlbi vaae1is, Xt`)
- **kernel**: All .S files now assemble with custom assembler (boot.S, vectors.S, tables.S)
- **kernel**: All .c files now assemble with custom assembler (replaced GNU as)

## Toolchain Gaps

### 1. C Compiler: Inline Assembly ✓

**Status:** Complete. All kernel inline assembly patterns are supported.

**Kernel usage (19 occurrences in cpu.h, 2 in test_exception.c, 1 in psci.c):**

```c
// Pattern A: No operands (6 uses)
asm volatile("nop");
asm volatile("tlbi vmalle1");

// Pattern B: Output register (10 uses)
asm volatile("mrs %0, daif" : "=r"(val));

// Pattern C: Input register (5 uses)
asm volatile("msr ttbr0_el1, %0" : : "r"(v));

// Pattern D: Register variable (1 use in psci.c)
register long x0 asm("x0") = PSCI_SYSTEM_OFF;
asm volatile("hvc #0" ::"r"(x0));
```

### 2. Assembler: Missing Directives ✓

**Status:** Complete. The assembler now supports `.equ`, `.fill`, `.balign`,
`.quad`, and `.section` directives.

### 3. Assembler: System Instructions ✓

**Status:** Complete. The assembler supports `msr`, `mrs`, `isb`, `dsb`, `tlbi`,
`eret`, `wfe`, and all system registers used by the kernel.

### 4. Assembler: Simple Macros ✓

**Status:** Complete. The assembler supports `.macro`/`.endm` with `\param`
substitution. The `ram_blocks` macro in tables.S has been manually expanded
to explicit `.quad` directives.

### 5. Linker: Kernel Memory Layout

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

### 6. Variadic Functions

**Status:** Already works! chibicc has full va_list support with 224-byte
`__va_area__` buffer, register save area, and proper AArch64 ABI handling.

## Bootloader

The bootloader replaces QEMU's `-kernel` flag, enabling true disk-based boot.
It loads `/boot/kernel.bin` from the filesystem and jumps to the kernel.

See [BOOTLOADER.md](BOOTLOADER.md) for detailed design and implementation plan.

## Implementation Plan

### Phase 1: Assembler Directives ✓

Add to `cmd/as`:
- `.equ NAME, value` - constant in symbol table
- `.fill count, size, value` - emit bytes
- `.balign N` - align to N bytes

**Status:** Complete

### Phase 2: System Instructions ✓

Added system register and barrier instruction support to `cmd/as`.

**Status:** Complete. kernel/boot.S assembles with custom assembler.

### Phase 3: Assembler Macros ✓

Added `.macro`/`.endm` with `\param` substitution to `cmd/as`.

**Status:** Complete. kernel/vectors.S assembles with custom assembler.

### Phase 4: Manual Expansion ✓

Expanded `ram_blocks` macro in kernel/tables.S to explicit `.quad` directives.

**Status:** Complete. kernel/tables.S assembles with custom assembler.

### Phase 5: Kernel C Compilation ✓

**Goal:** Compile all kernel .c files with custom cc, assemble with GNU as.

**Status:** Complete. All 137 kernel tests pass.

The following has been added to `cmd/cc`:
- ✓ Inline asm: parse `: "=r"(var)` output and `: : "r"(var)` input constraints
- ✓ Inline asm: emit load/store around asm string with `%0` substitution
- ✓ Inline asm: handle `register ... asm("x0")` variable binding
- ✓ Fixed negative immediate codegen (use `mov` instead of `ldr =value` for [-65536, 65535])
- ✓ Fixed 32-bit arithmetic (use w registers for types ≤ 4 bytes)
- ✓ Fixed comparison codegen (use operand types for register width, not result type)

**Kernel C files (28 total):**

| Category | Files |
|----------|-------|
| Core | kernel.c, init.c, syscall.c, exception.c |
| Memory | pmm.c, vmm.c, elf.c |
| Filesystem | fs.c, bio.c, disk.c, pipe.c |
| Devices | uart.c, virtio.c, gic.c, timer.c, console.c |
| Process | proc.c, sched.c, sync.c, signal.c |
| Utilities | kprintf.c, string.c, dtb.c, cmdline.c, initramfs.c, file.c, psci.c |

**Inline asm patterns to support:**

```c
// Pattern A: No operands (6 uses)
asm volatile("nop");

// Pattern B: Output register (10 uses)
asm volatile("mrs %0, daif" : "=r"(val));

// Pattern C: Input register (5 uses)
asm volatile("msr ttbr0_el1, %0" : : "r"(v));

// Pattern D: Register variable + input (1 use)
register long x0 asm("x0") = PSCI_SYSTEM_OFF;
asm volatile("hvc #0" ::"r"(x0));
```

**Exit criteria:** Makefile updated to use custom cc for kernel .c → .s, GNU as for
.s → .o, GNU ld for linking. `make test` passes.

### Phase 6: Assembler for Compiler Output ✓

**Goal:** Replace GNU as with custom as for compiler-generated assembly.

**Status:** Complete. All 427 tests pass (137 kernel + 290 userspace).

The following has been added to `cmd/as`:
- ✓ `hvc #imm16` instruction for PSCI hypervisor calls
- ✓ `tlbi vaae1is, Xt` for TLB invalidation by VA
- ✓ `wfi` instruction for wait-for-interrupt
- ✓ System registers: `daif`, `cntfrq_el0`, `cntp_tval_el0`, `cntp_ctl_el0`
- ✓ Handle `adr Xn, .` (current location) without emitting relocation

**Exit criteria:** Makefile uses custom cc + custom as + GNU ld. `make test` passes.

### Phase 7: Kernel Linker Mode

Add to `cmd/ld`:
- `-T kernel` flag
- Kernel section layout (boot, tables, text at high VA)
- Symbol definitions
- `--oformat=binary` for raw output

**Exit criteria:** Makefile uses custom cc + custom as + custom ld. `make test` passes.

### Phase 8: Kernel Build Script

Create `kernel/build.c`:
- Compile all .c files
- Assemble all .S files
- Link with `-T kernel`
- Output to `/boot/kernel.bin`

**Exit criteria:** Build kernel within slopix using `/bin/build`.

### Phase 9: Bootloader

See [BOOTLOADER.md](BOOTLOADER.md) for detailed design and implementation phases.

**Exit criteria:** Boot slopix without -kernel flag.

### Phase 10: Integration

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
# Produces /boot/kernel.bin
```

## Testing Strategy

### Incremental Toolchain Tests

Each phase has concrete validation:

| Phase | Test | Status |
|-------|------|--------|
| 1. Directives | `as` parses `.equ`, `.fill`, `.balign` | ✓ |
| 2. System instrs | `as` accepts kernel/boot.S | ✓ |
| 3. Macros | `as` accepts kernel/vectors.S | ✓ |
| 4. Expansion | `as` accepts kernel/tables.S | ✓ |
| 5. Kernel C | custom cc + GNU as + GNU ld, `make test` passes | ✓ |
| 6. Assembler | custom cc + custom as + GNU ld, `make test` passes | ✓ |
| 7. Linker | custom cc + custom as + custom ld, `make test` passes | ✓ |
| 8. Build script | kernel/build.c produces kernel.bin | ✓ |
| 9. Bootloader | Boot without -kernel flag | |

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
/boot/kernel.bin             Kernel image (rebuilt in place)
```

## Effort Summary

| Component | Estimated LOC |
|-----------|---------------|
| as: directives | ✓ |
| as: system instructions | ✓ |
| as: simple macros | ✓ |
| tables.S expansion | ✓ |
| cc: inline asm | ✓ |
| as: compiler output gaps | ✓ |
| ld: kernel mode | ✓ |
| kernel/build.c | ✓ |
| bootloader | 500-800 |
| **Remaining** | ~500-800 |

## References

- [DESIGN.md](DESIGN.md) - System design including build system
- [QEMU ARM virt docs](https://qemu-project.gitlab.io/qemu/system/arm/virt.html)
- chibicc: https://github.com/rui314/chibicc
- ARM64 ABI: https://github.com/ARM-software/abi-aa
- GNU as manual: https://sourceware.org/binutils/docs/as/
