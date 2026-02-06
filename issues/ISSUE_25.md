# Code Duplication: Source Entry Lists in libc/build.c

## Severity
**High**

## File and Location
- **File:** `/Users/davidklassen/work/davidklassen/slopix/libc/build.c`
- **Lines:** 4-56 (specifically lines 4-17 for `c_srcs[]` and lines 41-56 for `all_objs[]`)

## Description
The `c_srcs[]` array (lines 4-17) and the first portion of `all_objs[]` array (lines 42-52) contain identical entries that are maintained separately. The `all_objs[]` array is simply the concatenation of `c_srcs[]` and `asm_srcs[]`, but the `c_srcs[]` entries are duplicated rather than derived from the source array.

### Current Code
```c
// Lines 4-17: c_srcs definition
static const char *c_srcs[] = {
    "ctype",
    "dirent",
    "errno",
    "libgen",
    "malloc",
    "stdio",
    "stdio_file",
    "stdlib",
    "string",
    "test",
    "time",
    NULL,
};

// Lines 41-56: all_objs definition (duplicates c_srcs entries)
static const char *all_objs[] = {
    "ctype",      // Duplicated
    "dirent",     // Duplicated
    "errno",      // Duplicated
    "libgen",     // Duplicated
    "malloc",     // Duplicated
    "stdio",      // Duplicated
    "stdio_file", // Duplicated
    "stdlib",     // Duplicated
    "string",     // Duplicated
    "test",       // Duplicated
    "time",       // Duplicated
    "crt0",       // From asm_srcs
    "syscall",    // From asm_srcs
    NULL,
};
```

## Problem Analysis

### Maintenance Burden
- When adding a new C source file, developers must update two separate arrays
- Each array maintains its own list of identical entries, creating two sources of truth
- Changes to one list may not propagate to the other, leading to inconsistencies

### Risk of Divergence
- If a new source file is added to `c_srcs[]` but forgotten in `all_objs[]`, the object file will not be linked into the final library
- If an entry is removed from `c_srcs[]` but not from `all_objs[]`, the build may attempt to archive non-existent object files
- This creates a class of bugs that would only surface at link time, making them difficult to detect early

### Example Scenario
1. Developer adds a new source: `"math"` to `c_srcs[]` at line 15
2. Developer forgets to add `"math"` to `all_objs[]` at line 51
3. The build compiles `math.c` successfully but silently fails to include `math.o` in `libc.a`
4. Any program linking against `libc.a` will have unresolved symbols from the math module

## Risk Assessment
- **Likelihood:** Medium (manual maintenance required on every source change)
- **Impact:** Medium to High (missing object files cause link-time failures and silent linking issues)
- **Cost to Fix:** Low (simple refactoring to generate `all_objs` from source arrays)

## Test Recommendations

1. **Build Verification Test:** Verify that object files referenced in `all_objs[]` actually exist after compilation:
   ```bash
   # After running compile and assemble loops, verify each object in all_objs exists
   for obj in all_objs; do
       if [ ! -f ".build/obj/$obj.o" ]; then
           error "Object file for $obj not found"
       fi
   done
   ```

2. **Array Consistency Test:** Add a validation function to ensure `all_objs` contains exactly the union of `c_srcs` and `asm_srcs`:
   ```c
   // Verify that all_objs contains all entries from c_srcs and asm_srcs
   // and nothing more (except NULL terminator)
   void validate_obj_arrays(void) {
       int c_count = 0, asm_count = 0, all_count = 0;
       for (int i = 0; c_srcs[i]; i++) c_count++;
       for (int i = 0; asm_srcs[i]; i++) asm_count++;
       for (int i = 0; all_objs[i]; i++) all_count++;

       assert(all_count == c_count + asm_count);
   }
   ```

3. **Regression Test:** Add new source files and verify they are automatically included in the final library without manual `all_objs` updates.

## Fixing Recommendation

Replace the duplicated `all_objs[]` array with a generated list that combines `c_srcs` and `asm_srcs` dynamically. This eliminates manual maintenance of the combined list.

### Option 1: Generate all_objs at Runtime (Recommended)
```c
int main(void) {
    mkdir_p(".build/obj");
    mkdir_p(".build/out/lib");

    for (int i = 0; c_srcs[i]; i++) {
        char src[64];
        snprintf(src, sizeof(src), "%s.c", c_srcs[i]);
        if (compile(src) != 0) return 1;
    }

    for (int i = 0; asm_srcs[i]; i++) {
        char src[64];
        snprintf(src, sizeof(src), "%s.S", asm_srcs[i]);
        if (assemble(src) != 0) return 1;
    }

    // Build combined object list at runtime
    int c_count = 0;
    while (c_srcs[c_count] != NULL) c_count++;

    int asm_count = 0;
    while (asm_srcs[asm_count] != NULL) asm_count++;

    // Allocate space for combined list
    const char **all_objs = malloc((c_count + asm_count + 1) * sizeof(char*));

    // Copy c_srcs entries
    for (int i = 0; i < c_count; i++) {
        all_objs[i] = c_srcs[i];
    }

    // Copy asm_srcs entries
    for (int i = 0; i < asm_count; i++) {
        all_objs[c_count + i] = asm_srcs[i];
    }

    // Terminate list
    all_objs[c_count + asm_count] = NULL;

    if (archive_objs(".build/out/lib/libc.a", all_objs) != 0) {
        free(all_objs);
        return 1;
    }

    free(all_objs);

    mkdir_p(".build/out/include");
    if (copy_dir("include", ".build/out/include") < 0) {
        log_error("failed to install headers");
        return 1;
    }

    return 0;
}
```

### Benefits of This Fix
1. **Single Source of Truth:** `c_srcs[]` and `asm_srcs[]` are the only lists that need maintenance
2. **No Manual Concatenation:** Adding a new source to either array automatically includes it in archiving
3. **Type Safety:** The combined list is still a string array compatible with `archive_objs()`
4. **Transparency:** The runtime list generation is explicit and easy to understand
5. **Low Risk:** The fix is localized to `main()` and doesn't affect the public API

### Alternative: Macro-based Solution
If runtime allocation is not desired, a compile-time macro could generate the combined list, but this would be more complex and less maintainable than the runtime approach.

## Summary
This duplication creates a maintenance hazard that can lead to silent build failures where object files are compiled but not linked. The fix is straightforward and eliminates the possibility of the two lists diverging during future development.
