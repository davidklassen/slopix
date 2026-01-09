# SLOPIX - M1: Boot

Milestone 1 implementation: Boot and print "SLOPIX" to serial console.

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

You should see:

```
SLOPIX
```

To exit QEMU, press `Ctrl-A` then `X`.

## Project Structure

- `boot.S` - ARM64 assembly startup code
- `linker.ld` - Linker script for kernel layout
- `main.c` - C entry point
- `uart.c/h` - PL011 UART driver for serial output
- `printf.c/h` - Minimal printf implementation
- `Makefile` - Build system

## What's Working

✅ Boot on QEMU virt machine
✅ Initialize PL011 UART
✅ Print to serial console
✅ Basic printf support (%s, %d, %x, %c)

## Next Steps

- M2: Interrupts (GIC setup, timer interrupts, exception handlers)
