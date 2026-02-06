# ISSUE_21: Missing RUN_TESTS Guards in test_sync.c

## Title
Test code compiles into production builds due to missing `#ifdef RUN_TESTS` guards

## Severity
**Critical**

## Location
File: `/Users/davidklassen/work/davidklassen/slopix/kernel/tests/test_sync.c`
- Lines 1-105: Entire file missing guards

## Description

`test_sync.c` is missing the `#ifdef RUN_TESTS` / `#endif` preprocessor guards that wrap all other test files in the kernel. This causes all test code (test functions and test suite definition) to be compiled into production builds when `RUN_TESTS` is not defined.

All other test files in the codebase follow the correct pattern:
- `test_bio.c` lines 1-95: Wrapped in `#ifdef RUN_TESTS` / `#endif`
- `test_vmm.c`: Wrapped in `#ifdef RUN_TESTS` / `#endif`
- `test_virtio.c`: Wrapped in `#ifdef RUN_TESTS` / `#endif`
- And all other test_*.c files

When `RUN_TESTS` is not defined (normal builds), the test macros in `test.h` expand to `((void)0)` no-ops, but without the guards, the entire file including the TEST() function definitions and TEST_SUITE() still get compiled into the kernel binary, bloating the production build with unused test code.

## How to Reproduce

1. Build a production kernel without `RUN_TESTS` defined:
   ```
   make run  # or make test in normal mode
   ```

2. Examine the resulting kernel binary:
   ```
   nm kernel | grep spinlock_init
   nm kernel | grep sleeplock_init
   nm kernel | grep sync_test
   ```

3. Observe that test functions (`spinlock_init`, `sleeplock_lock_unlock`, etc.) and `sync` test suite symbols appear in the production binary

4. Compare with a properly guarded file:
   ```
   nm kernel | grep bread_returns_buffer
   ```
   These symbols should not appear in the binary

## Test Recommendations

1. **Binary size verification**: Build kernel with and without the fix, compare binary sizes:
   ```
   make clean && make run
   ls -lh kernel
   ```

2. **Symbol inspection**: Verify test symbols are absent from production builds:
   ```
   nm kernel | grep -E "(spinlock_init|sleeplock_init|sync_test)"
   # Should return empty
   ```

3. **Test mode verification**: Ensure test mode still works correctly:
   ```
   make test  # With RUN_TESTS defined
   # Tests should run and pass
   ```

4. **Run full test suite**: Verify no regressions:
   ```
   make tidy && make test
   ```

## Fixing Recommendations

Wrap the entire file with preprocessor guards, exactly matching the pattern used in `test_bio.c`:

**Add at line 1 (before includes):**
```c
#ifdef RUN_TESTS

```

**Add at end of file (after line 104, before EOF):**
```c
#endif
```

**Complete structure should be:**
```c
#ifdef RUN_TESTS

#include "test.h"
#include "sync.h"
#include "cpu.h"

TEST(spinlock_init) {
	// ... existing code ...
}

// ... remaining tests ...

TEST_SUITE(sync) {
	// ... existing code ...
}

#endif
```

This ensures:
1. Test code is only compiled when `RUN_TESTS` is defined
2. Production builds don't include unused test symbols
3. Consistency with all other test files in the codebase
4. No impact on test functionality - test.h macros are designed to work correctly with these guards
