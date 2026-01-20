# Slopix Design

Bare-metal AArch64 kernel for QEMU virt board.

## Target Hardware

QEMU virt machine with:
- CPU: Cortex-A57 (ARMv8-A)
- RAM: 128MB at 0x4000_0000
- UART: PL011 at 0x0900_0000
- Interrupt controller: GICv2
- Timer: ARM Generic Timer
- Block device: Virtio-blk at 0x0a00_0000

## Boot Sequence

1. QEMU loads kernel.elf at 0x4000_0000, jumps to _start
2. _start (boot.S) - runs at physical address:
   - Configure MMU registers (MAIR, TCR, TTBR0, TTBR1)
   - Enable MMU with data and instruction caches
   - Set VBAR_EL1 to exception vectors (high VA)
   - Set SP to __stack_top (high VA)
   - Enable FP/SIMD via CPACR_EL1
   - Zero BSS section
   - Jump to kernel_main at high VA
3. kernel_main() - runs at high virtual address:
   - Initialize UART
   - Initialize physical memory allocator
   - Initialize interrupts (GIC + timer)
   - Start first user process

Page tables are statically defined in tables.S:
- Identity tables (TTBR0): VA = PA for boot code
- Kernel tables (TTBR1): VA = PA + 0xFFFF_0000_0000_0000

## Subsystems

| Subsystem | Files | Prefix | Description |
|-----------|-------|--------|-------------|
| CPU | cpu.h | - | CPU primitives (wfi, isb, irq control, system registers) |
| Board | board.h | - | QEMU virt constants (addresses, PA/VA conversion) |
| UART | uart.c/h | uart_ | PL011 serial driver |
| GIC | gic.c/h | gic_ | GICv2 interrupt controller driver |
| Timer | timer.c/h | timer_ | ARM Generic Timer driver |
| Exception | exception.c, vectors.S | - | Exception dispatch and trap frame handling |
| PMM | pmm.c/h | pmm_ | Physical memory manager (page allocator) |
| VMM | vmm.c/h | vmm_ | Virtual memory manager (user page tables) |
| Process | proc.c/h, switch.S | proc_, swtch | Process management and scheduling |
| Syscall | syscall.c | sys_ | System call implementations |
| ELF | elf.c/h | elf_ | ELF binary loader |
| InitRAMFS | initramfs.c/h | initramfs_ | Initial RAM filesystem |
| Utilities | kprintf.c/h | kprintf, kpanic, ksleep | Kernel utilities (k-prefix) |

### API Conventions

Function naming: `subsystem_action()` (e.g., `pmm_alloc`, `vmm_map_page`, `timer_init`)

Return values:
- Pointers: NULL on failure
- Integers: 0 on success, -1 on failure

Kernel utilities use `k` prefix to distinguish from userspace equivalents.

## Memory Map

### Physical Memory (QEMU virt)

```
0x0000_0000 - 0x07FF_FFFF   Flash (unused)
0x0800_0000 - 0x08FF_FFFF   Reserved
0x0900_0000                 PL011 UART
0x0901_0000                 RTC
0x0a00_0000 - 0x0a00_3FFF   Virtio MMIO (32 devices, 0x200 each)
0x0c00_0000 - 0x0DFF_FFFF   Platform bus
0x0e00_0000                 Secure SRAM
0x4000_0000 - 0x47FF_FFFF   RAM (128MB default)
```

### Virtual Memory Layout (with MMU)

ARM64 uses two page table base registers:
- TTBR0_EL1: translates 0x0000... addresses (user space)
- TTBR1_EL1: translates 0xFFFF... addresses (kernel space)

Hardware selects TTBR based on bit 63 of virtual address.

```
User (TTBR0, per-process):
0x0000_0000_0001_0000   Code (.text) - starts at 64KB to catch NULL derefs
0x0001_0000_0000_0000   Stack (grows down from top of user space)

Kernel (TTBR1, shared):
0xFFFF_0000_0000_0000   Direct map of all physical RAM
0xFFFF_0000_4000_0000   Kernel code (mapped from phys 0x4000_0000)
0xFFFF_0000_0900_0000   Device MMIO
```

