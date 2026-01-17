#include "psci.h"

void psci_system_off(void) {
	register long x0 asm("x0") = PSCI_SYSTEM_OFF;
	asm volatile("hvc #0" ::"r"(x0));
}
