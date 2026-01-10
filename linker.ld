ENTRY(_start)

SECTIONS
{
    /* Physical address where kernel is loaded */
    . = 0x40000000;
    __kernel_phys_start = .;

    .text : {
        *(.text.boot)
        *(.text*)
    } :text

    .rodata : {
        *(.rodata*)
    } :rodata

    .data : {
        *(.data*)
    } :data

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } :data

    . = ALIGN(4096);
    __kernel_end = .;

    . = ALIGN(8);
    . = . + 0x8000;
    __stack_top = .;
}

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R+X */
    rodata PT_LOAD FLAGS(4); /* R */
    data PT_LOAD FLAGS(6);   /* RW */
}
