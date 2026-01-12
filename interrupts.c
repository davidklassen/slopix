#include "interrupts.h"
#include "gic.h"
#include "printf.h"
#include "uart.h"
#include "syscall.h"

extern void exception_vector_table(void);

void interrupts_init(void) {
    // Install exception vector table
    __asm__ volatile("msr vbar_el1, %0" :: "r"(&exception_vector_table));

    // Initialize GIC
    gic_init();
}

void interrupts_enable(void) {
    __asm__ volatile("msr daifclr, #2" ::: "memory");
}

void interrupts_disable(void) {
    __asm__ volatile("msr daifset, #2" ::: "memory");
}

// IRQ handler called from exception vector
void handle_irq(void) {
    uint32_t irq = gic_acknowledge();

    // Check for spurious interrupt
    if (irq == GIC_SPURIOUS_IRQ) {
        return;
    }

    printf("[IRQ] Received interrupt %u\n", irq);

    gic_end_interrupt(irq);
}


// Sync exception handler that supports full context saving
// Called from exception handler with pointer to saved context on stack
// Returns pointer to stack to restore (currently always returns same pointer)
void *handle_sync_exception_with_context(void *stack_ptr) {
    unsigned long esr, elr, far, spsr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    __asm__ volatile("mrs %0, spsr_el1" : "=r"(spsr));

    // Extract Exception Class from ESR_EL1 bits [31:26]
    unsigned int ec = (esr >> 26) & 0x3F;

    // Check for SVC instruction (EC = 0x15)
    if (ec == 0x15) {
        // SVC instruction - dispatch to syscall handler
        return syscall_handler(stack_ptr);
    }

    // Other synchronous exceptions (page fault, undefined instruction, etc.)
    printf("[EXCEPTION] Synchronous exception\n");
    printf("  ESR_EL1:  %lx (EC=%x)\n", esr, ec);
    printf("  ELR_EL1:  %lx\n", elr);
    printf("  FAR_EL1:  %lx\n", far);
    printf("  SPSR_EL1: %lx\n", spsr);

    // Halt on unhandled exceptions
    while (1);
}

// Stub handlers for FIQ and SError - these exceptions are never enabled
void handle_fiq(void) {
    printf("[EXCEPTION] FIQ\n");
    while (1);
}

void handle_serror(void) {
    printf("[EXCEPTION] SError\n");
    while (1);
}

