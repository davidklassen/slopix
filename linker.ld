ENTRY(_start)

MEMORY
{
    /* QEMU virt board RAM starts at 0x40000000. RWX for simplicity. */
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 128M
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : {
        *(.text.boot)  /* boot code first */
        *(.text*)
    } > RAM

    .rodata : {
        *(.rodata*)
    } > RAM

    .data : {
        *(.data*)
    } > RAM

    . = ALIGN(8);
    __bss_start = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    } > RAM
    __bss_end = .;

    . = ALIGN(16);
    . = . + 0x10000;  /* 64KB stack */
    __stack_top = .;

    . = ALIGN(4096);
    __pagetables_start = .;
    .pagetables (NOLOAD) : {
        *(.pagetables)
    } > RAM
    __pagetables_end = .;
}
