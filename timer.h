#ifndef TIMER_H
#define TIMER_H

void timer_init(unsigned int frequency_hz);
void timer_handler(void);
unsigned long timer_get_ticks(void);

#endif