On context switch, only TTBR0 changes. TTBR1 stays constant.

### Page Table Format

4KB granule, 4-level tables, 48-bit VA:

| Level | VA bits | Entry covers | Entries |
|-------|---------|--------------|---------|
| 0     | [47:39] | 512 GB       | 512     |
| 1     | [38:30] | 1 GB         | 512     |
| 2     | [29:21] | 2 MB         | 512     |
| 3     | [20:12] | 4 KB         | 512     |

Table entry format (64 bits):
```
[63:52] Upper attributes (UXN, PXN, etc.)
[51:48] Reserved
[47:12] Output address (next table or page frame)
[11:2]  Lower attributes (AF, SH, AP, etc.)
[1:0]   Entry type (00=invalid, 01=block, 11=table/page)
```

## Exception Model

### Vector Table

16 entries at 128-byte intervals, 2KB aligned:

| Offset | Exception from | Type |
|--------|----------------|------|
| 0x000  | Current EL, SP0 | Synchronous |
| 0x080  | Current EL, SP0 | IRQ |
| 0x100  | Current EL, SP0 | FIQ |
| 0x180  | Current EL, SP0 | SError |
| 0x200  | Current EL, SPx | Synchronous |
| 0x280  | Current EL, SPx | IRQ |
| 0x300  | Current EL, SPx | FIQ |
| 0x380  | Current EL, SPx | SError |
| 0x400  | Lower EL, AArch64 | Synchronous |
| 0x480  | Lower EL, AArch64 | IRQ |
| 0x500  | Lower EL, AArch64 | FIQ |
| 0x580  | Lower EL, AArch64 | SError |
| 0x600  | Lower EL, AArch32 | Synchronous |
| 0x680  | Lower EL, AArch32 | IRQ |
| 0x700  | Lower EL, AArch32 | FIQ |
| 0x780  | Lower EL, AArch32 | SError |

### Exception Syndrome (ESR_EL1)

```
[31:26] EC  - Exception Class
[25]    IL  - Instruction Length (0=16-bit, 1=32-bit)
[24:0]  ISS - Instruction Specific Syndrome
```

Key EC values:
| EC | Exception |
|----|-----------|
| 0x00 | Unknown |
| 0x15 | SVC from AArch64 (syscall) |
| 0x20 | Instruction abort, lower EL |
| 0x21 | Instruction abort, same EL |
| 0x24 | Data abort, lower EL |
| 0x25 | Data abort, same EL |

### Trap Frame

Saved on kernel stack on exception entry:

| Offset | Register(s) | Size |
|--------|-------------|------|
| 0      | x0-x29      | 240B |
| 240    | x30 (LR)    | 8B   |
| 248    | SP_EL0      | 8B   |
| 256    | ELR_EL1     | 8B   |
| 264    | SPSR_EL1    | 8B   |

Total: 272 bytes (must be 16-byte aligned).

## Interrupt Controller (GICv2)

### Memory Map

| Component | Base Address | Size |
|-----------|--------------|------|
| Distributor (GICD) | 0x0800_0000 | 64KB |
| CPU Interface (GICC) | 0x0801_0000 | 64KB |

### Key Registers

Distributor (GICD):
- GICD_CTLR (0x000): Control register
- GICD_ISENABLER0 (0x100): Interrupt set-enable for SGI/PPI
- GICD_IPRIORITYR (0x400+): Interrupt priority (byte per interrupt)

CPU Interface (GICC):
- GICC_CTLR (0x00): Control register
- GICC_PMR (0x04): Priority mask
- GICC_IAR (0x0C): Interrupt acknowledge (read returns INTID)
- GICC_EOIR (0x10): End of interrupt

### Initialization Sequence

1. GICD_CTLR = 1 (enable distributor)
2. GICC_PMR = 0xFF (accept all priorities)
3. GICC_CTLR = 1 (enable CPU interface)

### Interrupt Flow

