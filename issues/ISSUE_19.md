# Magic Timeout Value in Virtio Polling Path

## Title
Magic timeout value `100000000` in virtio polling path violates named constant guideline

## Severity
Medium

## Location
File: `/Users/davidklassen/work/davidklassen/slopix/kernel/virtio.c`
Line: 217

## Description

The virtio disk I/O function `virtio_disk_rw()` contains two separate timeout paths:

1. **Interrupt-driven path (lines 204-215)**: Uses named constant `VIRTIO_TIMEOUT_TICKS` (defined as 100 in `virtio.h` line 64)
2. **Polling path (lines 216-227)**: Uses bare magic literal `100000000` for nop loop iterations

The magic literal `100000000` represents a timeout counter for the fallback polling implementation but lacks:
- A named constant definition
- Comments explaining the rationale
- Clear relationship to other timeout mechanisms in the codebase

This violates the project's explicit guideline from CLAUDE.md: **"Use named constants instead of magic numbers"**

## Code Context

```c
// Interrupt-driven path - uses named constant (GOOD)
if (current) {
    unsigned long deadline = timer_get_ticks() + VIRTIO_TIMEOUT_TICKS;
    while (used->idx == last_used) {
        unsigned long now = timer_get_ticks();
        if (now >= deadline) {
            // cleanup and return timeout error
        }
        proc_wait_timeout_nointr(&virtio_disk_chan, deadline - now);
    }
} else {
    // Polling path - uses magic literal (BAD)
    unsigned long timeout = 100000000;  // <-- LINE 217: Magic number
    while (used->idx == last_used) {
        if (--timeout == 0) {
            // cleanup and return timeout error
        }
        nop();
    }
}
```

## Why This Is an Issue

1. **Inconsistency**: One path uses `VIRTIO_TIMEOUT_TICKS` (named constant), the other uses a bare literal
2. **Maintainability**: The literal provides no context for why this specific value was chosen
3. **Scalability**: If timeout semantics change, this literal could be missed during updates
4. **Code clarity**: Future developers cannot easily understand if this value is:
   - Related to timer ticks (it's not - it's nop iterations)
   - Configurable like `VIRTIO_TIMEOUT_TICKS`
   - Architecture-dependent
5. **Project standards**: Explicit violation of documented coding guidelines

## How to Reproduce

1. Open `/Users/davidklassen/work/davidklassen/slopix/kernel/virtio.c`
2. Navigate to line 217
3. Observe `unsigned long timeout = 100000000;` with no corresponding named constant in `virtio.h`
4. Compare to line 205 which uses `VIRTIO_TIMEOUT_TICKS` constant

## Test Recommendations

After fixing:

1. **Compile test**: Verify code compiles without warnings
   ```bash
   make tidy
   make clean
   make
   ```

2. **Unit/functional test**: Verify polling path timeout behavior
   - Boot kernel without interrupt support (verify polling path executes)
   - Attempt I/O that would timeout
   - Verify timeout is triggered appropriately

3. **Regression test**: Ensure interrupt-driven path still works
   ```bash
   make test
   ```

## Fixing Recommendations

Define a new named constant in `/Users/davidklassen/work/davidklassen/slopix/kernel/virtio.h` (around line 64):

```c
// Timeout and retry configuration
#define VIRTIO_TIMEOUT_TICKS    100        // timer ticks (interrupt-driven path)
#define VIRTIO_TIMEOUT_NOPS     100000000  // nop iterations (polling path fallback)
#define VIRTIO_MAX_RETRIES      3
```

Then replace line 217 in `virtio.c`:

**Before:**
```c
unsigned long timeout = 100000000;
```

**After:**
```c
unsigned long timeout = VIRTIO_TIMEOUT_NOPS;
```

## Additional Notes

- The polling path is used during early boot when process scheduler isn't running (`!current`)
- The value `100000000` appears to be empirically chosen to provide sufficient time for disk operations on QEMU
- Consider adding a comment explaining why polling and interrupt-driven paths use different timeout mechanisms and units (nops vs. ticks)
