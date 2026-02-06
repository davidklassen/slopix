# Test Fragility: pmm_init_populates_freelist Uses Hardcoded Tolerance

## Severity
**High**

## File and Location
- **File:** `/Users/davidklassen/work/davidklassen/slopix/kernel/tests/test_pmm.c`
- **Lines:** 7-18

## Description
The `pmm_init_populates_freelist` test uses a hardcoded tolerance of 3000 pages (~12MB) to account for kernel and initramfs overhead. The test itself acknowledges its fragility in an embedded comment: "This test is fragile: it depends on RAM_SIZE (128MB = 32768 pages) and the size of the initramfs which is reserved before pmm_init(). As userspace programs grow, fewer pages will be available."

### Current Code
```c
// This test is fragile: it depends on RAM_SIZE (128MB = 32768 pages) and the
// size of the initramfs which is reserved before pmm_init(). As userspace
// programs grow, fewer pages will be available. We use a conservative lower
// bound that allows ~12MB for kernel + initramfs overhead.
TEST(pmm_init_populates_freelist) {
	unsigned long count = pmm_free_count();
	unsigned long total_pages = RAM_SIZE / PAGE_SIZE;
	unsigned long min_expected = total_pages - 3000; // allow ~12MB overhead
	ASSERT(count > min_expected, "Should have most of RAM available");
	ASSERT(count <= total_pages, "Cannot exceed total RAM");
	return 0;
}
```

### Problems

1. **Magic Number Without Justification**: The tolerance of 3000 pages is hardcoded with no derivation shown. Why 3000 and not 2500 or 3500? How was this number calculated or validated?

2. **Brittle to System Changes**: The test will fail when:
   - Initramfs size increases (as userspace programs grow)
   - Kernel code/data sections expand (new features, device drivers)
   - RAM_SIZE configuration changes (different QEMU variants or hardware)
   - Device tree or other reserved regions grow
   - New memory reservations are added (NUMA, secure regions, etc.)

3. **Acknowledges Its Own Fragility**: The comment admits the test is fragile and depends on specific system constants, yet doesn't provide a mechanism to adapt when those constants change.

4. **Weak Error Messages**: When the test fails, it only says "Should have most of RAM available" without indicating what the actual overhead was or why the test failed.

## When It Will Break

This test is vulnerable to failure in these scenarios:

1. **Userspace Growth**: As more programs are added to the initramfs, fewer pages remain for allocation
2. **Kernel Expansion**: New kernel features, drivers, or subsystems consume more memory
3. **Configuration Changes**: Modifying RAM_SIZE, adding device tree nodes, or changing reserved regions
4. **Maintenance Work**: Future developers won't know if 3000 is still valid without empirical testing
5. **Refactoring**: Changes to memory layout, alignment requirements, or allocation strategies may shift the overhead

Example: If initramfs grows by 5MB (1280 pages), available pages drop from ~20768 to ~19488, potentially violating the 29768 minimum (32768 - 3000).

## Test Recommendations

### Option 1: Percentage-Based Tolerance (Recommended)
Instead of a fixed page count, verify that at least a percentage of total RAM is available:

```c
TEST(pmm_init_populates_freelist) {
	unsigned long count = pmm_free_count();
	unsigned long total_pages = RAM_SIZE / PAGE_SIZE;

	// At least 60% of total RAM should be available for allocation
	// (kernel + initramfs + reserves = max 40%)
	unsigned long min_expected = (total_pages * 60) / 100;
	ASSERT(count >= min_expected, "Should have at least 60% of RAM available");
	ASSERT(count <= total_pages, "Cannot exceed total RAM");
	return 0;
}
```

**Pros**: Self-adapts to configuration changes, documents intent clearly (60% minimum)
**Cons**: Percentage threshold is still somewhat arbitrary

### Option 2: Measure Actual Reserved Memory
Query the PMM subsystem for how much memory was actually reserved, then verify the math:

```c
// In pmm.h
extern unsigned long pmm_get_reserved_count(void);

TEST(pmm_init_populates_freelist) {
	unsigned long free_count = pmm_free_count();
	unsigned long reserved_count = pmm_get_reserved_count();
	unsigned long total_pages = RAM_SIZE / PAGE_SIZE;
	unsigned long expected_free = total_pages - reserved_count;

	ASSERT_EQ(free_count, expected_free,
		"Free count should equal total pages minus reserved");
	ASSERT(free_count > 0, "Must have at least one page available");
	return 0;
}
```

**Pros**: Objective, self-validating, eliminates magic numbers
**Cons**: Requires new PMM API to expose reserved count

### Option 3: Document and Make Maintainable
If 3000 pages is the correct bound:

```c
// Maximum pages consumed by kernel image, device tree, initramfs, and
// page table structures. This includes:
// - Kernel ELF sections (code, data, bss): ~4MB
// - Device tree blob: ~256KB
// - Initramfs (userspace programs): ~8MB
// Total: ~12MB = 3000 pages
// See build/kernel.map for actual kernel size; validate initramfs size in build logs
#define MAX_KERNEL_AND_INITRAMFS_PAGES 3000

TEST(pmm_init_populates_freelist) {
	unsigned long count = pmm_free_count();
	unsigned long total_pages = RAM_SIZE / PAGE_SIZE;
	unsigned long min_expected = total_pages - MAX_KERNEL_AND_INITRAMFS_PAGES;
	ASSERT(count > min_expected, "Should have most of RAM available");
	ASSERT(count <= total_pages, "Cannot exceed total RAM");
	return 0;
}
```

**Pros**: Intent is clear, bound is documented with sources
**Cons**: Still requires manual updates if overhead grows

## Fixing Recommendation

Implement **Option 2** as the most robust solution:

1. In `/Users/davidklassen/work/davidklassen/slopix/kernel/pmm.c`, expose the reserved page count:
```c
unsigned long pmm_get_reserved_count(void) {
	// Calculate the number of pages reserved during pmm_init()
	// This is (total_pages - free_pages) after initialization
	unsigned long total = RAM_SIZE / PAGE_SIZE;
	extern unsigned long pmm_free_count(void);
	return total - pmm_free_count();
}
```

2. Add declaration in `/Users/davidklassen/work/davidklassen/slopix/kernel/pmm.h`:
```c
unsigned long pmm_get_reserved_count(void);
```

3. Update the test in `/Users/davidklassen/work/davidklassen/slopix/kernel/tests/test_pmm.c`:
```c
TEST(pmm_init_populates_freelist) {
	unsigned long free_count = pmm_free_count();
	unsigned long reserved_count = pmm_get_reserved_count();
	unsigned long total_pages = RAM_SIZE / PAGE_SIZE;
	unsigned long expected_free = total_pages - reserved_count;

	ASSERT_EQ(free_count, expected_free,
		"Free count must equal total pages minus reserved");
	ASSERT(free_count > 0, "Must have at least one page available for allocation");
	ASSERT(reserved_count < total_pages, "Reserved pages must be less than total");
	return 0;
}
```

**Benefits of this approach**:
- Eliminates the magic number entirely
- Test becomes self-validating and fail-safe
- Automatically adapts when system configuration changes
- Provides visibility into how much memory is actually reserved
- Future maintainers understand the intent without guessing
- If allocation logic breaks, test fails with clear information about what's wrong

**Alternative if Option 2 is not feasible**: Implement Option 3 with comprehensive documentation explaining the bound's derivation and when it must be updated.