1. Device asserts interrupt
2. GIC signals CPU via IRQ
3. CPU reads GICC_IAR to acknowledge (returns INTID)
4. CPU handles interrupt
5. CPU writes INTID to GICC_EOIR to signal completion

### Key Interrupt IDs

| INTID | Source |
|-------|--------|
| 30    | Non-secure Physical Timer (EL1) |
| 33    | UART0 |
| 48+   | Virtio devices |

## Timer

ARM Generic Timer, accessed via system registers.

### Registers

| Register | Purpose |
|----------|---------|
| CNTFRQ_EL0 | Timer frequency (Hz) |
| CNTPCT_EL0 | Physical counter value |
| CNTP_TVAL_EL0 | Timer value (countdown) |
| CNTP_CTL_EL0 | Timer control |

### CNTP_CTL_EL0 bits

| Bit | Name | Function |
|-----|------|----------|
| 0 | ENABLE | Timer enabled |
| 1 | IMASK | Interrupt mask (1=masked) |
| 2 | ISTATUS | Interrupt pending (read-only) |

### Timer Setup (10ms tick at 62.5MHz)

```
period = CNTFRQ_EL0 / 100  // 625000 cycles for 10ms
CNTP_TVAL_EL0 = period
CNTP_CTL_EL0 = 1           // Enable, unmask
```

On interrupt: write CNTP_TVAL_EL0 again to reset countdown.

## Process Structure

```
struct proc {
    enum state { UNUSED, RUNNABLE, RUNNING, SLEEPING, ZOMBIE };
    int pid;
    struct proc *parent;

    // Memory
    uint64_t *pagetable;    // TTBR0 value
    uint64_t sz;            // Process memory size

    // Kernel stack
    char *kstack;           // Bottom of kernel stack
    struct trapframe *tf;   // Trap frame on kstack

    // Context switch
    struct context ctx;     // Saved kernel context

    // Scheduler
    void *chan;             // Sleep channel

    // Files
    struct file *ofile[NOFILE];
    struct inode *cwd;
};

struct context {
    uint64_t x19, x20, x21, x22, x23;
    uint64_t x24, x25, x26, x27, x28;
    uint64_t x29;           // Frame pointer
    uint64_t x30;           // Return address
    uint64_t sp;
};
```

## Context Switch Flow

```
Process A running at EL0
        |
        v
Timer IRQ fires
        |
        v
vectors.S: save trapframe to A->kstack
        |
        v
irq_handler() -> timer_tick() -> yield()
        |
        v
sched(): save A->ctx (x19-x30, sp)
        |
        v
swtch(&A->ctx, &scheduler_ctx)
        |
        v
scheduler(): find RUNNABLE proc B
        |
        v
swtch(&scheduler_ctx, &B->ctx)
        |
        v
B resumes in sched(), returns to irq_handler
        |
        v
vectors.S: restore trapframe from B->kstack
        |
        v
ERET to EL0, B continues
```

## Filesystem Layout (on-disk)

Block size: 1024 bytes

```
Block 0:    Boot block (unused)
Block 1:    Superblock
Block 2-L:  Log blocks
Block L+1:  Inode blocks start
Block I+1:  Bitmap block
Block B+1:  Data blocks start
```

### Superblock

```
struct superblock {
    uint32_t magic;       // 0x10203040
    uint32_t size;        // Total blocks
    uint32_t nblocks;     // Data blocks
    uint32_t ninodes;     // Inode count
    uint32_t nlog;        // Log blocks
    uint32_t logstart;    // First log block
    uint32_t inodestart;  // First inode block
    uint32_t bmapstart;   // Bitmap block
};
```

### Inode (on-disk)

```
struct dinode {
    uint16_t type;        // 0=free, 1=file, 2=dir, 3=device
    uint16_t major;       // Device major (if type=3)
    uint16_t minor;       // Device minor
    uint16_t nlink;       // Link count
    uint32_t size;        // File size
    uint32_t addrs[12];   // Direct blocks
    uint32_t indirect;    // Indirect block
};
```

Max file size: 12 direct + 256 indirect = 268 blocks = 268KB

### Directory Entry

