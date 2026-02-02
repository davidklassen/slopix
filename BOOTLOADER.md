# Slopix Bootloader Design

This document describes the design and implementation plan for a standalone
bootloader that enables true disk-based boot without QEMU's `-kernel` flag.

## Goals

1. Boot slopix from disk image without `-kernel` flag
2. Enable the kernel self-hosting cycle: edit source, rebuild, reboot
3. Keep the bootloader minimal and simple

## Overview

```
Current boot (QEMU -kernel):
  QEMU loads kernel.bin → 0x40080000
  QEMU passes DTB in x0
  CPU starts at kernel _start

Self-hosted boot (pflash bootloader):
  CPU starts at pflash0 (0x0)
  Bootloader initializes UART, virtio-blk
  Bootloader reads /boot/kernel.bin from disk
  Bootloader loads kernel → 0x40080000
  Bootloader passes DTB (0x40000000) in x0
  Bootloader jumps to kernel _start
```

## QEMU Configuration

```bash
# Current
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M \
  -kernel kernel.bin \
  -drive file=disk.img,if=none,format=raw,id=hd0 \
  -device virtio-blk-device,drive=hd0 \
  -nographic

# With bootloader
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M \
  -drive if=pflash,format=raw,file=bootloader.bin,readonly=on \
  -drive file=disk.img,if=none,format=raw,id=hd0 \
  -device virtio-blk-device,drive=hd0 \
  -nographic
```

## Memory Map During Boot

```
Physical Address    Content
─────────────────────────────────────────────────
0x00000000          pflash0 (bootloader code)
0x08000000          GIC distributor
0x08010000          GIC CPU interface
0x09000000          PL011 UART
0x0A003E00          Virtio-blk MMIO (slot 31)
0x40000000          RAM base, DTB (~few KB)
   ...              ← stack grows down into this region
0x40080000          Initial SP / Kernel load address
   ...              ← kernel extends upward from here
```

When QEMU boots from pflash (no `-kernel` flag), it places the DTB at 0x40000000
(RAM base). The bootloader passes this address in x0 to the kernel, matching the
convention used with `-kernel`.

**TODO:** Verify DTB placement with pflash boot. QEMU virt machine documentation
suggests DTB is placed at RAM base, but this should be confirmed experimentally
in Phase 1.

The bootloader sets SP = 0x40080000. The stack grows down toward lower addresses
(toward the DTB). There's 512KB between SP and DTB - more than enough since the
bootloader stack uses only a few KB. The kernel loads at 0x40080000 and extends
upward, so stack (below SP) and kernel (at/above SP) don't overlap.

## Bootloader Components

### 1. Entry Point (boot/start.S)

Minimal assembly stub that:
1. Sets up stack pointer in RAM
2. Jumps to C code

```
_start:
    ldr x0, =0x40080000     // SP at kernel load address
    mov sp, x0              // Stack grows down toward 0x40000000
    bl  boot_main
    b   .                   // Should never return
```

The stack grows down from 0x40080000 toward the DTB at 0x40000000 (512KB gap).
The kernel is loaded at 0x40080000 and extends upward. Stack and kernel don't
overlap - stack is below SP, kernel is at and above SP.

The bootloader runs entirely in physical address space. No MMU, no virtual
addresses. All memory access uses physical addresses directly.

### 2. UART Driver (boot/uart.c)

Minimal PL011 driver for debug output:

```c
#define UART0_PA 0x09000000

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
```

No interrupts, no RX - just blocking TX for status messages.

### 3. Virtio-blk Driver (boot/virtio.c)

Simplified virtio driver for disk reads:

```c
#define VIRTIO0_PA 0x0A003E00

void virtio_init(void);
int  virtio_read(uint32_t sector, void *buf);
```

Key simplifications vs kernel driver:
- Polling only, no interrupts
- Single-threaded, no locking
- Read-only (no write support needed)
- Single 512-byte sector reads
- Queue size of 1 is sufficient

