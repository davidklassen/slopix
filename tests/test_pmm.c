#include "test_framework.h"
#include "../pmm.h"
#include "../printf.h"

void test_pmm_allocate_100_pages(void) {
    TEST("Allocate 100 pages - all should succeed with unique addresses");

    void *pages[100];
    int all_succeeded = 1;
    int all_unique = 1;

    // Allocate 100 pages
    for (int i = 0; i < 100; i++) {
        pages[i] = pmm_alloc_page();
        if (!pages[i]) {
            all_succeeded = 0;
            break;
        }
    }

    ASSERT(all_succeeded, "All 100 page allocations succeeded");

    // Check uniqueness
    for (int i = 0; i < 100 && all_succeeded; i++) {
        for (int j = i + 1; j < 100; j++) {
            if (pages[i] == pages[j]) {
                all_unique = 0;
                break;
            }
        }
        if (!all_unique) break;
    }

    ASSERT(all_unique, "All 100 pages have unique addresses");

    // Clean up - free all pages
    for (int i = 0; i < 100 && pages[i]; i++) {
        pmm_free_page(pages[i]);
    }
}

void test_pmm_free_and_reallocate(void) {
    TEST("Free 100 pages and reallocate - should succeed");

    void *pages[100];

    // Allocate 100 pages
    for (int i = 0; i < 100; i++) {
        pages[i] = pmm_alloc_page();
    }

    // Free all
    for (int i = 0; i < 100; i++) {
        pmm_free_page(pages[i]);
    }

    // Reallocate 100 pages
    int all_succeeded = 1;
    for (int i = 0; i < 100; i++) {
        void *page = pmm_alloc_page();
        if (!page) {
            all_succeeded = 0;
            break;
        }
        pages[i] = page;
    }

    ASSERT(all_succeeded, "Reallocated 100 pages after freeing");

    // Clean up
    for (int i = 0; i < 100; i++) {
        pmm_free_page(pages[i]);
    }
}

void test_pmm_pattern_alloc_free(void) {
    TEST("Pattern: alloc 10, free 5, alloc 5 - should succeed");

    void *pages1[10];
    void *pages2[5];

    // Allocate 10 pages
    int success = 1;
    for (int i = 0; i < 10; i++) {
        pages1[i] = pmm_alloc_page();
        if (!pages1[i]) {
            success = 0;
            break;
        }
    }
    ASSERT(success, "Allocated initial 10 pages");

    // Free middle 5 pages (indices 2-6)
    for (int i = 2; i < 7; i++) {
        pmm_free_page(pages1[i]);
    }

    // Allocate 5 more pages - should get the freed ones
    success = 1;
    for (int i = 0; i < 5; i++) {
        pages2[i] = pmm_alloc_page();
        if (!pages2[i]) {
            success = 0;
            break;
        }
    }
    ASSERT(success, "Reallocated 5 pages after freeing 5");

    // Check that at least some of the new pages match the freed addresses
    int found_reused = 0;
    for (int i = 0; i < 5; i++) {
        for (int j = 2; j < 7; j++) {
            if (pages2[i] == pages1[j]) {
                found_reused++;
                break;
            }
        }
    }
    ASSERT(found_reused > 0, "At least one freed page was reused");

    // Clean up
    for (int i = 0; i < 10; i++) {
        if (i < 2 || i >= 7) {  // Skip the ones we freed
            pmm_free_page(pages1[i]);
        }
    }
    for (int i = 0; i < 5; i++) {
        pmm_free_page(pages2[i]);
    }
}

void test_pmm_page_alignment(void) {
    TEST("Allocated pages are 4KB aligned");

    void *pages[10];
    int all_aligned = 1;

    for (int i = 0; i < 10; i++) {
        pages[i] = pmm_alloc_page();
        unsigned long addr = (unsigned long)pages[i];
        if ((addr & 0xFFF) != 0) {  // Check 4KB alignment (lower 12 bits should be 0)
            all_aligned = 0;
            printf("  [ERROR] Page %d at 0x%lx is not 4KB aligned\n", i, addr);
        }
    }

    ASSERT(all_aligned, "All allocated pages are 4KB aligned");

    // Clean up
    for (int i = 0; i < 10; i++) {
        pmm_free_page(pages[i]);
    }
}

void run_pmm_tests(void) {
    TEST_SUITE("Physical Memory Manager (PMM)");

    test_pmm_allocate_100_pages();
    test_pmm_free_and_reallocate();
    test_pmm_pattern_alloc_free();
    test_pmm_page_alignment();
}
