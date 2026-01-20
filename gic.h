#ifndef GIC_H
#define GIC_H

#include "board.h"

// Legacy names for backward compatibility
#define GICD_PHYS GICD_PA
#define GICD_VIRT GICD_VA
#define GICC_VIRT GICC_VA

// Distributor register offsets
#define GICD_CTLR_OFF	     0x000
#define GICD_ISENABLER0_OFF  0x100
#define GICD_IPRIORITYR0_OFF 0x400

// CPU Interface register offsets
#define GICC_CTLR_OFF 0x000
#define GICC_PMR_OFF  0x004
#define GICC_IAR_OFF  0x00C
#define GICC_EOIR_OFF 0x010

// Timer interrupt (PPI, INTID 30)
#define TIMER_IRQ 30

// Interrupt ID mask (10 bits from IAR)
#define GIC_INTID_MASK 0x3FF

// Spurious interrupt
#define GIC_SPURIOUS_INTID 1023

void gic_init(void);
void gic_enable_irq(unsigned int intid);
unsigned int gic_acknowledge_irq(void);
void gic_end_irq(unsigned int intid);

#endif
