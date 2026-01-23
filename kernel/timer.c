#include "timer.h"
#include "gic.h"
#include "cpu.h"
#include "proc.h"
#include "kprintf.h"

#define CNTP_CTL_ENABLE (1 << 0)

static unsigned long timer_period;
static volatile unsigned long ticks;

void timer_init(void) {
	unsigned long freq = read_cntfrq_el0();

	timer_period = freq / 100;
	ticks = 0;

	gic_enable_irq(TIMER_IRQ);

	write_cntp_tval_el0(timer_period);
	write_cntp_ctl_el0(CNTP_CTL_ENABLE);

	isb();
	kprintf("timer: %lu Hz tick\n", freq / timer_period);
}

void timer_handler(void) {
	ticks++;
	write_cntp_tval_el0(timer_period);
	wakeup_timed();
	if (current != 0) {
		yield();
	}
}

unsigned long timer_get_ticks(void) {
	return ticks;
}
