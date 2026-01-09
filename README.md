# SLOPIX

A minimal Unix-like operating system for ARM64, built for learning.

## Current Status: M2 (Interrupts) ✅

**Completed Milestones:**
- ✅ M1: Boot
- ✅ M2: Interrupts

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
SLOPIX
Initializing interrupts...
Timer started. Waiting for interrupts...
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

The system prints a message every second (100 ticks at 100 Hz).

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

**Build:**
- `Makefile` - Build system

## Next Steps

- M3: Memory (physical page allocator, MMU setup, virtual memory)
- M4: Processes (process struct, context switching, multitasking)
- M5: Userspace (EL0 execution, syscall interface)
