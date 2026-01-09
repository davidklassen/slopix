#ifndef TIMER_H
#define TIMER_H

void timer_init(unsigned int frequency_hz);
void timer_handler(void);
void *timer_handler_with_context(void *stack_ptr);
unsigned long timer_get_ticks(void);
void timer_enable_scheduling(void);

#endif
