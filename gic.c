#include "gic.h"

void gic_init(void) {
    // Disable distributor
    GICD_CTLR = 0;

    // Get number of interrupt lines
    uint32_t num_irqs = ((GICD_TYPER & 0x1F) + 1) * 32;

    // Disable all interrupts
    for (uint32_t i = 0; i < num_irqs / 32; i++) {
        GICD_ICENABLER(i) = 0xFFFFFFFF;
    }

    // Set all interrupts to lowest priority
    for (uint32_t i = 0; i < num_irqs / 4; i++) {
        GICD_IPRIORITYR(i) = (GIC_PRIORITY_DEFAULT << 24) | (GIC_PRIORITY_DEFAULT << 16) |
                              (GIC_PRIORITY_DEFAULT << 8) | GIC_PRIORITY_DEFAULT;
    }

    // Set all interrupts to target CPU 0
    for (uint32_t i = 8; i < num_irqs / 4; i++) {
        GICD_ITARGETSR(i) = (GIC_TARGET_CPU0 << 24) | (GIC_TARGET_CPU0 << 16) |
                             (GIC_TARGET_CPU0 << 8) | GIC_TARGET_CPU0;
    }

    // Configure all interrupts as level-sensitive
    for (uint32_t i = 0; i < num_irqs / 16; i++) {
        GICD_ICFGR(i) = 0;
    }

    // Enable distributor
    GICD_CTLR = 1;

    // Set priority mask to allow all priorities
    GICC_PMR = GIC_PRIORITY_MASK_ALL;

    // Enable CPU interface
    GICC_CTLR = 1;
}

void gic_enable_irq(uint32_t irq) {
    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;
    GICD_ISENABLER(reg) = (1U << bit);
}

uint32_t gic_is_irq_enabled(uint32_t irq) {
    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;
    return (GICD_ISENABLER(reg) >> bit) & 1U;
}

uint32_t gic_acknowledge(void) {
    return GICC_IAR;
}

void gic_end_interrupt(uint32_t irq) {
    GICC_EOIR = irq;
}
