#include "gic.h"
#include "arch.h"

void gic_init(void) {
	GIC_REG(GICD_CTLR) = 1;
	GIC_REG(GICC_PMR) = 0xFF;
	GIC_REG(GICC_CTLR) = 1;
	isb();
}

void gic_enable_irq(unsigned int intid) {
	if (intid < 32) {
		GIC_REG(GICD_ISENABLER0) = (1u << intid);
		volatile unsigned char *prio =
		    (volatile unsigned char *)(GICD_IPRIORITYR0 + intid);
		*prio = 0x80;
	}
	isb();
}

unsigned int gic_acknowledge_irq(void) {
	return GIC_REG(GICC_IAR) & 0x3FF;
}

void gic_end_irq(unsigned int intid) {
	GIC_REG(GICC_EOIR) = intid;
}
