# Memory Leak in walk() Function on Partial Page Table Allocation Failure

## Severity
**High**

## File and Line Number
- **File:** kernel/vmm.c
- **Lines:** 39-44
- **Function:** `walk()`

## Description

The `walk()` function allocates intermediate page table structures (L0, L1, L2) to navigate the page table hierarchy. If allocation succeeds for some levels (e.g., L0 and L1) but fails on a later level (e.g., L2 fails via `pmm_alloc()` returning `PMM_INVALID`), the function returns 0 without freeing the already-allocated and linked tables. This causes a memory leak.

### Current Code
```c
for (int level = 0; level < 3; level++) {
    pte_t *entry = &table[indices[level]];
    if (*entry & PTE_VALID) {
        table = (pte_t *)PA_TO_VA(*entry & PTE_ADDR_MASK);
    } else {
        if (!alloc) {
            return 0;
        }
        paddr_t pa = pmm_alloc();
        if (pa == PMM_INVALID) {
            return 0;  // Allocated L0 and L1 tables are now leaked!
        }
        *entry = make_table_desc(pa);
        table = (pte_t *)PA_TO_VA(pa);
    }
}
```

### Problem

When `pmm_alloc()` fails on line 40 and the function returns on line 41:
1. The L0 and/or L1 intermediate tables that were successfully allocated in previous loop iterations are still linked into the page table hierarchy via their parent entries
2. These allocated pages are never freed because `walk()` returns early without cleanup
3. The caller (`vmm_map_page()`) receives a NULL return value and gives up, unable to clean up the partial allocation
4. Memory is wasted and unavailable for future allocation

This is particularly problematic because:
- The partially-allocated tables are not recorded anywhere the caller can access them
- Only `walk()` knows about the intermediate allocations, but it returns 0 without indication of what was partially allocated
- The kernel has no way to reclaim these lost pages except by restarting or implementing a complex page table repair function

## How to Reproduce

1. Set up a scenario where physical memory is nearly exhausted
2. Attempt to map multiple pages via `vmm_map_page()` in a scenario where:
   - The first mapping succeeds (allocates L0 table)
   - The second mapping for a different L0 region needs L1 allocation and succeeds
   - The third mapping for a different L0/L1 region attempts to allocate L2 but fails due to `pmm_alloc()` returning `PMM_INVALID`
3. Verify that the L0 and/or L1 tables allocated in steps 1-2 are now inaccessible and leak

Example (conceptual):
```c
// Allocate user page table (L0 succeeds)
pte_t *pt = vmm_create();  // L0 table allocated, accessible

// Map page in one L0 region (L1 succeeds)
vmm_map_page(pt, 0x0000000000001000, physical_page_1, 1, 0);  // L1 allocated

// Map page in another L0 region (L2 fails due to low memory)
paddr_t freed_page = pmm_alloc();
pmm_free(freed_page);  // Free the last available page

vmm_map_page(pt, 0x0080000000001000, physical_page_2, 1, 0);  // walk() allocates L0, L1, L2 fails
// Result: L0 and L1 tables from this call are now leaked

// No way to free them - they're not recorded by the caller
```

## Test Recommendations

Add an automated test that verifies memory is not lost on allocation failure:

```c
// Test: Verify no memory leak on partial page table allocation failure
void test_walk_alloc_failure_no_leak(void) {
    paddr_t initial_free = pmm_free_count();

    // Create a page table
    pte_t *pt = vmm_create();
    ASSERT_NE(pt, 0, "vmm_create should succeed");

    paddr_t after_create = pmm_free_count();
    // Should have allocated one page (L0 table)
    ASSERT_EQ(initial_free - after_create, 1, "L0 allocation consumed 1 page");

    // Allocate enough pages to map one valid page
    paddr_t pa1 = pmm_alloc();
    ASSERT_NE(pa1, PMM_INVALID, "first page alloc");
    vmm_map_page(pt, 0x0000000000001000, pa1, 1, 0);

    paddr_t after_map1 = pmm_free_count();

    // Try to map in a region that would require new L1/L2 tables
    // but fail the L2 allocation by exhausting remaining memory
    paddr_t pa2 = pmm_alloc();  // Allocate remaining pages
    // (This should fail any subsequent walk() allocations)

    int result = vmm_map_page(pt, 0x0080000000001000, pa2, 1, 0);
    // Should return -1 due to allocation failure

    paddr_t after_failed_map = pmm_free_count();
    // No additional pages should be permanently lost
    ASSERT_EQ(after_map1, after_failed_map, "no pages leaked on walk() failure");

    vmm_free(pt);
    pmm_free(pa1);
    pmm_free(pa2);
}
```

Alternatively, use the existing memory tracking:
- Check `pmm_free_count()` before and after a failed `vmm_map_page()` call
- Verify that free count doesn't decrease unexpectedly (indicating a leak)

## Fixing Recommendations

The `walk()` function needs to track allocated pages and free them if a later allocation fails. There are two viable approaches:

### Option 1: Track allocations and free on failure (Recommended)
Modify `walk()` to track intermediate allocations and free them if a later one fails:

```c
static pte_t *walk(pte_t *pagetable, unsigned long va, int alloc) {
    pte_t *table = pagetable;
    int indices[3] = {L0_INDEX(va), L1_INDEX(va), L2_INDEX(va)};
    paddr_t allocated[3] = {0, 0, 0};  // Track allocations at each level

    for (int level = 0; level < 3; level++) {
        pte_t *entry = &table[indices[level]];
        if (*entry & PTE_VALID) {
            table = (pte_t *)PA_TO_VA(*entry & PTE_ADDR_MASK);
        } else {
            if (!alloc) {
                return 0;
            }
            paddr_t pa = pmm_alloc();
            if (pa == PMM_INVALID) {
                // Free any tables allocated at earlier levels
                for (int i = 0; i < level; i++) {
                    if (allocated[i] != 0) {
                        pmm_free(allocated[i]);
                    }
                }
                return 0;
            }
            allocated[level] = pa;
            *entry = make_table_desc(pa);
            table = (pte_t *)PA_TO_VA(pa);
        }
    }

    return &table[L3_INDEX(va)];
}
```

### Option 2: Return error code and let caller clean up
Return an error indicator and modify `vmm_map_page()` to detect partial allocation and clean up:

```c
// walk() returns special error code (e.g., -1 as pte_t*)
// vmm_map_page() detects this and calls vmm_free() on the page table

// This approach requires changing walk() signature and all callers
```

**Option 1 is recommended** because:
- It keeps the cleanup logic localized to the function that knows what was allocated
- It doesn't require changing the function signature or all call sites
- It follows the principle that functions should clean up their own allocations on failure
- The `allocated[]` array has minimal overhead (3 paddr_t values on stack)
