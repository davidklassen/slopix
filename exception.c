#include "exception.h"
#include "cpu.h"
#include "gic.h"
#include "timer.h"
#include "kprintf.h"
#include "syscall.h"

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

const char *fault_status_string(unsigned int fsc) {
	switch (fsc & FSC_MASK) {
	case FSC_ADDR_L0:
		return "Address size fault, L0";
	case FSC_ADDR_L1:
		return "Address size fault, L1";
	case FSC_ADDR_L2:
		return "Address size fault, L2";
	case FSC_ADDR_L3:
		return "Address size fault, L3";
	case FSC_TRANS_L0:
		return "Translation fault, L0";
	case FSC_TRANS_L1:
		return "Translation fault, L1";
	case FSC_TRANS_L2:
		return "Translation fault, L2";
	case FSC_TRANS_L3:
		return "Translation fault, L3";
	case FSC_ACCESS_L1:
		return "Access flag fault, L1";
	case FSC_ACCESS_L2:
		return "Access flag fault, L2";
	case FSC_ACCESS_L3:
		return "Access flag fault, L3";
	case FSC_PERM_L1:
		return "Permission fault, L1";
	case FSC_PERM_L2:
		return "Permission fault, L2";
	case FSC_PERM_L3:
		return "Permission fault, L3";
	case FSC_SYNC_EXT:
		return "Synchronous external abort";
	case FSC_ALIGN:
		return "Alignment fault";
	default:
		return "Unknown fault";
	}
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

	case EC_IABT_SAME: {
		unsigned int fsc = iss & FSC_MASK;
		kpanic("INSTRUCTION ABORT at 0x%lx\n"
		       "  FAR: 0x%lx\n"
		       "  Fault: %s (0x%x)\n",
		       tf->elr,
		       read_far_el1(),
		       fault_status_string(fsc),
		       fsc);
		break;
	}

	case EC_DABT_SAME: {
		unsigned int fsc = iss & FSC_MASK;
		const char *op = (iss & ISS_WNR) ? "write" : "read";
		kpanic("DATA ABORT (%s) at 0x%lx\n"
		       "  FAR: 0x%lx\n"
		       "  Fault: %s (0x%x)\n",
		       op,
		       tf->elr,
		       read_far_el1(),
		       fault_status_string(fsc),
		       fsc);
		break;
	}

	default:
		kpanic("UNHANDLED EXCEPTION: EC=0x%x, ISS=0x%x, ELR=0x%lx\n",
		       ec,
		       iss,
		       tf->elr);
	}
}

void irq_handler(struct trap_frame *tf) {
	(void)tf;
	unsigned int intid = gic_acknowledge_irq();

	// End interrupt before calling handler. Timer handler may yield and never
	// return, so we must signal completion to GIC before that happens.
	if (intid != GIC_SPURIOUS_INTID) {
		gic_end_irq(intid);
	}

	if (intid == TIMER_IRQ) {
		timer_handler();
	} else if (intid != GIC_SPURIOUS_INTID) {
		kprintf("Unhandled IRQ: %u\n", intid);
	}
}

void sync_exception_handler_user(struct trap_frame *tf) {
	unsigned long esr = read_esr_el1();
	unsigned int ec = ESR_EC(esr);
	unsigned int iss = ESR_ISS(esr);

	switch (ec) {
	case EC_SVC_AARCH64:
		syscall(tf);
		break;

	case EC_IABT_LOWER: {
		unsigned int fsc = iss & FSC_MASK;
		kprintf("USER INSTRUCTION ABORT at 0x%lx\n", tf->elr);
		kprintf("  FAR: 0x%lx, Fault: %s (0x%x)\n",
			read_far_el1(),
			fault_status_string(fsc),
			fsc);
		for (;;) {
			wfi();
		}
		break;
	}

	case EC_DABT_LOWER: {
		unsigned int fsc = iss & FSC_MASK;
		const char *op = (iss & ISS_WNR) ? "write" : "read";
		kprintf("USER DATA ABORT (%s) at 0x%lx\n", op, tf->elr);
		kprintf("  FAR: 0x%lx, Fault: %s (0x%x)\n",
			read_far_el1(),
			fault_status_string(fsc),
			fsc);
		for (;;) {
			wfi();
		}
		break;
	}

	default:
		kprintf("UNHANDLED USER EXCEPTION: EC=0x%x, ISS=0x%x, ELR=0x%lx\n",
			ec,
			iss,
			tf->elr);
		for (;;) {
			wfi();
		}
	}
}
