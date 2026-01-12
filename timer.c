#include "timer.h"
#include "printf.h"
#include "gic.h"

static uint64_t timer_frequency = 0;
static uint64_t timer_quantum = 0;
static int timer_periodic_mode = 0;

void timer_init(void) {
    timer_frequency = read_cntfrq_el0();
    printf("[TIMER] Initialized: frequency = %lu Hz\n", timer_frequency);
}

uint64_t timer_get_frequency(void) {
    return timer_frequency;
}

uint64_t timer_get_counter(void) {
    return read_cntvct_el0();
}

void timer_enable_irq(void) {
    gic_enable_irq(TIMER_IRQ);
}

void timer_set_quantum(uint64_t ticks) {
    timer_quantum = ticks;
    timer_periodic_mode = 1;
}

void timer_reload(void) {
    if (timer_quantum > 0) {
        write_cntv_tval_el0(timer_quantum);
    }
}

void timer_stop_periodic(void) {
    timer_periodic_mode = 0;
    timer_quantum = 0;
    write_cntv_ctl_el0(0);
}

int timer_is_periodic(void) {
    return timer_periodic_mode;
}
