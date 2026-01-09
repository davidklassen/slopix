# SLOPIX

A minimal Unix-like operating system for ARM64, built for learning.

## Current Status: M3 (Memory) ✅

**Completed Milestones:**
- ✅ M1: Boot
- ✅ M2: Interrupts
- ✅ M3: Memory

## Prerequisites

You need the following tools installed:

- **ARM64 cross-compiler**: `aarch64-elf-gcc` (macOS) or `aarch64-linux-gnu-gcc` (Linux)
- **QEMU**: `qemu-system-aarch64`

### Installing on macOS

```bash
brew install qemu aarch64-elf-gcc
```

### Installing on Linux (Debian/Ubuntu)

```bash
sudo apt-get update
sudo apt-get install qemu-system-arm gcc-aarch64-linux-gnu
```

**Note for Linux**: If using `gcc-aarch64-linux-gnu`, override the toolchain:
```bash
make CROSS_COMPILE=aarch64-linux-gnu-
```

## Building

To build the kernel:

```bash
make
```

This will produce `slopix.elf`.

To clean build artifacts:

```bash
make clean
```

## Running in QEMU

To run the kernel:

```bash
make run
```

Or manually:

```bash
qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -kernel slopix.elf
```

To exit QEMU, press `Ctrl-A` then `X`.

## Testing & Validation

### M1: Boot
After running, you should see:
```
SLOPIX
```

### M2: Interrupts
With M2 complete, you should see timer interrupts working:
```
[Timer] 1 seconds elapsed (100 ticks)
[Timer] 2 seconds elapsed (200 ticks)
[Timer] 3 seconds elapsed (300 ticks)
...
```

This demonstrates:
- ✅ GIC (Generic Interrupt Controller) initialized
- ✅ ARM Generic Timer configured for 100 Hz (10ms ticks)
- ✅ Timer interrupts firing periodically
- ✅ Exception handlers properly routing IRQs
- ✅ Timer tick counter incrementing

### M3: Memory
With M3 complete, you should see memory management initialization and tests:
```
SLOPIX

=== M3: Memory Management ===
[PMM] Initialized: 128 MB (32768 pages, 32750 free)
[PMM] Kernel end: 4005c000, Bitmap at: 4005c000 (4096 bytes)
[MMU] Initializing page tables...
[MMU] TTBR0 (identity): 4005d000
[MMU] TTBR1 (kernel high): 4005e000
[MMU] Enabling MMU...
[MMU] MMU enabled!

=== Testing Physical Memory Allocator ===
[TEST] Allocating 5 pages...
  Page 0 allocated at: 40062000
  Page 1 allocated at: 40063000
  Page 2 allocated at: 40064000
  Page 3 allocated at: 40065000
  Page 4 allocated at: 40066000
[TEST] Free pages: 32745 / 32768
[TEST] Freeing pages 1 and 3...
[TEST] Free pages: 32747 / 32768
[TEST] Allocating 2 more pages...
  Page 6 allocated at: 40063000
  Page 7 allocated at: 40065000
[TEST] Free pages: 32745 / 32768

=== M2: Interrupts ===
...
[Timer] 1 seconds elapsed (100 ticks)
```

This demonstrates:
- ✅ Physical memory manager (bitmap-based page allocator)
- ✅ MMU enabled with 4KB pages (using 2MB blocks)
- ✅ Page table setup with TTBR0 (identity map) and TTBR1 (kernel high memory)
- ✅ Page allocation and deallocation working
- ✅ Free page tracking accurate
- ✅ Memory operations functioning after MMU enable

## Project Structure

**Boot & Core:**
- `boot.S` - ARM64 assembly startup code
- `linker.ld` - Linker script for kernel layout
- `main.c` - C entry point and main loop

**I/O:**
- `uart.c/h` - PL011 UART driver for serial output
- `printf.c/h` - Minimal printf implementation

**Interrupts:**
- `exceptions.S` - Exception vector table and handlers
- `interrupts.c/h` - High-level interrupt management
- `gic.c/h` - GICv2 (Generic Interrupt Controller) driver
- `timer.c/h` - ARM Generic Timer driver

**Memory Management:**
- `memory.h` - Memory layout constants and definitions
- `pmm.c/h` - Physical memory manager (page allocator)
- `mmu.c/h` - MMU setup and page table management

**Build:**
- `Makefile` - Build system

## Next Steps

- M4: Processes (process struct, context switching, multitasking)
- M5: Userspace (EL0 execution, syscall interface)
- M6: Fork & Exec
