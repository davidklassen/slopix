#include "timer.h"
#include "scheduler.h"
#include "printf.h"

// ARM Generic Timer interrupt number for QEMU virt
#define TIMER_IRQ 30

static volatile unsigned long tick_count = 0;
static int scheduling_enabled = 0;

// Read system counter frequency
static inline unsigned int timer_get_frequency(void) {
    unsigned int freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

// Read current counter value
static inline unsigned long timer_get_counter(void) {
    unsigned long count;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(count));
    return count;
}

// Set compare value
static inline void timer_set_compare(unsigned long value) {
    __asm__ volatile("msr cntp_cval_el0, %0" :: "r"(value));
}

// Enable timer
static inline void timer_enable(void) {
    unsigned long ctrl = 1;  // Enable bit
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(ctrl));
}

// Disable timer
static inline void timer_disable(void) {
    unsigned long ctrl = 0;
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(ctrl));
}

void timer_init(unsigned int frequency_hz) {
    // Get system counter frequency (usually 62.5 MHz on QEMU)
    unsigned int sys_freq = timer_get_frequency();

    // Calculate ticks per interrupt
    unsigned int ticks_per_interrupt = sys_freq / frequency_hz;

    // Disable timer
    timer_disable();

    // Set initial compare value
    unsigned long current = timer_get_counter();
    timer_set_compare(current + ticks_per_interrupt);

    // Enable timer
    timer_enable();

    tick_count = 0;
}

// Timer handler that supports context switching
void *timer_handler_with_context(void *stack_ptr) {
    tick_count++;

    // Get system counter frequency
    unsigned int sys_freq = timer_get_frequency();

    // Schedule next interrupt
    unsigned int ticks_per_interrupt = sys_freq / TIMER_FREQUENCY_HZ;
    unsigned long current = timer_get_counter();
    timer_set_compare(current + ticks_per_interrupt);

    // Trigger scheduler every N ticks
    if (scheduling_enabled && (tick_count % SCHEDULER_TICK_INTERVAL) == 0) {
        stack_ptr = scheduler_schedule_with_context(stack_ptr);
    }

    return stack_ptr;
}

unsigned long timer_get_ticks(void) {
    return tick_count;
}

void timer_enable_scheduling(void) {
    scheduling_enabled = 1;
}
