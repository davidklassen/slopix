#include "interrupts.h"
#include "gic.h"
#include "timer.h"
#include "printf.h"

#define TIMER_IRQ 30

extern void exception_vector_table(void);

void interrupts_init(void) {
    // Install exception vector table
    __asm__ volatile("msr vbar_el1, %0" :: "r"(&exception_vector_table));

    // Initialize GIC
    gic_init();

    // Enable timer interrupt in GIC
    gic_enable_interrupt(TIMER_IRQ);
}

void interrupts_enable(void) {
    __asm__ volatile("msr daifclr, #2");  // Clear IRQ mask
}

void interrupts_disable(void) {
    __asm__ volatile("msr daifset, #2");  // Set IRQ mask
}

void handle_sync_exception(void) {
    printf("[EXCEPTION] Synchronous exception\n");
    while (1);
}

void handle_irq(void) {
    // Acknowledge interrupt and get IRQ number
    unsigned int irq = gic_acknowledge_interrupt();

    if (irq == TIMER_IRQ) {
        timer_handler();
    } else {
        printf("[IRQ] Unknown IRQ: %d\n", irq);
    }

    // Signal end of interrupt
    gic_end_interrupt(irq);
}

// New IRQ handler that supports context switching
// Called from exception handler with pointer to saved context on stack
// Returns pointer to stack to restore (may be different process)
void *handle_irq_with_context(void *stack_ptr) {
    // Acknowledge interrupt and get IRQ number
    unsigned int irq = gic_acknowledge_interrupt();

    if (irq == TIMER_IRQ) {
        // Timer handler may trigger scheduler
        stack_ptr = timer_handler_with_context(stack_ptr);
    } else {
        printf("[IRQ] Unknown IRQ: %d\n", irq);
    }

    // Signal end of interrupt
    gic_end_interrupt(irq);

    return stack_ptr;
}

void handle_fiq(void) {
    printf("[EXCEPTION] FIQ\n");
    while (1);
}

void handle_serror(void) {
    printf("[EXCEPTION] SError\n");
    while (1);
}
