#include "gic.h"
#include "cpu.h"

#define GICD_REG(off) (*(volatile unsigned int *)(GICD_VA + (off)))
#define GICC_REG(off) (*(volatile unsigned int *)(GICC_VA + (off)))

void gic_init(void) {
	GICD_REG(GICD_CTLR_OFF) = 1;
	GICC_REG(GICC_PMR_OFF) = 0xFF;
	GICC_REG(GICC_CTLR_OFF) = 1;
	isb();
}

void gic_enable_irq(unsigned int intid) {
	if (intid < 32) {
		GICD_REG(GICD_ISENABLER0_OFF) = (1u << intid);
		volatile unsigned char *prio =
		    (volatile unsigned char *)(GICD_VA + GICD_IPRIORITYR0_OFF +
					       intid);
		*prio = 0x80;
	}
	isb();
}

unsigned int gic_acknowledge_irq(void) {
	return GICC_REG(GICC_IAR_OFF) & GIC_INTID_MASK;
}

void gic_end_irq(unsigned int intid) {
	GICC_REG(GICC_EOIR_OFF) = intid;
}
