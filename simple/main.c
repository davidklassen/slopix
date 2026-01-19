// main.c - Prototype kernel entry point
//
// Validates:
//   1. Kernel runs at high VA (0xFFFF...)
//   2. TTBR0 can be switched without breaking kernel

void uart_init(void);
void uart_puts(const char *s);
void uart_puthex(unsigned long v);
void halt(void);

static inline unsigned long get_pc(void) {
    unsigned long pc;
    __asm__ volatile("adr %0, ." : "=r"(pc));
    return pc;
}

static inline unsigned long read_ttbr0_el1(void) {
    unsigned long v;
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(v));
    return v;
}

static inline void write_ttbr0_el1(unsigned long v) {
    __asm__ volatile("msr ttbr0_el1, %0" : : "r"(v));
}

static inline void tlbi_vmalle1(void) {
    __asm__ volatile("dsb sy; tlbi vmalle1; dsb sy; isb");
}

static void test_ttbr0_switch(void) {
    uart_puts("Testing TTBR0 switch... ");

    // Save original TTBR0
    unsigned long original = read_ttbr0_el1();

    // Switch to a zeroed page table (will fault on any TTBR0 access)
    // We use 0 which is invalid - any user space access would fault
    // But kernel runs from TTBR1, so we should be fine
    write_ttbr0_el1(0);
    tlbi_vmalle1();

    // If we get here, kernel survived the switch
    volatile unsigned long test = 42;
    if (test != 42) {
        uart_puts("FAIL (memory corruption)\n");
        halt();
    }

    // Restore original
    write_ttbr0_el1(original);
    tlbi_vmalle1();

    uart_puts("PASS\n");
}

void kernel_main(void) {
    uart_init();

    uart_puts("\n=== Prototype Kernel ===\n");

    // Print PC to show we're at high VA
    uart_puts("Running at PC: ");
    unsigned long pc = get_pc();
    uart_puthex(pc);
    uart_puts("\n");

    // Verify PC is in TTBR1 range
    if ((pc >> 48) == 0xFFFF) {
        uart_puts("PC in high VA range: PASS\n");
    } else {
        uart_puts("PC in high VA range: FAIL\n");
        halt();
    }

    // Test TTBR0 switch
    test_ttbr0_switch();

    uart_puts("\n=== SUCCESS ===\n");
    halt();
}
