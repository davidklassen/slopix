ENTRY(_start)

/*
 * Boot code runs at physical 0x40080000 before MMU enable.
 * QEMU virt machine loads raw binaries at RAM_BASE + 0x80000.
 * Kernel code/data runs at high virtual 0xFFFF000040080000 via TTBR1.
 */
__phys_base = 0x40080000;
__virt_base = 0xFFFF000040080000;

MEMORY
{
    /* Boot code at physical address (before MMU) */
    BOOT (rx) : ORIGIN = 0x40080000, LENGTH = 64K

    /* Kernel at high virtual address (after MMU) */
    KERNEL (rwx) : ORIGIN = 0xFFFF000040090000, LENGTH = 128M - 64K
}

SECTIONS
{
    /* Boot section at physical address - entry point */
    . = ORIGIN(BOOT);
    .text.boot : {
        *(.text.boot)
    } > BOOT

    /* Page tables at physical address, 4KB aligned */
    . = ALIGN(4096);
    .tables : {
        *(.tables)
    } > BOOT

    /* Rest of kernel at high VA, loaded after boot section */
    . = ORIGIN(KERNEL);

    .text : AT(0x40090000) {
        *(.text*)
    } > KERNEL

    .rodata : {
        *(.rodata*)
    } > KERNEL

    .data : {
        *(.data*)
    } > KERNEL

    . = ALIGN(4);
    _initramfs_start = .;
    .initramfs : {
        *(.initramfs)
    } > KERNEL
    _initramfs_end = .;

    . = ALIGN(8);
    __bss_start = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    } > KERNEL
    __bss_end = .;

    /* Stack grows down from __stack_top */
    . = ALIGN(16);
    . = . + 0x10000;  /* 64KB stack */
    __stack_top = .;

    /* Discard unused sections to prevent orphan placement issues */
    /DISCARD/ : {
        *(.rela*)
        *(.rel*)
        *(.igot*)
        *(.note*)
        *(.comment*)
    }
}
