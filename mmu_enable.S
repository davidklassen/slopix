// vim: ft=arm64asm
// MMU enable sequence for AArch64
//
// Must be called while running at physical addresses with identity
// mapping in TTBR0, so the instruction stream continues after MMU enable.

.section .text.boot
.global mmu_enable

mmu_enable:
    // Invalidate all TLB entries
    tlbi    vmalle1
    dsb     nsh
    isb

    // Read current SCTLR_EL1
    mrs     x0, sctlr_el1

    // Enable MMU (M=1), data cache (C=1), instruction cache (I=1)
    orr     x0, x0, #(1 << 0)
    orr     x0, x0, #(1 << 2)
    orr     x0, x0, #(1 << 12)

    // Write SCTLR_EL1 to enable MMU
    msr     sctlr_el1, x0

    // Ensure the MMU is enabled before continuing
    isb

    ret
