#ifndef GIC_H
#define GIC_H

void gic_init(void);
void gic_enable_interrupt(unsigned int irq);
unsigned int gic_acknowledge_interrupt(void);
void gic_end_interrupt(unsigned int irq);

#endif
