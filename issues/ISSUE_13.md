# Issue 13: Quadratic Time Complexity in pmm_alloc_contiguous()

## Severity
High

## File and Location
**File:** `/Users/davidklassen/work/davidklassen/slopix/kernel/pmm.c`
**Lines:** 91-120 (function `pmm_alloc_contiguous()`)

## Description

The `pmm_alloc_contiguous()` function has O(n*m) time complexity, where:
- **n** = number of contiguous pages requested
- **m** = number of free pages in the free list

For each candidate page in the free list (line 100-117), the algorithm:
1. Checks if the next (n-1) pages are free by calling `is_page_free()` for each (lines 103-107)
2. Each `is_page_free()` call walks the entire free list (lines 68-76), which is O(m)
3. If n consecutive pages are found, removes them by calling `remove_page()` n times (lines 110-113)
4. Each `remove_page()` call walks the entire free list again (lines 79-88), which is O(m)

This creates nested loops: outer loop iterates up to m times (checking each candidate), and inner operations are O(n*m).

**Actual complexity breakdown:**
- Checking contiguity: O(m) outer iterations × O(n) calls to `is_page_free()` × O(m) per call = **O(n*m²)**
- Removing pages: O(n) removals × O(m) per `remove_page()` = **O(n*m)**

For small allocations this is acceptable, but allocating dozens or hundreds of contiguous pages with many free pages in the list becomes problematic.

## How to Reproduce / Measure

1. Create a test that allocates multiple large contiguous blocks:
   ```c
   // Allocate 100 contiguous pages, then 100 more
   paddr_t p1 = pmm_alloc_contiguous(100);  // Searches O(free_count * 100 * free_count)
   paddr_t p2 = pmm_alloc_contiguous(100);  // Similar search
   ```

2. Measure execution time with a large free list:
   - Initialize kernel, leaving most memory free
   - Call `pmm_alloc_contiguous(N)` for varying N
   - Time increases quadratically with N and free page count

3. Use performance profiling to confirm time spent in `is_page_free()` and `remove_page()`

## Test Recommendations

1. **Functional correctness test:** Verify allocations still return contiguous pages and don't overlap
2. **Performance regression test:** Add timing measurements around `pmm_alloc_contiguous()` calls in the test suite
3. **Edge cases:**
   - Allocating when only one contiguous block exists matching the size
   - Allocating when many small fragmented blocks exist but no single contiguous block
   - Allocating 1 page (already optimized to call `pmm_alloc()`)

## Fixing Recommendations

**Option 1: Maintain a sorted index (Recommended)**
- Keep free pages sorted by physical address
- Use linear scan (still O(m)) but eliminate `is_page_free()` redundant walks
- When looking for n contiguous pages, scan once: if current + next (n-1) are sequential, allocate
- This reduces worst-case from O(n*m²) to O(n*m)

**Option 2: Use a binary search tree or balanced tree structure**
- Store free pages in a tree indexed by address
- Lookup if a page is free: O(log m) instead of O(m)
- Finding contiguous blocks: O(n*log m) instead of O(n*m)
- Trade-off: More complex bookkeeping

**Option 3: Bitmap-based free page tracking**
- Maintain a bitmap of free pages
- Lookup/remove operations: O(1)
- Contiguity check: scan bitmap for n consecutive 1-bits
- Requires memory proportional to total pages

**Recommended approach:** Option 1 (sorted index) offers good balance:
- Minimal complexity increase
- Significantly improves performance
- Maintains the simple free-page-stores-pointer design
- O(m) for allocation is acceptable for typical system configurations

## Implementation Notes

- Maintain freelist sorted by ascending physical address (already linearized, just need ordering)
- Update `pmm_free()` to insert in sorted position instead of prepending
- Update `pmm_alloc_contiguous()` to check addresses sequentially instead of calling `is_page_free()`
- `remove_page()` can remain unchanged or be optimized if needed
- No API changes needed; purely internal optimization
