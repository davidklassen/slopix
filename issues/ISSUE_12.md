# ISSUE_12: vmm_copyinstr() Calls walk() for Every Byte

## Severity
**High**

## File and Line
File: `/Users/davidklassen/work/davidklassen/slopix/kernel/vmm.c`
Line: 274

## Description
The `vmm_copyinstr()` function performs a redundant 3-level page table traversal (`walk()`) for every single byte being copied, even though most bytes reside on the same page. The function already correctly detects page boundary crossings (lines 265-271) but fails to cache the physical page address, instead re-walking the entire page table hierarchy for each byte.

### Impact
- **Performance overhead**: ~4096 unnecessary page table walks per 4KB page
- **CPU cycles wasted**: Each `walk()` call traverses 3 levels of page table indirection (L0 → L1 → L2 → L3)
- **Affects system calls**: `copyinstr()` is a core mechanism for reading user-space strings (syscall arguments, filenames, etc.)

### Root Cause
The code structure (lines 261-282) is:
1. Detects page boundaries and validates new pages
2. Calls `walk()` for **every** byte (line 274)
3. Extracts physical address and offset from the PTE (line 275)
4. Reads the byte (line 276)

The physical page address (`*pte & PTE_ADDR_MASK`) doesn't change within a page, so repeatedly calling `walk()` is redundant.

## How to Reproduce / Measure
1. Add instrumentation to count `walk()` calls in `vmm_copyinstr()`
2. Copy a long user string (e.g., 500+ bytes)
3. Observe `walk()` call count: should be ~number_of_bytes, but should be ~(number_of_pages)
4. Benchmark: measure time to copy a large string before and after optimization

Example scenario:
- Copying a 4KB string (4096 bytes)
- Current: 4096 calls to `walk()`
- Optimal: 1 call to `walk()`

## Test Recommendations
Add a benchmark in the test suite:
1. Create a user-space buffer with a long null-terminated string (8-16 KB)
2. Call `vmm_copyinstr()` and measure elapsed cycles (via PMU or timing instructions)
3. Verify correctness is unchanged (string is copied accurately)
4. Compare before/after optimization to confirm performance improvement

Example test structure:
```c
TEST(vmm_copyinstr_performance) {
    // Map a large user buffer and populate with a long string
    // Measure cycles for vmm_copyinstr()
    // Assert no performance regression
}
```

## Fixing Recommendations
Cache the physical page base address to avoid redundant `walk()` calls:

```c
// Safely copy a null-terminated string from user space to kernel buffer
// Returns string length on success, -1 on failure
int vmm_copyinstr(pte_t *pagetable, char *dst, unsigned long srcva, unsigned long max) {
    unsigned long i = 0;
    unsigned long cur_page = srcva & ~(PAGE_SIZE - 1);
    paddr_t cur_page_pa = 0;  // Cache physical page address

    if (validate_page(pagetable, srcva) < 0) {
        return -1;
    }

    // Get initial page physical address
    pte_t *pte = walk(pagetable, srcva, 0);
    if (pte == 0 || (*pte & PTE_VALID) == 0) {
        return -1;
    }
    cur_page_pa = *pte & PTE_ADDR_MASK;

    while (i < max - 1) {
        unsigned long addr = srcva + i;

        // Check if we crossed into a new page
        unsigned long page = addr & ~(PAGE_SIZE - 1);
        if (page != cur_page) {
            if (validate_page(pagetable, addr) < 0) {
                return -1;
            }
            // Walk page table only on page boundary
            pte = walk(pagetable, addr, 0);
            if (pte == 0 || (*pte & PTE_VALID) == 0) {
                return -1;
            }
            cur_page_pa = *pte & PTE_ADDR_MASK;
            cur_page = page;
        }

        // Use cached page address + offset (no walk() call)
        paddr_t pa = cur_page_pa + (addr & (PAGE_SIZE - 1));
        char c = *(char *)PA_TO_VA(pa);

        dst[i] = c;
        if (c == '\0') {
            return i;
        }
        i++;
    }

    dst[i] = '\0';
    return i;
}
```

### Key Changes
1. Add `paddr_t cur_page_pa` to cache the physical page base address
2. Call `walk()` **once** when initializing (get the first page's physical address)
3. Call `walk()` **only** when crossing page boundaries
4. Reuse `cur_page_pa + offset` for all bytes within the same page
5. Eliminates ~4095 redundant `walk()` calls per page

### Correctness Notes
- Behavior is identical: same bytes copied, same validation checks
- Page boundaries still validated via `validate_page()`
- No change to function signature or return values
