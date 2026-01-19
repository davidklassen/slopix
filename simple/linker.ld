ENTRY(_start)

MEMORY
{
    /* Boot code and tables at physical address */
    BOOT (rwx)   : ORIGIN = 0x40000000, LENGTH = 64K

    /* Kernel at high virtual address */
    KERNEL (rwx) : ORIGIN = 0xFFFF000040010000, LENGTH = 1M
}

SECTIONS
{
    /* Boot code at physical address (PA) */
    .text.boot : {
        *(.text.boot)
    } > BOOT

    /* Page tables at physical address (PA), 4KB aligned */
    . = ALIGN(4096);
    .tables : {
        *(.tables)
    } > BOOT

    /* Kernel sections at high virtual address (VA) */
    /* AT() sets physical load address = 0x40010000 */
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

    /* Stack at end of kernel region */
    . = ALIGN(16);
    . = . + 0x4000;  /* 16KB stack */
    __stack_top = .;
}
