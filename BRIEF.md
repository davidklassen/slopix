# SLOPIX

A minimal Unix-like operating system for ARM64, built for learning.

## Target

- Architecture: AArch64 (ARM64)
- Platform: QEMU `virt` machine
- Language: C with inline assembly where needed

## Goals

1. Boot on QEMU, print to serial console (PL011 UART)
2. Set up exception levels (drop from EL1 to EL0 for userspace)
3. Virtual memory with user/kernel separation
4. Preemptive multitasking on single core (timer-driven context switch)
5. ~10 syscalls: exit, fork, exec, wait, read, write, open, close, getpid, sbrk
6. Read-only ramdisk filesystem (initrd)
7. Custom minimal shell that can launch programs

## Non-goals

- SMP (multi-core)
- Networking
- Disk I/O beyond ramdisk
- POSIX compliance
- Portability

## Milestones

### M1: Boot
- Linker script, startup assembly
- UART driver, printf
- Output "SLOPIX" to serial

### M2: Interrupts
- GIC (Generic Interrupt Controller) setup
- Timer interrupt via ARM generic timer
- Basic exception handlers

### M3: Memory
- Physical page allocator (bitmap-based, 4KB pages)
- Page allocation and deallocation
- Free page tracking

### M4: Processes
- Process struct (context, page table, state)
- Context switching on timer tick (preemptive multitasking)
- Two kernel threads alternating (proof of concept)
- Scheduler with round-robin on interrupt-driven context switches

### M5: Virtual Memory & MMU
- Enable MMU with 4KB page granularity
- Fix device memory mapping (UART/GIC use proper 4KB pages, not 2MB blocks)
- Map kernel to high virtual addresses (0xFFFF000000000000 region)
- Identity map devices in kernel space
- Test with existing kernel threads in virtual address space
- Success: Kernel runs at virtual addresses with MMU enabled

### M6: Userspace (EL0)
- Drop to EL0 for process execution
- Syscall interface via `svc` instruction
- Exception handler for syscalls
- Implement basic syscalls: exit, getpid, write
- Create first userspace test program
- Success: Process runs at EL0 and can make syscalls

### M7: Fork & Exec
- fork: duplicate process, copy-on-write optional
- exec: load ELF from ramdisk, replace address space
- wait: parent blocks until child exits

### M8: Filesystem
- Ramdisk loaded by QEMU (-initrd flag)
- Simple flat filesystem or tar archive
- open, close, read syscalls

### M9: Shell
- Minimal shell in userspace
- Parse command, fork, exec, wait loop
- 2-3 toy programs (echo, cat, ls)

## Toolchain

- Cross compiler: aarch64-none-elf-gcc or aarch64-linux-gnu-gcc
- QEMU: qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -kernel slopix.elf

## References

- ARM Architecture Reference Manual (ARMv8-A)
- QEMU virt machine documentation
- OSDev wiki (ARM sections)
- "Operating Systems: Three Easy Pieces" (concepts)
