ENTRY(_start)

/*
 * Boot code runs at physical 0x40000000 before MMU enable.
 * Kernel code/data runs at high virtual 0xFFFF000040000000 via TTBR1.
 */
__phys_base = 0x40000000;
__virt_base = 0xFFFF000040000000;

MEMORY
{
    /* Boot code at physical address (before MMU) */
    BOOT (rx) : ORIGIN = 0x40000000, LENGTH = 64K

    /* Kernel at high virtual address (after MMU) */
    KERNEL (rwx) : ORIGIN = 0xFFFF000040010000, LENGTH = 128M - 64K
}

SECTIONS
{
    /* Boot section at physical address - entry point */
    . = ORIGIN(BOOT);
    .text.boot : {
        *(.text.boot)
    } > BOOT

    /* Rest of kernel at high VA, loaded after boot section */
    . = ORIGIN(KERNEL);

    .text : AT(0x40010000) {
        *(.text*)
    } > KERNEL

    .rodata : {
        *(.rodata*)
    } > KERNEL

    .data : {
        *(.data*)
    } > KERNEL

    . = ALIGN(8);
    __bss_start = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    } > KERNEL
    __bss_end = .;

    . = ALIGN(4096);
    __pagetables_start = .;
    .pagetables (NOLOAD) : {
        *(.pagetables)
    } > KERNEL
    __pagetables_end = .;

    /* Stack grows down from __stack_top */
    . = ALIGN(16);
    . = . + 0x10000;  /* 64KB stack */
    __stack_top = .;
}
