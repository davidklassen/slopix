# Issue 3: Null Pointer Dereference in vmm_copyinstr()

## Severity
Critical

## File and Line Number
- **File:** kernel/vmm.c
- **Lines:** 274-275
- **Function:** `vmm_copyinstr()`

## Description

The `vmm_copyinstr()` function calls `walk()` on line 274 without checking if the return value is NULL before dereferencing it on line 275.

The `walk()` function can return NULL in two cases:
1. Line 37: when `alloc=0` (called with non-allocating flag) and a page table entry is invalid
2. Line 41: when memory allocation fails via `pmm_alloc()`

In `vmm_copyinstr()`, line 274 calls `walk(pagetable, addr, 0)` with `alloc=0`, making NULL returns possible. However, line 275 immediately dereferences the pointer without validation:

```c
pte_t *pte = walk(pagetable, addr, 0);
paddr_t pa = (*pte & PTE_ADDR_MASK) + (addr & (PAGE_SIZE - 1));  // NULL dereference!
```

This is inconsistent with other functions in the same file that properly check for NULL:
- `vmm_map_page()` (line 54): checks `if (pte == 0)`
- `vmm_unmap_page()` (line 68): checks `if (pte == 0 || ...)`
- `vmm_validate()` (line 233): checks `if (pte == 0 || ...)`
- `validate_page()` (line 197): checks `if (pte == 0 || ...)`

## How to Reproduce

Trigger a case where `walk()` returns NULL during string copy:

1. Call `vmm_copyinstr()` with a source address that spans multiple pages
2. Have the second page unmapped or otherwise invalid
3. The function will call `walk()` on line 274 which returns NULL
4. Dereferencing NULL on line 275 causes a fault/crash

Example scenario: A string that starts on a valid user page but continues to an unmapped page boundary would trigger this.

## Test Recommendations

Add automated test case:
- Create a user process with a string that spans two pages, where the second page is unmapped
- Call `vmm_copyinstr()` to read this string
- Verify the function returns -1 (failure) gracefully instead of faulting
- Test that the kernel handles the error without crashing

Alternatively:
- Test `vmm_copyinstr()` with various page boundary crossing scenarios
- Verify NULL return from `walk()` is handled in all branches

## Fixing Recommendations

Add NULL check after `walk()` call on line 274, matching the pattern used in other functions:

```c
// Get physical address and read byte
pte_t *pte = walk(pagetable, addr, 0);
if (pte == 0) {
    return -1;
}
paddr_t pa = (*pte & PTE_ADDR_MASK) + (addr & (PAGE_SIZE - 1));
char c = *(char *)PA_TO_VA(pa);
```

This matches the defensive programming pattern already established in:
- `vmm_validate()` line 233
- `validate_page()` line 197
- `vmm_unmap_page()` line 68
