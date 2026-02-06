# realloc() Does Not Update Block Size When Shrinking

## Severity
High

## Location
File: `/Users/davidklassen/work/davidklassen/slopix/libc/malloc.c`
Lines: 117-118

## Description
When `realloc()` shrinks a memory block (new size < old size), the function returns the original pointer without updating the block header's `size` field. The header still contains the original (larger) size value, causing subsequent `realloc()` calls to read stale size information.

The bug occurs in this code path:
```c
if (old_size >= size) {
    return ptr;  // BUG: block->size is never updated to the new size
}
```

When the condition is true (block is kept in place), the header metadata is inconsistent with the actual allocated size.

## Impact
- Subsequent `realloc()` calls on the same pointer will use incorrect size information
- A second `realloc()` attempting to expand the block will incorrectly calculate available space
- Memory corruption can occur if the allocator relies on accurate size information for future operations
- Any code path that calls `realloc()` multiple times on the same block is affected

## How to Reproduce
```c
void *ptr = malloc(64);
// ptr points to 64 bytes, block->size = 64

void *result = realloc(ptr, 32);
// result == ptr (returned unchanged)
// BUT: block->size still = 64 (stale!)

void *result2 = realloc(result, 48);
// Function reads block->size as 64 (old value)
// Incorrectly thinks it has 64 bytes available
// Returns ptr instead of allocating new memory (wrong behavior)
```

## Test Recommendations
Add an automated test to verify correct behavior when multiple `realloc()` calls occur:

1. **Test case: Multiple realloc calls with shrink then expand**
   - Allocate 64 bytes
   - Shrink to 32 bytes via `realloc()` (verify returns same pointer)
   - Expand to 48 bytes via `realloc()` (verify new allocation or correct behavior)
   - Verify block->size is correctly updated at each step

2. **Test case: Size field consistency**
   - Allocate block of known size
   - Call `realloc()` with different sizes (both shrink and grow)
   - Verify block header size field matches the current allocation size

## Fixing Recommendations
Update lines 117-118 to update the block size even when the block remains in place:

```c
if (old_size >= size) {
    block->size = size;  // Update header with new size
    return ptr;
}
```

This ensures the block header always reflects the current allocation size, regardless of whether the pointer changes.
