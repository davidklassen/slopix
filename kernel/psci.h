#ifndef PSCI_H
#define PSCI_H

#define PSCI_SYSTEM_OFF	  0x84000008
#define PSCI_SYSTEM_RESET 0x84000009

void psci_system_off(void);
void psci_system_reset(void);

#endif
