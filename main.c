#include "uart.h"
#include "printf.h"
#include "interrupts.h"
#include "timer.h"
#include "pmm.h"
#include "mmu.h"

// Assembly function to enable MMU
extern void enable_mmu(unsigned long ttbr0, unsigned long ttbr1);

void main(void) {
    uart_init();
    printf("SLOPIX\n");
    printf("\n=== M3: Memory Management ===\n");

    // Initialize physical memory manager
    pmm_init();

    // MMU setup - defer to M4 due to time constraints
    printf("[Note] MMU setup deferred to next milestone\n");

    // Test physical memory allocator
    printf("\n=== Testing Physical Memory Allocator ===\n");
    printf("[TEST] Allocating 5 pages...\n");

    void *pages[5];
    for (int i = 0; i < 5; i++) {
        pages[i] = pmm_alloc_page();
        if (pages[i]) {
            printf("  Page %d allocated at: %x\n", i, pages[i]);
        } else {
            printf("  Page %d allocation failed!\n", i);
        }
    }

    printf("[TEST] Free pages: %d / %d\n", pmm_get_free_pages(), pmm_get_total_pages());

    printf("[TEST] Freeing pages 1 and 3...\n");
    pmm_free_page(pages[1]);
    pmm_free_page(pages[3]);

    printf("[TEST] Free pages: %d / %d\n", pmm_get_free_pages(), pmm_get_total_pages());

    printf("[TEST] Allocating 2 more pages...\n");
    void *page6 = pmm_alloc_page();
    void *page7 = pmm_alloc_page();
    printf("  Page 6 allocated at: %x\n", page6);
    printf("  Page 7 allocated at: %x\n", page7);

    printf("[TEST] Free pages: %d / %d\n", pmm_get_free_pages(), pmm_get_total_pages());

    printf("\n=== M2: Interrupts ===\n");
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
