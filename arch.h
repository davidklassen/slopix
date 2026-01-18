#ifndef ARCH_H
#define ARCH_H

static inline void wfi(void) {
	__asm__ volatile("wfi");
}

static inline void isb(void) {
	__asm__ volatile("isb");
}

static inline void enable_irq(void) {
	__asm__ volatile("msr daifclr, #2");
}

static inline void disable_irq(void) {
	__asm__ volatile("msr daifset, #2");
}

static inline unsigned long read_daif(void) {
	unsigned long daif;
	__asm__ volatile("mrs %0, daif" : "=r"(daif));
	return daif;
}

static inline unsigned long read_cntfrq_el0(void) {
	unsigned long v;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
	return v;
}

#define DAIF_IRQ_BIT (1 << 7)

#endif
