ENTRY(_start)

SECTIONS
{
    /* Physical address where kernel is loaded */
    . = 0x40000000;
    __kernel_phys_start = .;

    .text : {
        *(.text.boot)
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(4096);
    __kernel_end = .;

    . = ALIGN(8);
    . = . + 0x8000;
    __stack_top = .;
}
