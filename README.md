# SLOPIX

A minimal Unix-like operating system for ARM64, built for learning.

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

### M4: Processes
With M4 complete, you should see two kernel threads alternating:
```
=== M4: Processes ===
[PROCESS] Process management initialized
[SCHEDULER] Scheduler initialized
[PROCESS] Created process PID=1, entry=0x400000b0, stack=0x4000a000-0x4000b000
[PROCESS] Created process PID=2, entry=0x40000120, stack=0x4000c000-0x4000d000
[SCHEDULER] Added process PID=1 to run queue
[SCHEDULER] Added process PID=2 to run queue

=== M2: Interrupts ===
...
Timer and scheduler started
Two threads will alternate printing...

[Thread 1] Count: 0
[Thread 1] Count: 1
...
[Thread 1] Count: 22
[Thread 2] Count: 0
[Thread 2] Count: 1
[Thread 2] Count: 2
...
```

This demonstrates:
- ✅ Process control blocks (PCBs) with saved context
- ✅ Process creation with separate stacks
- ✅ Context switching (saves/restores registers)
- ✅ Round-robin scheduler
- ✅ Timer-driven preemptive multitasking (switches every 100ms)
- ✅ Two kernel threads running and alternating

**Note:** Threads currently run in kernel mode (EL1) with MMU disabled, sharing the same physical address space. MMU enablement with virtual memory will be added in M5, and userspace (EL0) execution will be added in M6.

### M5: Virtual Memory & MMU
With M5 complete, the MMU is enabled with identity mapping:
```
=== M5: Virtual Memory & MMU ===
[MMU] Allocating page tables...
[MMU] L1 table at 0x40007000
[MMU] L2 table (low) at 0x40008000
[MMU] L2 table (kernel) at 0x40009000
[MMU] Setting up identity mapping...
[MMU] Mapped first 1GB (0x00000000-0x3FFFFFFF)
[MMU] Mapped second 1GB (0x40000000-0x7FFFFFFF)
[MMU] MAIR_EL1 = 0x00FF4400
[MMU] TCR_EL1 configured (T0SZ=25, 39-bit VA)
[MMU] TTBR0_EL1 = 0x40007000
[MMU] Enabling MMU...
[MMU] MMU enabled! SCTLR_EL1.M = 1
[MMU] Virtual addressing active (identity mapped)
```

This demonstrates:
- ✅ 2-level page tables (L1 → L2) for 39-bit address space
- ✅ Identity mapping (VA = PA)
- ✅ Device memory for MMIO (GIC, UART)
- ✅ Normal memory for kernel/RAM
- ✅ MMU enabled with SCTLR_EL1.M = 1
- ✅ System continues running with virtual addressing

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
