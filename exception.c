#include "exception.h"
#include "kprintf.h"

static const char *vector_names[] = {
    "SP0 Sync",
    "SP0 IRQ",
    "SP0 FIQ",
    "SP0 SError",
    "SPx Sync",
    "SPx IRQ",
    "SPx FIQ",
    "SPx SError",
    "Lower64 Sync",
    "Lower64 IRQ",
    "Lower64 FIQ",
    "Lower64 SError",
    "Lower32 Sync",
    "Lower32 IRQ",
    "Lower32 FIQ",
    "Lower32 SError",
};

void panic(unsigned long vector, unsigned long elr, unsigned long esr) {
	const char *name = vector < 16 ? vector_names[vector] : "Unknown";
	kpanic("\n*** PANIC: %s (vector %lu) ***\n"
	       "  ELR_EL1: 0x%lx\n"
	       "  ESR_EL1: 0x%lx (EC=0x%x)\n",
	       name,
	       vector,
	       elr,
	       esr,
	       ESR_EC(esr));
}

static inline unsigned long read_esr_el1(void) {
	unsigned long esr;
	__asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
	return esr;
}

static inline unsigned long read_far_el1(void) {
	unsigned long far;
	__asm__ volatile("mrs %0, far_el1" : "=r"(far));
	return far;
}

void sync_exception_handler(struct trap_frame *tf) {
	unsigned long esr = read_esr_el1();
	unsigned int ec = ESR_EC(esr);
	unsigned int iss = ESR_ISS(esr);

	switch (ec) {
	case EC_UNKNOWN:
		kprintf("UNDEFINED INSTRUCTION at 0x%lx\n", tf->elr);
		tf->elr += 4;
		break;

	case EC_SVC_AARCH64:
		kprintf("SVC #%d at 0x%lx\n", iss & 0xFFFF, tf->elr);
		break;

	case EC_IABT_SAME:
		kpanic("INSTRUCTION ABORT at 0x%lx, FAR=0x%lx\n",
		       tf->elr,
		       read_far_el1());

	case EC_DABT_SAME:
		kpanic("DATA ABORT at 0x%lx, FAR=0x%lx\n",
		       tf->elr,
		       read_far_el1());

	default:
		kpanic("UNHANDLED EXCEPTION: EC=0x%x, ISS=0x%x, ELR=0x%lx\n",
		       ec,
		       iss,
		       tf->elr);
	}
}

void irq_handler(struct trap_frame *tf) {
	(void)tf;
	kprintf("IRQ received\n");
}
