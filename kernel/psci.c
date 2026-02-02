#include "psci.h"

void psci_system_off(void) {
	register long x0 asm("x0") = PSCI_SYSTEM_OFF;
	asm volatile("hvc #0" ::"r"(x0));
}

void psci_system_reset(void) {
	register long x0 asm("x0") = PSCI_SYSTEM_RESET;
	asm volatile("hvc #0" ::"r"(x0));
}
