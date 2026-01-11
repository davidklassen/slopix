ENTRY(_start)

SECTIONS
{
    /* Link at virtual address, but load at physical address */
    . = 0xFFFFFF8040000000;
    __kernel_virt_start = .;

    .text : AT(0x40000000) {
        *(.text.boot)
        *(.text*)
    } :text

    .rodata : AT(ADDR(.rodata) - 0xFFFFFF8000000000) {
        *(.rodata*)
    } :rodata

    .data : AT(ADDR(.data) - 0xFFFFFF8000000000) {
        *(.data*)
    } :data

    .bss : AT(ADDR(.bss) - 0xFFFFFF8000000000) {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } :data

    . = ALIGN(4096);
    __kernel_end = .;

    /* Allocate 32KB kernel stack (0x8000 = 32768 bytes) */
    . = . + 0x8000;
    __stack_top = .;

    /* Physical address aliases for boot code */
    __kernel_phys_start = 0x40000000;
    __stack_top_phys = __stack_top - 0xFFFFFF8000000000;
    __bss_start_phys = __bss_start - 0xFFFFFF8000000000;
    __bss_end_phys = __bss_end - 0xFFFFFF8000000000;
}

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R+X */
    rodata PT_LOAD FLAGS(4); /* R */
    data PT_LOAD FLAGS(6);   /* RW */
}
