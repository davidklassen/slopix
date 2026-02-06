# Issue 11: Impossible Assertion in blockwise_file_rw Test

## Severity
High

## File and Line
- **File:** `/Users/davidklassen/work/davidklassen/slopix/cmd/tests/test_stdio.c`
- **Line:** 651

## Description
The `blockwise_file_rw` test contains an impossible assertion that defeats error reporting. The assertion:

```c
ASSERT_EQ(global_i, -1, "byte mismatch");
```

This assertion can never succeed because `global_i` is computed as `block * 1024 + i` where both `block` and `i` are non-negative loop variables (ranges: `block ∈ [0,4]`, `i ∈ [0,1023]`). The value of `global_i` is always >= 0.

The assertion was likely intended to report a test failure when a byte mismatch is detected, but due to this impossible condition, it will always fail if executed. This masks the actual bug location and makes it impossible to identify which byte in the 5000-byte buffer caused the mismatch.

## How to Reproduce
The bug manifests only when a byte mismatch occurs during the test:

1. Introduce a data corruption scenario where bytes written differ from bytes read
2. Run the test suite with `make test`
3. When a mismatch is detected in the inner loop (line 647), the test will always fail with the impossible assertion rather than reporting the actual mismatch location

## Test Recommendations
The test is currently not failing because the file I/O is working correctly. To verify the fix:

1. After correcting the assertion, intentionally corrupt the file write logic to cause a mismatch
2. Verify the test correctly reports the byte offset where the mismatch occurred
3. Restore the original (working) code and confirm the test passes

## Fixing Recommendations
Replace line 651 with a proper assertion that reports the actual mismatch:

```c
ASSERT_EQ(global_i, -1, "byte mismatch at offset");
```

Should be changed to report the actual problematic byte offset. Two recommended approaches:

**Option 1 (Simple - skip on mismatch):**
```c
if (rbuf[i] != expected) {
    close(fd);
    unlink("/test_block.txt");
    ASSERT(0, "byte mismatch");
}
```

**Option 2 (Informative - report offset):**
```c
if (rbuf[i] != expected) {
    close(fd);
    unlink("/test_block.txt");
    // Test will fail with a clear message about where the mismatch occurred
    char msg[64];
    snprintf(msg, sizeof(msg), "byte mismatch at offset %d", global_i);
    ASSERT(0, msg);
}
```

The second approach provides better debugging information by reporting the exact byte offset (0-4999) where the mismatch was detected.
