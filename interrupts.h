#ifndef INTERRUPTS_H
#define INTERRUPTS_H

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);

// Exception handlers called from assembly
void handle_sync_exception(void);
void handle_fiq(void);
void handle_serror(void);

#endif
