#ifndef CPU_H
#define CPU_H

static inline void nop(void) {
	asm volatile("nop");
}

static inline void wfi(void) {
	asm volatile("wfi");
}

static inline void wfe(void) {
	asm volatile("wfe");
}

static inline void isb(void) {
	asm volatile("isb");
}

static inline void dsb(void) {
	asm volatile("dsb sy");
}

static inline void enable_irq(void) {
	asm volatile("msr daifclr, #2");
}

static inline void disable_irq(void) {
	asm volatile("msr daifset, #2");
}

static inline unsigned long read_daif(void) {
	unsigned long daif;
	asm volatile("mrs %0, daif" : "=r"(daif));
	return daif;
}

static inline unsigned long read_cntfrq_el0(void) {
	unsigned long v;
	asm volatile("mrs %0, cntfrq_el0" : "=r"(v));
	return v;
}

static inline void write_cntp_tval_el0(unsigned long v) {
	asm volatile("msr cntp_tval_el0, %0" : : "r"(v));
}

static inline void write_cntp_ctl_el0(unsigned long v) {
	asm volatile("msr cntp_ctl_el0, %0" : : "r"(v));
}

// MMU system register accessors

static inline unsigned long read_sctlr_el1(void) {
	unsigned long v;
	asm volatile("mrs %0, sctlr_el1" : "=r"(v));
	return v;
}

static inline unsigned long read_tcr_el1(void) {
	unsigned long v;
	asm volatile("mrs %0, tcr_el1" : "=r"(v));
	return v;
}

static inline unsigned long read_ttbr0_el1(void) {
	unsigned long v;
	asm volatile("mrs %0, ttbr0_el1" : "=r"(v));
	return v;
}

static inline unsigned long read_ttbr1_el1(void) {
	unsigned long v;
	asm volatile("mrs %0, ttbr1_el1" : "=r"(v));
	return v;
}

static inline void write_ttbr0_el1(unsigned long v) {
	asm volatile("msr ttbr0_el1, %0" : : "r"(v));
}

static inline void tlbi_vmalle1(void) {
	asm volatile("tlbi vmalle1");
	dsb();
	isb();
}

static inline void tlbi_va(unsigned long va) {
	dsb();
	asm volatile("tlbi vaae1is, %0" : : "r"(va >> 12));
	dsb();
	isb();
}

static inline unsigned long read_pc(void) {
	unsigned long pc;
	asm volatile("adr %0, ." : "=r"(pc));
	return pc;
}

static inline unsigned long read_esr_el1(void) {
	unsigned long v;
	asm volatile("mrs %0, esr_el1" : "=r"(v));
	return v;
}

static inline unsigned long read_far_el1(void) {
	unsigned long v;
	asm volatile("mrs %0, far_el1" : "=r"(v));
	return v;
}

#define DAIF_IRQ_BIT (1 << 7)

// Save IRQ state and disable IRQs. Returns previous DAIF value.
// Call irq_restore() with the returned value to restore state.
static inline unsigned long irq_save(void) {
	unsigned long daif = read_daif();
	disable_irq();
	return daif;
}

// Restore IRQ state from a previous irq_save() call.
// Only re-enables IRQs if they were enabled before irq_save().
static inline void irq_restore(unsigned long daif) {
	if (!(daif & DAIF_IRQ_BIT)) {
		enable_irq();
	}
}

#endif
