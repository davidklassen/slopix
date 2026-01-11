#ifndef KERNEL_EXIT_H
#define KERNEL_EXIT_H

// ARM semihosting exit - only works when QEMU started with -semihosting
// Uses ARM semihosting SYS_EXIT (0x18) call via HLT instruction
// Parameter block contains [reason, exit_code]
static inline void kernel_exit(int exit_code) {
    // ARM semihosting parameter block for exit
    // [0] = ADP_Stopped_ApplicationExit (0x20026)
    // [1] = exit code
    unsigned long params[2] = {
        0x20026,     // ADP_Stopped_ApplicationExit
        (unsigned long)exit_code
    };

    register long x0 asm("x0") = 0x18;            // SYS_EXIT operation
    register long x1 asm("x1") = (long)&params;   // Pointer to parameter block

    asm volatile("hlt 0xf000" : : "r"(x0), "r"(x1) : "memory");

    // Fallback if semihosting not enabled
    while (1) asm volatile("wfe");
}

#endif
