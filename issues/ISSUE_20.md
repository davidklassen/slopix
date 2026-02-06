# Circular Buffer Overflow in make_objpath() During Linking

## Severity
**Critical**

## File and Location
- **File:** `/Users/davidklassen/work/davidklassen/slopix/lib/build.h`
- **Lines:** 510-518

## Description
The `make_objpath()` function uses a fixed-size 64-slot circular buffer to generate object file paths. When linking in test mode with 48 objects, the function provides only 16 slots of headroom (25% free). If linker code consumes pointers before all buffer slots cycle, or if object count grows, **silent data corruption occurs** as newly generated paths overwrite previously returned pointers.

### Current Code
```c
static char objbufs[64][256];
static int objbuf_idx = 0;

static const char *make_objpath(const char *base) {
    snprintf(objbufs[objbuf_idx], sizeof(objbufs[0]), ".build/obj/%s.o", base);
    const char *result = objbufs[objbuf_idx];
    objbuf_idx = (objbuf_idx + 1) % 64;
    return result;
}
```

### Problem
The circular buffer operates under a critical assumption: **all returned pointers must be consumed (passed to the linker/archiver) before the buffer wraps around**. This assumption is fragile:

1. **Insufficient headroom:** Test mode uses 48 out of 64 slots. Only 16 slots remain for safety margin.
2. **Silent corruption:** If the buffer wraps while earlier pointers are still in use, `snprintf()` overwrites them with new paths.
3. **Linker execution delay:** The `link_objs()` and `archive_objs()` functions build a `Cmd` struct, append object paths, then execute the command. If the internal string buffer isn't deep-copied before later `make_objpath()` calls, corruption occurs.
4. **Growth risk:** Adding new test files reduces headroom further, approaching buffer exhaustion.

### Current Usage Counts
- Assembly sources: 4
- C sources: 25
- Test sources: 19
- **Total in test mode: 48 objects**
- **Buffer capacity: 64 slots**
- **Remaining headroom: 16 slots (25%)**

## How to Reproduce
1. Run `make test` to build in test mode (triggers 48 `make_objpath()` calls)
2. Add a few more test files to reduce headroom further
3. Observe linking errors or unexpected behavior due to corrupted object paths
4. The corruption is silent—no error message, just wrong paths passed to linker/archiver

### Scenario
If `kernel_link()` calls `make_objpath()` 48 times in sequence, then later adds more paths, the 49th call wraps to index 0, overwriting the first returned path. If the linker command hasn't executed yet, it receives corrupted paths.

## Test Recommendations
1. **Add an overflow test:** Modify `make_objpath()` to log all allocations and verify no overwrites occur during actual linking.
2. **Stress test:** Add temporary test sources to reduce headroom, verify linking still succeeds with correct object paths.
3. **Pointer lifetime validation:** Ensure all returned pointers remain valid until the command completes execution.

## Fixing Recommendation
Replace the fixed circular buffer with dynamic allocation:

```c
// Option 1: Dynamic allocation (safest)
static const char *make_objpath(const char *base) {
    char *buf = malloc(256);
    if (buf == NULL) {
        log_error("out of memory");
        return NULL;
    }
    snprintf(buf, 256, ".build/obj/%s.o", base);
    return buf;
}
```

Or:

```c
// Option 2: Increase buffer capacity with compile-time check
#define OBJBUF_SIZE 256  // Increased from 64
static char objbufs[OBJBUF_SIZE][256];
static int objbuf_idx = 0;

static const char *make_objpath(const char *base) {
    if (objbuf_idx >= OBJBUF_SIZE) {
        log_error("object path buffer exhausted");
        return NULL;
    }
    snprintf(objbufs[objbuf_idx], sizeof(objbufs[0]), ".build/obj/%s.o", base);
    const char *result = objbufs[objbuf_idx];
    objbuf_idx++;
    return result;
}

// Reset in cmd_reset() or at build completion
```

Or:

```c
// Option 3: Pre-allocate for worst case (safest)
// Calculate max objects at compile time: 4 (asm) + 25 (c) + N (tests)
#define MAX_OBJECTS 64
static_assert(sizeof(objbufs) / 256 >= MAX_OBJECTS, "buffer too small");
```

**Recommended:** Use Option 1 (dynamic) for simplicity and safety. Each path is freed by the linker command execution caller, preventing buffer exhaustion.