### 4. Filesystem Reader (boot/fs.c)

Minimal filesystem traversal:

```c
int fs_init(void);                           // Read superblock
int fs_read_file(const char *path, void *buf, uint32_t max_size);
```

Implementation:
1. Read superblock from block 1
2. Start at root inode (inode 1)
3. Parse directory entries to find each path component
4. Follow inode chain to final file
5. Read file data blocks sequentially

Limitations:
- Read-only
- No caching (re-read blocks as needed)
- Path must be absolute, starting with /
- Only supports regular files

Note: Indirect block support is required. The filesystem uses 1KB blocks with 12
direct block pointers per inode (12KB max). The kernel is ~64KB, so it requires
indirect blocks.

### 5. Main Boot Logic (boot/main.c)

```c
#define KERNEL_PATH     "/boot/kernel.bin"
#define KERNEL_LOAD_PA  0x40080000
#define DTB_PA          0x40000000
#define MAX_KERNEL_SIZE (256 * 1024)  // 256KB limit (filesystem max ~268KB)

void boot_main(void) {
    uart_init();
    uart_puts("slopix bootloader\n");

    virtio_init();
    if (fs_init() < 0) {
        uart_puts("fs: init failed\n");
        halt();
    }

    uart_puts("loading ");
    uart_puts(KERNEL_PATH);
    uart_puts("\n");

    int size = fs_read_file(KERNEL_PATH, (void *)KERNEL_LOAD_PA, MAX_KERNEL_SIZE);
    if (size < 0) {
        uart_puts("kernel not found\n");
        halt();
    }

    uart_puts("booting kernel\n");
    jump_to_kernel(DTB_PA, KERNEL_LOAD_PA);
}

void halt(void) {
    for (;;)
        asm volatile("wfe");
}
```

### 6. Kernel Jump (boot/start.S)

```
// Called as: jump_to_kernel(dtb_addr, entry_point)
// AArch64 calling convention: x0 = first arg, x1 = second arg
jump_to_kernel:
    // x0 = DTB address (from first argument)
    // x1 = kernel entry point (from second argument)
    br x1
```

## Directory Structure

```
boot/
├── start.S         Entry point, stack setup, jump_to_kernel
├── main.c          Boot logic
├── uart.c          UART driver
├── virtio.c        Virtio-blk driver
├── fs.c            Filesystem reader
├── boot.h          Shared definitions
└── build.c         Build script (host only)
```

## Build System

The bootloader is built as a standalone binary using the slopix toolchain:

```c
// boot/build.c
static const char *srcs[] = { "start", "main", "uart", "virtio", "fs", NULL };

int main(void) {
    for (int i = 0; srcs[i]; i++) {
        // Assemble .S or compile .c to .o
    }
    // Link with -T bootloader --oformat=binary
    // Output: .build/out/bootloader.bin
}
```

The linker needs a bootloader mode (`-T bootloader`) that:
- Places code at address 0x0 (pflash base)
- Outputs raw binary (no ELF headers)
- Entry point is first instruction

## Linker Support

Add to `cmd/ld`:

```c
// -T bootloader mode
#define BOOT_TEXT_BASE 0x0

// Section layout:
// .text   @ 0x0
// .rodata @ after .text
// .data   @ after .rodata
// .bss    @ after .data

// Output: raw binary, entry at 0x0
```

This is simpler than kernel mode - no VMA/LMA split, just linear physical layout.

Note: BSS must be zeroed by startup code. The bootloader should avoid relying on
BSS zero-initialization, or start.S must include BSS zeroing before calling C.

## Implementation Plan

### Phase 0: Remove Initramfs Dependency

Before implementing the bootloader, migrate tests away from initramfs:

1. Modify test infrastructure to load `/bin/tests` from disk instead of initramfs
2. Update Makefile to include tests binary in disk image
3. Remove `-initrd` flag from QEMU commands
4. Verify all tests pass with disk-based loading

