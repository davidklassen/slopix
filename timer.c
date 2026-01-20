#include "timer.h"
#include "gic.h"
#include "cpu.h"
#include "proc.h"

#define CNTP_CTL_ENABLE (1 << 0)

static unsigned long timer_period;
static volatile unsigned long ticks;

static inline void write_cntp_tval_el0(unsigned long v) {
	__asm__ volatile("msr cntp_tval_el0, %0" : : "r"(v));
}

static inline void write_cntp_ctl_el0(unsigned long v) {
	__asm__ volatile("msr cntp_ctl_el0, %0" : : "r"(v));
}

void timer_init(void) {
	unsigned long freq = read_cntfrq_el0();

	// Calculate period for 10ms interval (100 Hz)
	timer_period = freq / 100;
	ticks = 0;

	// Enable timer interrupt in GIC
	gic_enable_irq(TIMER_IRQ);

	// Set countdown value and enable timer
	write_cntp_tval_el0(timer_period);
	write_cntp_ctl_el0(CNTP_CTL_ENABLE);

	isb();
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
