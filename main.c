#include "uart.h"
#include "printf.h"
#include "interrupts.h"
#include "timer.h"

void main(void) {
    uart_init();
    printf("SLOPIX\n");
    printf("Initializing interrupts...\n");

    // Initialize interrupt system
    interrupts_init();

    // Initialize timer (100 Hz = 10ms per tick)
    timer_init(100);

    // Enable interrupts
    interrupts_enable();

    printf("Timer started. Waiting for interrupts...\n");

    unsigned long last_ticks = 0;

    // Main loop - print tick count every 100 ticks (1 second)
    while (1) {
        unsigned long ticks = timer_get_ticks();
        if (ticks > 0 && ticks != last_ticks && (ticks % 100) == 0) {
            printf("[Timer] %d seconds elapsed (%d ticks)\n", ticks / 100, ticks);
            last_ticks = ticks;
        }
        __asm__ volatile("wfe");
    }
}
