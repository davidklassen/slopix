#ifndef GIC_H
#define GIC_H

#include <stdint.h>

// GICv2 Base Addresses for QEMU virt machine
#define GICD_BASE 0x08000000UL  // Distributor
#define GICC_BASE 0x08010000UL  // CPU Interface

// GICD (Distributor) Register Offsets
#define GICD_CTLR_OFFSET       0x000  // Distributor Control Register
#define GICD_TYPER_OFFSET      0x004  // Interrupt Controller Type Register
#define GICD_ISENABLER_OFFSET  0x100  // Interrupt Set-Enable Registers (base)
#define GICD_ICENABLER_OFFSET  0x180  // Interrupt Clear-Enable Registers (base)
#define GICD_IPRIORITYR_OFFSET 0x400  // Interrupt Priority Registers (base)
#define GICD_ITARGETSR_OFFSET  0x800  // Interrupt Processor Targets Registers (base)
#define GICD_ICFGR_OFFSET      0xC00  // Interrupt Configuration Registers (base)

// GICC (CPU Interface) Register Offsets
#define GICC_CTLR_OFFSET  0x000  // CPU Interface Control Register
#define GICC_PMR_OFFSET   0x004  // Interrupt Priority Mask Register
#define GICC_IAR_OFFSET   0x00C  // Interrupt Acknowledge Register
#define GICC_EOIR_OFFSET  0x010  // End of Interrupt Register

// Memory-mapped register access macros
#define GICD_CTLR       (*(volatile uint32_t *)(GICD_BASE + GICD_CTLR_OFFSET))
#define GICD_TYPER      (*(volatile uint32_t *)(GICD_BASE + GICD_TYPER_OFFSET))
#define GICD_ISENABLER(n)  (*(volatile uint32_t *)(GICD_BASE + GICD_ISENABLER_OFFSET + (n) * 4))
#define GICD_ICENABLER(n)  (*(volatile uint32_t *)(GICD_BASE + GICD_ICENABLER_OFFSET + (n) * 4))
#define GICD_IPRIORITYR(n) (*(volatile uint32_t *)(GICD_BASE + GICD_IPRIORITYR_OFFSET + (n) * 4))
#define GICD_ITARGETSR(n)  (*(volatile uint32_t *)(GICD_BASE + GICD_ITARGETSR_OFFSET + (n) * 4))
#define GICD_ICFGR(n)      (*(volatile uint32_t *)(GICD_BASE + GICD_ICFGR_OFFSET + (n) * 4))

#define GICC_CTLR  (*(volatile uint32_t *)(GICC_BASE + GICC_CTLR_OFFSET))
#define GICC_PMR   (*(volatile uint32_t *)(GICC_BASE + GICC_PMR_OFFSET))
#define GICC_IAR   (*(volatile uint32_t *)(GICC_BASE + GICC_IAR_OFFSET))
#define GICC_EOIR  (*(volatile uint32_t *)(GICC_BASE + GICC_EOIR_OFFSET))

// GIC configuration constants
#define GIC_PRIORITY_DEFAULT  0xA0
#define GIC_TARGET_CPU0       0x01
#define GIC_PRIORITY_MASK_ALL 0xFF

// Spurious interrupt ID
#define GIC_SPURIOUS_IRQ 1023

// Function declarations
void gic_init(void);
void gic_enable_irq(uint32_t irq);
uint32_t gic_is_irq_enabled(uint32_t irq);
uint32_t gic_acknowledge(void);
void gic_end_interrupt(uint32_t irq);

#endif
