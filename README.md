# SLOPIX

A minimal Unix-like operating system for ARM64, built for learning.

## AI-Generated Code

This project is 100% AI-generated code. Every line of code, documentation, and configuration has been created by an AI coding agent in response to natural language prompts.

**Transparency & Reproducibility:**
- All prompts used to generate this code are preserved in the git commit history
- Each commit message contains the exact prompt given to the AI agent
- The complete development process is transparent and reproducible
- You can trace any feature or bug fix back to its original prompt

This prompt-per-commit workflow makes SLOPIX both a functioning OS kernel and a case study in AI-assisted development.

## Current Status: M5 Complete ✅ → Next: M6 (Userspace)

**Completed Milestones:**
- ✅ M1: Boot (UART, printf)
- ✅ M2: Interrupts (GIC, timer, exception handlers)
- ✅ M3: Memory (physical page allocator)
- ✅ M4: Processes (preemptive multitasking, context switching)
- ✅ M5: Virtual Memory & MMU (MMU enabled, identity mapping, page tables)

**Next Milestone:**
- 🔨 M6: Userspace (EL0 execution, syscall interface)

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

## Running Tests

SLOPIX includes a comprehensive test suite for validating MMU functionality and other kernel subsystems.

To build the test kernel:

```bash
make test
```

This produces `slopix-test.elf`.

To run the tests:

```bash
make run-test
```

Or manually:

```bash
qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -kernel slopix-test.elf
```

The test suite includes:
- **MMU Register Configuration**: Verifies MAIR_EL1, TCR_EL1, TTBR0_EL1, SCTLR_EL1
- **MMU Page Table Structure**: Validates L1/L2 table layout and descriptors
- **MMU Pre-flight Checks**: Ensures critical mappings before MMU enable
- **MMU Post-flight Verification**: Confirms system functionality after MMU enable
- **Physical Memory Manager**: Tests page allocation/deallocation
- **Process Management**: Validates PCB creation and initialization

## Testing & Validation

To see the kernel in action, run `make run` for the main kernel or `make run-test` for the test suite.

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

**Process Management:**
- `process.c/h` - Process control blocks and creation
- `scheduler.c/h` - Round-robin scheduler
- `switch.S` - Context switching assembly

**Build:**
- `Makefile` - Build system

**Tests:**
- `tests/` - Test suite
  - `test_framework.h` - Simple test assertion framework
  - `test_main.c` - Test entry point
  - `test_pmm.c` - Physical memory manager tests
  - `test_processes.c` - Process management tests
  - `test_mmu_registers.c` - MMU register configuration tests
  - `test_mmu_tables.c` - MMU page table structure tests
  - `test_mmu_enable.c` - MMU pre/post-flight verification tests

## Documentation

Comprehensive documentation is available in the `docs/` directory:

**ARM64 Architecture:**
- **[docs/arm64-page-tables.md](docs/arm64-page-tables.md)** - Complete ARM64 page table architecture
  - Starting level determination
  - Translation table structure
  - Table/block descriptor formats
  - Translation walk examples
  - Common mistakes and debugging

- **[docs/arm64-registers.md](docs/arm64-registers.md)** - ARM64 system register specifications
  - SCTLR_EL1, TCR_EL1, TTBR0/1_EL1
  - MAIR_EL1 memory attributes
  - SPSR_EL1, ELR_EL1, ESR_EL1
  - Starting level calculation for T0SZ

**SLOPIX-Specific:**
- **[docs/slopix-memory-layout.md](docs/slopix-memory-layout.md)** - SLOPIX memory organization
  - Physical memory map (QEMU virt platform)
  - Virtual address space layout
  - Page table structure with actual addresses
  - Memory attributes by region
  - Boot requirements and critical mappings
  - Translation examples

- **[docs/qemu-virt-platform.md](docs/qemu-virt-platform.md)** - QEMU virt machine specification
  - Device memory map (GIC, UART, RTC, etc.)
  - RAM layout and configuration

All documentation includes:
- ✅ Verified against ARM Architecture Reference Manual
- ✅ Real SLOPIX code examples with actual addresses
- ✅ ARM ARM section references for verification
- ✅ Debugging guidance and common pitfalls

## Next Steps

**Upcoming Milestones:**
- M6: Userspace (EL0 execution, syscall interface)
- M7: Fork & Exec
- M8: Filesystem
- M9: Shell

See [BRIEF.md](BRIEF.md) for detailed milestone descriptions.
