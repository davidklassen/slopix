#ifndef INTERRUPTS_H
#define INTERRUPTS_H

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);

// Exception handlers called from assembly
void handle_sync_exception(void);  // Old handler (no longer called, kept for reference)
void *handle_sync_exception_with_context(void *stack_ptr);
void handle_fiq(void);
void handle_serror(void);

#endif
