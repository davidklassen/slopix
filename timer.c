#include "timer.h"
#include "printf.h"

static uint64_t timer_frequency = 0;

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
