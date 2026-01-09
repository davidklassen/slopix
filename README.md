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
With M3 complete, you should see physical memory management:
```
SLOPIX

=== M3: Memory Management ===
[PMM] Initialized: 128 MB (32768 pages, 32765 free)
[Note] MMU setup deferred to next milestone

=== Testing Physical Memory Allocator ===
[TEST] Allocating 5 pages...
  Page 0 allocated at: 0x40003000
  Page 1 allocated at: 0x40004000
  Page 2 allocated at: 0x40005000
  Page 3 allocated at: 0x40006000
  Page 4 allocated at: 0x40007000
[TEST] Free pages: 32760 / 32768
[TEST] Freeing pages 1 and 3...
[TEST] Free pages: 32762 / 32768
[TEST] Allocating 2 more pages...
  Page 6 allocated at: 0x40004000
  Page 7 allocated at: 0x40006000
[TEST] Free pages: 32760 / 32768

=== M2: Interrupts ===
...
[Timer] 1 seconds elapsed (100 ticks)
```

This demonstrates:
- ✅ Physical memory manager (bitmap-based page allocator for 4KB pages)
- ✅ Page allocation working (returns physical addresses)
- ✅ Page deallocation working
- ✅ Free page tracking accurate
- ✅ Freed pages are reused correctly (pages 1 and 3 freed, then reallocated as pages 6 and 7)
- ⏳ MMU setup deferred to M4 (will enable virtual memory with processes)

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