This simplifies the boot path and removes a feature the bootloader would
otherwise need to support.

**Exit criteria:** `make test` passes without `-initrd` flag.

### Phase 1: Bootloader Linker Mode + UART

Add `-T bootloader` support to the linker and minimal UART:

1. Add bootloader memory layout (code at 0x0)
2. Support `--oformat=binary` (already done for kernel)
3. Implement minimal UART (uart_init, uart_putc, uart_puts)
4. Test with "hello world" bootloader

**Exit criteria:** Bootloader prints "slopix bootloader" to UART and halts.

### Phase 2: Virtio-blk Driver

Implement disk read capability:

1. Port virtio initialization (polling mode)
2. Implement single-sector read
3. Test by reading and printing first sector

**Exit criteria:** Bootloader reads sector 0 from disk and prints first bytes.

### Phase 3: Filesystem Reader

Implement filesystem traversal:

1. Port superblock reading
2. Implement inode reading
3. Implement directory traversal
4. Implement file reading with indirect block support (kernel is ~64KB, needs indirect blocks since direct blocks only cover 12KB)
5. Test by reading a known file

**Exit criteria:** Bootloader reads `/hello` from disk and prints contents.

### Phase 4: Kernel Loading

Complete the boot sequence:

1. Load kernel to 0x40080000
2. Jump to kernel with DTB in x0
3. Verify kernel boots correctly

**Exit criteria:** System boots to shell using bootloader instead of -kernel.

### Phase 5: Integration

Update build system and test infrastructure:

1. Add `make bootloader` target
2. Update `make run` and `make test` to use bootloader
3. Document the new boot process

**Exit criteria:** All tests pass with bootloader-based boot.

## Testing Strategy

### Unit Tests (on host)

- Filesystem parsing logic (mock disk reads)
- Path parsing

### Integration Tests (QEMU)

| Phase | Test |
|-------|------|
| 0 | Tests pass without initramfs |
| 1 | Bootloader prints to UART and halts |
| 2 | Bootloader reads disk sector |
| 3 | Bootloader reads file from filesystem |
| 4 | Bootloader loads and jumps to kernel |
| 5 | Full boot cycle with all tests passing |

### Debugging

UART output at each stage:
```
slopix bootloader
virtio: found block device
fs: superblock ok
loading /boot/kernel.bin
kernel size: 65536 bytes
booting kernel
[kernel output follows]
```

## Size Estimate

| Component | Lines |
|-----------|-------|
| start.S | ~30 |
| main.c | ~50 |
| uart.c | ~30 |
| virtio.c | ~150 |
| fs.c | ~200 |
| build.c | ~50 |
| ld: bootloader mode | ~50 |
| **Total** | ~560 |

## Error Handling

All errors print a message to UART and halt:

```c
void panic(const char *msg) {
    uart_puts("boot: ");
    uart_puts(msg);
    uart_puts("\n");
    halt();
}
```

Error conditions:
- Virtio device not found or init failed
- Filesystem superblock invalid (magic mismatch)
- Kernel file not found
- Kernel too large (>256KB)

## Future Considerations

Not planned for initial implementation, but possible extensions:

- Boot configuration file (kernel path, command line)
- Multiple kernel support (fallback on failure)
- Kernel integrity checking (checksum)
- Network boot

## References

- [QEMU virt machine](https://qemu-project.gitlab.io/qemu/system/arm/virt.html)
- [ARM Cortex-A57 TRM](https://developer.arm.com/documentation/ddi0488)
- [PL011 UART TRM](https://developer.arm.com/documentation/ddi0183)
- [Virtio Spec v1.2](https://docs.oasis-open.org/virtio/virtio/v1.2/)
- [SELF_HOSTING.md](SELF_HOSTING.md) - Self-hosting roadmap
- [DESIGN.md](DESIGN.md) - System design
