#ifndef TIMER_H
#define TIMER_H

// Timer configuration
#define TIMER_FREQUENCY_HZ 100
#define SCHEDULER_TICK_INTERVAL 10

void timer_init(unsigned int frequency_hz);
void *timer_handler_with_context(void *stack_ptr);
unsigned long timer_get_ticks(void);
void timer_enable_scheduling(void);

#endif
