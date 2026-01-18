#ifndef EXCEPTION_H
#define EXCEPTION_H

#define ESR_EC_SHIFT 26
#define ESR_EC(esr)  (((esr) >> ESR_EC_SHIFT) & 0x3F)
#define ESR_ISS(esr) ((esr) & 0x1FFFFFF)

#define EC_UNKNOWN     0x00
#define EC_SVC_AARCH64 0x15
#define EC_IABT_SAME   0x21
#define EC_DABT_SAME   0x25

struct trap_frame {
	unsigned long regs[31];
	unsigned long sp_el0;
	unsigned long elr;
	unsigned long spsr;
};

#endif
