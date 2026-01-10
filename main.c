#include "uart.h"
#include "printf.h"
#include "interrupts.h"
#include "timer.h"
#include "pmm.h"
#include "process.h"
#include "scheduler.h"

// Thread functions
void thread1(void) {
    int count = 0;
    while (1) {
        printf("[Thread 1] Count: %d\n", count++);
        // Busy wait to slow down output
        for (volatile int i = 0; i < 1000000; i++);
    }
}

void thread2(void) {
    int count = 0;
    while (1) {
        printf("[Thread 2] Count: %d\n", count++);
        // Busy wait to slow down output
        for (volatile int i = 0; i < 1000000; i++);
    }
}

void main(void) {
    uart_init();
    printf("SLOPIX\n");
    printf("\n=== M3: Memory Management ===\n");

    // Initialize physical memory manager
    pmm_init();

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

    printf("\n=== M4: Processes ===\n");

    // Initialize process management
    process_init();
    scheduler_init();

    // Create two kernel threads
    process_t *proc1 = process_create(thread1, 4096);
    process_t *proc2 = process_create(thread2, 4096);

    if (!proc1 || !proc2) {
        printf("[ERROR] Failed to create threads\n");
        while (1);
    }

    // Add to scheduler
    scheduler_add(proc1);
    scheduler_add(proc2);

    printf("\n=== M2: Interrupts ===\n");
    printf("Initializing interrupts...\n");

    // Initialize interrupt system
    interrupts_init();

    // Initialize timer (100 Hz = 10ms per tick)
    timer_init(100);

    // Enable interrupts
    interrupts_enable();

    printf("Timer and scheduler started\n");
    printf("Two threads will alternate printing...\n\n");

    // Enable timer-driven scheduling
    timer_enable_scheduling();

    // Start the first thread
    scheduler_schedule();

    // Should never reach here
    while (1) {
        __asm__ volatile("wfe");
    }
}
