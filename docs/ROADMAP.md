# Slopix Roadmap

A structured path from bare-metal boot to a self-hosting Unix-like OS.

## Prerequisites

Before starting:
- Comfortable with C programming
- Basic understanding of pointers and memory layout
- Familiarity with command line and make/build systems
- Optional but helpful: Any assembly language exposure

---

## Milestone 1: Hello World

**Goal**: Print "Hello World" to UART on QEMU

### What You'll Build
- Cross-compilation setup
- Minimal linker script
- Boot assembly (stack setup, BSS clear)
- UART driver (TX only)

### Essential Reading
| Resource | Focus |
|----------|-------|
| [ARMv8-A Programmer's Guide](ARMv8-A-Programmer-Guide/ARMv8-A-Programmer-Guide.md) Ch 1-4 | Architecture fundamentals |
| [OSDev: QEMU AArch64 Virt Bare Bones](https://wiki.osdev.org/QEMU_AArch64_Virt_Bare_Bones) | Step-by-step tutorial |
| [PL011 UART TRM](DDI0183_pl011_uart/DDI0183_pl011_uart.md) Ch 1-3 | Register map |

### Deliverables
- `make run` boots kernel and prints "Hello from Slopix!"
- Can exit QEMU with Ctrl+A, X

---

## Milestone 2: Interactive Serial

**Goal**: Full UART I/O with echo loop

### What You'll Build
- Complete UART driver (TX + RX)
- Proper UART initialization
- Character echo loop

### Essential Reading
| Resource | Focus |
|----------|-------|
| [PL011 UART TRM](DDI0183_pl011_uart/DDI0183_pl011_uart.md) Ch 2-3 | TX/RX FIFOs, baud rate |

### Deliverables
- Type characters and see them echoed
- Enter key gives new prompt

---

## Milestone 3: Exception Vectors

**Goal**: Handle interrupts and synchronous exceptions

### What You'll Build
- Vector table (16 entries)
- Trap frame save/restore
- Exception handler dispatch
- GICv3 initialization

### Essential Reading
| Resource | Focus |
|----------|-------|
| [ARM Exception Model](ARM-Exception-Model/ARM-Exception-Model.md) | EL0-EL3, vector layout |
| [GIC Architecture Spec](IHI0069_gic_architecture/IHI0069_gic_architecture.md) Ch 2, 4, 12 | GICv3 initialization |

### Deliverables
- Trigger undefined instruction → handler prints message
- SVC instruction triggers syscall path

---

## Milestone 4: Timer Interrupts

**Goal**: Periodic timer ticks for preemption

### What You'll Build
- Generic timer driver
- Timer interrupt handling
- Tick counter
- Delay functions

### Essential Reading
| Resource | Focus |
|----------|-------|
| [ARMv8-A Programmer's Guide](ARMv8-A-Programmer-Guide/ARMv8-A-Programmer-Guide.md) Ch 11 | Timer registers |

### Deliverables
- Timer fires every 10ms
- Prints "Tick: N" every second

---

## Milestone 5: Virtual Memory

**Goal**: Enable MMU with identity + kernel high mapping

### What You'll Build
- Page table allocator
- Identity mapping for low memory
- Kernel mapped to 0xFFFF...
- MMIO mapping for devices

### Essential Reading
| Resource | Focus |
|----------|-------|
| [ARM ARM DDI 0487](https://developer.arm.com/documentation/ddi0487/latest/) Ch D5 | Translation tables |
| [ARMv8-A Programmer's Guide](ARMv8-A-Programmer-Guide/ARMv8-A-Programmer-Guide.md) Ch 12 | MMU practical guide |

### Deliverables
- MMU enabled at EL1
- Kernel runs from high addresses
- Page fault handler catches bad access

---

## Milestone 6: Physical Memory Allocator

**Goal**: Track and allocate physical pages

### What You'll Build
- Free page list
- alloc_page() / free_page()
- Bitmap or linked list allocator

### Essential Reading
| Resource | Focus |
|----------|-------|
| [xv6 Book](xv6-book-riscv/xv6-book-riscv.md) Ch 3 | Page allocator design |
| [OSDev: Page Frame Allocation](https://wiki.osdev.org/Page_Frame_Allocation) | Algorithms |

### Deliverables
- Can allocate/free 4KB pages
- Tracks available memory from device tree

---

## Milestone 7: Process Abstraction

**Goal**: Define process structure and switch between tasks

### What You'll Build
- Process Control Block (PCB)
- Context save/restore
- Kernel stack per process
- Simple round-robin scheduler

### Essential Reading
| Resource | Focus |
|----------|-------|
| [AAPCS64](aapcs64/aapcs64.md) | Callee-saved registers |
| [xv6 Book](xv6-book-riscv/xv6-book-riscv.md) Ch 7 | Scheduling |
| [xv6-riscv swtch.S](https://github.com/mit-pdos/xv6-riscv/blob/riscv/kernel/swtch.S) | Context switch pattern |

### Deliverables
- Two kernel threads alternate printing
- Timer triggers context switch

---

## Milestone 8: User Mode

**Goal**: Run user process at EL0

### What You'll Build
- TTBR0 per-process
- User stack setup
- ERET to EL0
- User/kernel address space split

### Essential Reading
| Resource | Focus |
|----------|-------|
| [ARM ARM DDI 0487](https://developer.arm.com/documentation/ddi0487/latest/) D1.6 | EL transitions |

### Deliverables
- User process runs simple loop
- Kernel handles user exceptions

---

## Milestone 9: System Calls

**Goal**: Implement SVC-based syscall interface

### What You'll Build
- SVC handler dispatch
- write() - output to console
- exit() - terminate process
- fork() - duplicate process
- exec() - load new program

### Essential Reading
| Resource | Focus |
|----------|-------|
| [System V ABI AArch64](sysvabi64/sysvabi64.md) | Syscall conventions |
| [Linux ARM64 Syscalls](https://syscalls.mebeim.net/?table=arm64) | Reference numbers |

### Deliverables
- User program calls write() to print
- fork() creates child process
- exec() loads ELF binary

---

## Milestone 10: Block Device

**Goal**: Read/write disk sectors via Virtio

### What You'll Build
- Virtio MMIO transport
- Virtqueue implementation
- Block device driver
- Sector read/write API

### Essential Reading
| Resource | Focus |
|----------|-------|
| [Virtio Spec v1.2](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html) Ch 2, 5.2 | Virtqueues, block device |
| [OSDev: Virtio](https://wiki.osdev.org/Virtio) | MMIO transport |

### Deliverables
- Read sector 0 from disk image
- Write and read back data

---

## Milestone 11: Filesystem

**Goal**: Mount filesystem and access files

### What You'll Build
- Simple filesystem (or FAT)
- Inode abstraction
- Directory listing
- open/close/read/write syscalls

### Essential Reading
| Resource | Focus |
|----------|-------|
| [xv6 Book](xv6-book-riscv/xv6-book-riscv.md) Ch 8 | File system design |
| [OSDev: FAT](https://wiki.osdev.org/FAT) | Simple filesystem |

### Deliverables
- List files in root directory
- Read file contents to console
- Create and write new file

---

## Milestone 12: Shell

**Goal**: Interactive command interpreter

### What You'll Build
- Command line parsing
- Program execution (fork + exec)
- Basic built-ins (cd, exit)
- I/O redirection
- Pipes

### Essential Reading
| Resource | Focus |
|----------|-------|
| [xv6 Book](xv6-book-riscv/xv6-book-riscv.md) Ch 1 | Shell concepts |

### Deliverables
- `ls` lists files
- `cat file` prints contents
- `prog1 | prog2` works
- `echo hello > file` redirects output

---

## Milestone 13: Self-Hosting

**Goal**: Compile Slopix on Slopix

### What You'll Build
- Port C compiler (chibicc or tcc)
- Port assembler
- Port linker
- Port make or build tool
- Port text editor

### Essential Reading
| Resource | Focus |
|----------|-------|
| [ELF for AArch64](aaelf64/aaelf64.md) | Executable format |
| [chibicc](https://github.com/rui314/chibicc) | Small C compiler |
| [tcc](https://bellard.org/tcc/) | Tiny C compiler |

### Deliverables
- Edit source file in Slopix editor
- Run make to build kernel
- Boot newly compiled kernel

---

## Supplementary Resources

### OS Concepts
| Resource | Description |
|----------|-------------|
| [OSTEP](https://pages.cs.wisc.edu/~remzi/OSTEP/) | Free OS textbook |
| [xv6 Book](xv6-book-riscv/xv6-book-riscv.md) | Teaching OS reference |

### Reference Implementations
| Project | Description |
|---------|-------------|
| [xv6-riscv](https://github.com/mit-pdos/xv6-riscv) | Simple teaching OS |
| [raspberry-pi-os](https://github.com/s-matyukevich/raspberry-pi-os) | ARM64 tutorial |
| [seL4](https://github.com/seL4/seL4) | Verified microkernel |

### Communities
| Community | Link |
|-----------|------|
| OSDev Forums | https://forum.osdev.org/ |
| Reddit r/osdev | https://reddit.com/r/osdev |
| ARM Community | https://community.arm.com/ |
