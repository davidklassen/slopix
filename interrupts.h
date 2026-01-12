#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);

// Exception handlers called from assembly
void handle_sync_exception(void);  // Old handler (no longer called, kept for reference)
void *handle_sync_exception_with_context(void *stack_ptr);
void handle_fiq(void);
void handle_serror(void);

// IRQ test support
int irq_get_timer_flag(void);
void irq_reset_timer_flag(void);
uint32_t irq_get_timer_count(void);
void irq_reset_timer_count(void);

#endif
