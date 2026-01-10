#ifndef GIC_H
#define GIC_H

// GIC configuration constants
#define GIC_PRIORITY_DEFAULT 0xA0
#define GIC_TARGET_CPU0 0x01
#define GIC_PRIORITY_MASK_ALL 0xFF

void gic_init(void);
void gic_enable_interrupt(unsigned int irq);
unsigned int gic_acknowledge_interrupt(void);
void gic_end_interrupt(unsigned int irq);

#endif
