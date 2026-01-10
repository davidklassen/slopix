#include "gic.h"

// GICv2 addresses for QEMU virt machine
#define GICD_BASE 0x08000000UL  // Distributor
#define GICC_BASE 0x08010000UL  // CPU Interface

// Distributor registers
#define GICD_CTLR       (*(volatile unsigned int *)(unsigned long)(GICD_BASE + 0x000))
#define GICD_TYPER      (*(volatile unsigned int *)(unsigned long)(GICD_BASE + 0x004))
#define GICD_ISENABLER(n) (*(volatile unsigned int *)(unsigned long)(GICD_BASE + 0x100 + (n) * 4))
#define GICD_ICENABLER(n) (*(volatile unsigned int *)(unsigned long)(GICD_BASE + 0x180 + (n) * 4))
#define GICD_IPRIORITYR(n) (*(volatile unsigned int *)(unsigned long)(GICD_BASE + 0x400 + (n) * 4))
#define GICD_ITARGETSR(n) (*(volatile unsigned int *)(unsigned long)(GICD_BASE + 0x800 + (n) * 4))
#define GICD_ICFGR(n) (*(volatile unsigned int *)(unsigned long)(GICD_BASE + 0xC00 + (n) * 4))

// CPU Interface registers
#define GICC_CTLR       (*(volatile unsigned int *)(unsigned long)(GICC_BASE + 0x000))
#define GICC_PMR        (*(volatile unsigned int *)(unsigned long)(GICC_BASE + 0x004))
#define GICC_IAR        (*(volatile unsigned int *)(unsigned long)(GICC_BASE + 0x00C))
#define GICC_EOIR       (*(volatile unsigned int *)(unsigned long)(GICC_BASE + 0x010))

void gic_init(void) {
    // Disable distributor
    GICD_CTLR = 0;

    // Get number of interrupt lines
    unsigned int num_irqs = ((GICD_TYPER & 0x1F) + 1) * 32;

    // Disable all interrupts
    for (unsigned int i = 0; i < num_irqs / 32; i++) {
        GICD_ICENABLER(i) = 0xFFFFFFFF;
    }

    // Set all interrupts to lowest priority
    for (unsigned int i = 0; i < num_irqs / 4; i++) {
        GICD_IPRIORITYR(i) = (GIC_PRIORITY_DEFAULT << 24) | (GIC_PRIORITY_DEFAULT << 16) |
                              (GIC_PRIORITY_DEFAULT << 8) | GIC_PRIORITY_DEFAULT;
    }

    // Set all interrupts to target CPU 0
    for (unsigned int i = 8; i < num_irqs / 4; i++) {
        GICD_ITARGETSR(i) = (GIC_TARGET_CPU0 << 24) | (GIC_TARGET_CPU0 << 16) |
                             (GIC_TARGET_CPU0 << 8) | GIC_TARGET_CPU0;
    }

    // Configure all interrupts as level-sensitive
    for (unsigned int i = 0; i < num_irqs / 16; i++) {
        GICD_ICFGR(i) = 0;
    }

    // Enable distributor
    GICD_CTLR = 1;

    // Set priority mask to allow all priorities
    GICC_PMR = GIC_PRIORITY_MASK_ALL;

    // Enable CPU interface
    GICC_CTLR = 1;
}

void gic_enable_interrupt(unsigned int irq) {
    unsigned int reg = irq / 32;
    unsigned int bit = irq % 32;
    GICD_ISENABLER(reg) = (1 << bit);
}

unsigned int gic_acknowledge_interrupt(void) {
    return GICC_IAR & 0x3FF;  // Get interrupt ID
}

void gic_end_interrupt(unsigned int irq) {
    GICC_EOIR = irq;
}
