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

// MMU system register accessors

static inline unsigned long read_sctlr_el1(void) {
	unsigned long v;
	__asm__ volatile("mrs %0, sctlr_el1" : "=r"(v));
	return v;
}

static inline unsigned long read_tcr_el1(void) {
	unsigned long v;
	__asm__ volatile("mrs %0, tcr_el1" : "=r"(v));
	return v;
}

static inline void write_tcr_el1(unsigned long v) {
	__asm__ volatile("msr tcr_el1, %0" : : "r"(v));
}

static inline void write_mair_el1(unsigned long v) {
	__asm__ volatile("msr mair_el1, %0" : : "r"(v));
}

static inline void write_ttbr0_el1(unsigned long v) {
	__asm__ volatile("msr ttbr0_el1, %0" : : "r"(v));
}

static inline void tlbi_vmalle1(void) {
	__asm__ volatile("tlbi vmalle1");
	__asm__ volatile("dsb sy");
	__asm__ volatile("isb");
}

static inline unsigned long read_esr_el1(void) {
	unsigned long v;
	__asm__ volatile("mrs %0, esr_el1" : "=r"(v));
	return v;
}

static inline unsigned long read_far_el1(void) {
	unsigned long v;
	__asm__ volatile("mrs %0, far_el1" : "=r"(v));
	return v;
}

#define DAIF_IRQ_BIT (1 << 7)

#endif
