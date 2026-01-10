#ifndef KERNEL_STATE_H
#define KERNEL_STATE_H

#include <stdbool.h>

// Returns true if currently executing from higher-half addresses (TTBR1 range)
// Returns false if executing from physical/identity-mapped addresses (TTBR0 range)
bool kernel_in_higher_half(void);

#endif