```
struct dirent {
    uint16_t inum;        // Inode number (0 = free)
    char name[14];        // File name
};
```

## System Call Table

Implemented syscalls:

| x8 | Name | x0 | x1 | x2 | Return |
|----|------|----|----|-----|--------|
| 0 | write | fd | buf | n | bytes written |
| 1 | exit | status | - | - | - |
| 2 | read | fd | buf | n | bytes read |
| 3 | sleep | ms | - | - | 0 |
| 4 | getpid | - | - | - | pid |
| 5 | fork | - | - | - | child pid or 0 |
| 6 | wait | - | - | - | child pid |
| 7 | exec | cmdline | - | - | argc or -1 |

Planned syscalls (for filesystem):

| x8 | Name | x0 | x1 | x2 | Return |
|----|------|----|----|-----|--------|
| 8 | open | path | flags | - | fd or -1 |
| 9 | close | fd | - | - | 0 or -1 |
| 10 | pipe | fds[2] | - | - | 0 or -1 |
| 11 | dup | fd | - | - | new fd |
| 12 | fstat | fd | &stat | - | 0 or -1 |
| 13 | mkdir | path | - | - | 0 or -1 |
| 14 | chdir | path | - | - | 0 or -1 |
| 15 | mknod | path | major | minor | 0 or -1 |
| 16 | link | old | new | - | 0 or -1 |
| 17 | unlink | path | - | - | 0 or -1 |
| 18 | sbrk | n | - | - | old break |
| 19 | kill | pid | - | - | 0 or -1 |
| 20 | uptime | - | - | - | ticks |

## Virtio Block Device

MMIO transport at 0x0a00_0000.

Note: QEMU uses legacy virtio interface by default. Use `-global virtio-mmio.force-legacy=false` for modern interface. Registers below are for modern (v2) interface.

### Registers (offsets from base)

| Offset | Name | R/W |
|--------|------|-----|
| 0x000 | MagicValue | R |
| 0x004 | Version | R |
| 0x008 | DeviceID | R |
| 0x00c | VendorID | R |
| 0x010 | DeviceFeatures | R |
| 0x014 | DeviceFeaturesSel | W |
| 0x020 | DriverFeatures | W |
| 0x024 | DriverFeaturesSel | W |
| 0x030 | QueueSel | W |
| 0x034 | QueueNumMax | R |
| 0x038 | QueueNum | W |
| 0x044 | QueueReady | W |
| 0x050 | QueueNotify | W |
| 0x060 | InterruptStatus | R |
| 0x064 | InterruptACK | W |
| 0x070 | Status | RW |
| 0x100 | QueueDescLow | W |
| 0x104 | QueueDescHigh | W |
| 0x110 | QueueDriverLow | W |
| 0x114 | QueueDriverHigh | W |
| 0x120 | QueueDeviceLow | W |
| 0x124 | QueueDeviceHigh | W |

### Initialization

1. Write 0 to Status (reset)
2. Write 1 to Status (ACKNOWLEDGE)
3. Write 3 to Status (ACKNOWLEDGE | DRIVER)
4. Read/negotiate features
5. Write 11 to Status (ACKNOWLEDGE | DRIVER | FEATURES_OK)
6. Setup virtqueue
7. Write 15 to Status (ACKNOWLEDGE | DRIVER | FEATURES_OK | DRIVER_OK)

### Block Request

```
struct virtio_blk_req {
    uint32_t type;        // 0=read, 1=write
    uint32_t reserved;
    uint64_t sector;
    // data follows (512 bytes per sector)
    // status byte at end (0=ok, 1=ioerr, 2=unsupported)
};
```

## References

- [ARM Cortex-A Programmer's Guide](docs/ARMv8-A-Programmer-Guide/)
- [ARM GIC Architecture Spec](docs/IHI0069_gic_architecture/)
- [PL011 UART TRM](docs/DDI0183_pl011_uart/)
- [Virtio Spec v1.2](https://docs.oasis-open.org/virtio/virtio/v1.2/)
- [xv6 Book](docs/xv6-book-riscv/)
