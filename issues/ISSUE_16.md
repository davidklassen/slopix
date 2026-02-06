# Double-Free Vulnerability in pmm_free()

## Title
Missing double-free detection allows page freelist corruption

## Severity
**Critical**

Exploitable memory corruption in physical memory allocator; a single double-free corrupts the freelist and causes system instability.

## Location
**File:** `/Users/davidklassen/work/davidklassen/slopix/kernel/pmm.c`
**Lines:** 122-136

## Description

The `pmm_free()` function has no protection against double-frees. If a page is freed twice, it gets added to the freelist twice, creating a memory corruption vulnerability that silently corrupts the freelist data structure.

### Current Code
```c
void pmm_free(paddr_t pa) {
	if (pa < RAM_BASE || pa >= RAM_BASE + RAM_SIZE) {
		return;
	}
	if (!IS_PAGE_ALIGNED(pa)) {
		return;
	}

	zero_page(pa);

	struct run *r = (struct run *)PA_TO_VA(pa);
	r->next = freelist;
	freelist = r;
	free_count++;
}
```

### The Problem

The function unconditionally prepends the page to the freelist without checking if it's already on the list. An existing helper function `is_page_free()` (lines 68-77) exists in the same file but is never called by `pmm_free()`. This is a clear sign of incomplete implementation.

### Attack Scenario

1. **First allocation:** `pa1 = pmm_alloc()` - obtains page at address A
2. **First free:** `pmm_free(pa1)` - adds page A to freelist
3. **Second free (bug):** `pmm_free(pa1)` - adds page A to freelist AGAIN
4. **Freelist corruption:** Page A now appears twice in the linked list
5. **Subsequent allocations:** Will return corrupted pointers or allocate the same physical page twice
6. **System behavior:** Memory allocations interfere with each other, caches conflict, data corruption

### Why This Is Critical

- **Silent corruption:** No error, no crash on the buggy free call - corruption is invisible until allocation reuses corrupted list
- **Cascading failures:** One double-free corrupts all future allocations from the same list
- **Kernel privilege:** Running in kernel mode, attacker (or buggy code) can corrupt any kernel memory
- **No safe recovery:** Once freelist is corrupted, there is no way to detect or repair it

## How to Reproduce

```c
// In any code calling pmm functions
paddr_t pa = pmm_alloc();           // Allocate page
pmm_free(pa);                       // Free it once
pmm_free(pa);                       // Free it again (BUG - should be detected)

paddr_t pa2 = pmm_alloc();          // May return same page or corrupted address
paddr_t pa3 = pmm_alloc();          // May also return page pa

// pa, pa2, and pa3 now all point to same physical memory
// Writing to one corrupts the others
```

### Practical Test Case

1. Run kernel tests normally: `make test`
2. Add this to `kernel_main()` before normal operation:
   ```c
   paddr_t test_page = pmm_alloc();
   pmm_free(test_page);
   pmm_free(test_page);  // Double-free happens silently
   ```
3. Continue normal operation - system will exhibit memory corruption later
4. Observe corruption in unrelated subsystems (page table inconsistencies, vmm test failures, etc.)

## Test Recommendations

Add comprehensive double-free and freelist integrity tests to `kernel/tests/test_pmm.c`:

```c
TEST(pmm_free_double_free_detected) {
	paddr_t pa = pmm_alloc();
	ASSERT(pa != PMM_INVALID, "Should allocate page");

	unsigned long before = pmm_free_count();
	pmm_free(pa);
	unsigned long after_first = pmm_free_count();
	ASSERT_EQ(before + 1, after_first, "First free should increase count");

	pmm_free(pa);
	unsigned long after_second = pmm_free_count();
	ASSERT_EQ(after_first, after_second, "Second free should NOT increase count");
	return 0;
}

TEST(pmm_freelist_integrity_after_double_free) {
	// Allocate 4 pages, free them in various patterns
	paddr_t pages[4];
	for (int i = 0; i < 4; i++) {
		pages[i] = pmm_alloc();
		ASSERT(pages[i] != PMM_INVALID, "Should allocate");
	}

	unsigned long before = pmm_free_count();

	// Free page 2 twice (double-free)
	pmm_free(pages[2]);
	pmm_free(pages[2]);

	// Free other pages normally
	pmm_free(pages[0]);
	pmm_free(pages[1]);
	pmm_free(pages[3]);

	// Count should reflect only 4 pages freed, not 5
	ASSERT_EQ(before + 4, pmm_free_count(), "Count should be 4, not 5");
	return 0;
}

TEST(pmm_no_page_duplicate_on_freelist) {
	paddr_t pa = pmm_alloc();
	pmm_free(pa);
	pmm_free(pa);  // Double-free

	// Verify page appears only once on freelist by checking allocations
	// After double-free, we should NOT be able to allocate the same page twice
	paddr_t pa2 = pmm_alloc();
	paddr_t pa3 = pmm_alloc();

	// Both pa2 and pa3 should NOT be the same physical address
	ASSERT(pa2 == PMM_INVALID || pa3 == PMM_INVALID || pa2 != pa3,
	       "Should not allocate duplicate pages from corrupted freelist");
	return 0;
}
```

## Fixing Recommendation

Call `is_page_free()` before adding a page to the freelist. If the page is already free, return early without adding it again:

### Change to pmm.c (lines 122-136)

```c
void pmm_free(paddr_t pa) {
	if (pa < RAM_BASE || pa >= RAM_BASE + RAM_SIZE) {
		return;
	}
	if (!IS_PAGE_ALIGNED(pa)) {
		return;
	}

	// Check for double-free: page should not already be on the freelist
	if (is_page_free(pa)) {
		return;  // Already free, prevent corruption from double-free
	}

	zero_page(pa);

	struct run *r = (struct run *)PA_TO_VA(pa);
	r->next = freelist;
	freelist = r;
	free_count++;
}
```

### Trade-offs

- **Added cost:** `is_page_free()` does O(n) linked-list traversal of freelist
  - For current system: typically 100-1000 free pages, negligible cost
  - Acceptable because `pmm_free()` is called during shutdown/cleanup, not fast path
- **Alternative (not recommended):** Mark pages with a "free" flag in a separate bitmap, adds memory overhead
- **Future improvement:** When multi-core support added with spinlock protection, consider lock + bitmap for O(1) double-free detection

### Minimal Risk

The fix is minimal and defensive:
- Uses existing, tested `is_page_free()` function
- Only adds one condition check
- Zero impact on correct usage (no page is freed twice in normal code)
- Prevents silent corruption in buggy or malicious code
