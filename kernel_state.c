#include "kernel_state.h"
#include <stdint.h>

bool kernel_in_higher_half(void) {
    // Get current program counter using ADR instruction
    // ADR loads PC-relative address into register
    uint64_t pc;
    __asm__ volatile("adr %0, ." : "=r"(pc));

    // Check if PC is in higher-half range (TTBR1)
    // Higher-half addresses have top bits set: 0xFFFF_xxxx_xxxx_xxxx
    return pc >= 0xFFFF000000000000UL;
}
