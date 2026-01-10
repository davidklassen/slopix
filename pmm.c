#include "pmm.h"
#include "memory.h"
#include "printf.h"

// External symbols from linker script
extern char __kernel_end;

// Bitmap for tracking free pages
// Note: page_bitmap is volatile to prevent compiler from optimizing away writes
// (similar to WRITE_ONCE in mmu.c - bitmap is live memory that must be explicitly initialized)
static volatile unsigned char *page_bitmap;
static unsigned long total_pages;
static unsigned long free_pages;
static unsigned long bitmap_size;

static void set_page_used(unsigned long page_num) {
    unsigned long byte = page_num / 8;
    unsigned long bit = page_num % 8;
    page_bitmap[byte] |= (1 << bit);
}

static void set_page_free(unsigned long page_num) {
    unsigned long byte = page_num / 8;
    unsigned long bit = page_num % 8;
    page_bitmap[byte] &= ~(1 << bit);
}

static int is_page_free(unsigned long page_num) {
    unsigned long byte = page_num / 8;
    unsigned long bit = page_num % 8;
    return !(page_bitmap[byte] & (1 << bit));
}

void pmm_init(void) {
    printf("[PMM] Starting initialization...\n");

    // Calculate total pages in physical memory
    total_pages = PHYS_MEMORY_SIZE / PAGE_SIZE;

    // Bitmap needs 1 bit per page
    bitmap_size = (total_pages + 7) / 8;

    // Place bitmap right after kernel
    unsigned long kernel_end = (unsigned long)&__kernel_end;
    kernel_end = (kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);  // Align to page
    page_bitmap = (volatile unsigned char *)kernel_end;

    // Initialize bitmap - mark all pages as free
    for (unsigned long i = 0; i < bitmap_size; i++) {
        page_bitmap[i] = 0;
    }

    // Mark kernel pages and bitmap pages as used
    unsigned long kernel_pages = ((kernel_end + bitmap_size) - PHYS_MEMORY_START) / PAGE_SIZE;
    for (unsigned long i = 0; i < kernel_pages; i++) {
        set_page_used(i);
    }

    free_pages = total_pages - kernel_pages;

    printf("[PMM] Initialized: %d MB (%d pages, %d free)\n",
           PHYS_MEMORY_SIZE / (1024 * 1024), total_pages, free_pages);
    printf("[PMM] Kernel end: %x, Bitmap at: %x (%d bytes)\n",
           kernel_end, page_bitmap, bitmap_size);
}

void *pmm_alloc_page(void) {
    for (unsigned long i = 0; i < total_pages; i++) {
        if (is_page_free(i)) {
            set_page_used(i);
            free_pages--;
            unsigned long phys_addr = PHYS_MEMORY_START + (i * PAGE_SIZE);

            // Pages returned uninitialized - caller must zero if needed
            return (void *)phys_addr;
        }
    }
    return 0;  // Out of memory
}

void pmm_free_page(void *page) {
    unsigned long phys_addr = (unsigned long)page;

    if (phys_addr < PHYS_MEMORY_START || phys_addr >= PHYS_MEMORY_END) {
        printf("[PMM] Error: Invalid page address %x\n", phys_addr);
        return;
    }

    unsigned long page_num = (phys_addr - PHYS_MEMORY_START) / PAGE_SIZE;

    if (!is_page_free(page_num)) {
        set_page_free(page_num);
        free_pages++;
    }
}

unsigned long pmm_get_free_pages(void) {
    return free_pages;
}

unsigned long pmm_get_total_pages(void) {
    return total_pages;
}
