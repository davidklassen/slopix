#ifndef GIC_H
#define GIC_H

// GICv2 memory-mapped base addresses for QEMU virt
// Source: https://github.com/qemu/qemu/blob/master/hw/arm/virt.c
#define GICD_BASE 0x08000000UL
#define GICC_BASE 0x08010000UL

// Distributor registers (GICD)
#define GICD_CTLR	 (GICD_BASE + 0x000)
#define GICD_ISENABLER0	 (GICD_BASE + 0x100)
#define GICD_IPRIORITYR0 (GICD_BASE + 0x400)

// CPU Interface registers (GICC)
#define GICC_CTLR (GICC_BASE + 0x000)
#define GICC_PMR  (GICC_BASE + 0x004)
#define GICC_IAR  (GICC_BASE + 0x00C)
#define GICC_EOIR (GICC_BASE + 0x010)

// Timer interrupt (PPI, INTID 30)
#define TIMER_IRQ 30

// Spurious interrupt
#define GIC_SPURIOUS_INTID 1023

// Memory-mapped register access
#define GIC_REG(addr) (*(volatile unsigned int *)(addr))

void gic_init(void);
void gic_enable_irq(unsigned int intid);
unsigned int gic_acknowledge_irq(void);
void gic_end_irq(unsigned int intid);

#endif
