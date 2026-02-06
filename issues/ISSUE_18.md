# ISSUE 18: basename() and dirname() modify input strings

**Status:** CONFIRMED DESIGN ISSUE

## Severity
**Critical** - Causes undefined behavior and segmentation faults when called with string literals

## Location
- **File:** `/Users/davidklassen/work/davidklassen/slopix/libc/libgen.c`
- **Lines affected:**
  - `dirname()`: line 33
  - `basename()`: lines 47-49

## Description

Both `basename()` and `dirname()` functions modify their input strings by writing null terminators (`'\0'`), violating the expected POSIX behavior that these functions should not modify the input.

**Problem locations:**

1. **`dirname()` at line 33:**
   ```c
   *(end + 1) = '\0';  // Modifies input string
   ```
   This writes a null terminator to the input string, truncating it.

2. **`basename()` at lines 47-49:**
   ```c
   while (end > path && *end == '/') {
       *end-- = '\0';  // Modifies input string
   }
   ```
   This loop modifies the input string by nullifying trailing slashes.

### Root Cause
The functions assume they can modify the input string directly to achieve the desired output. This assumption is invalid when:
- The input is a string literal (e.g., `basename("/path/to/file")`)
- The input is in read-only memory
- The caller expects the input string to remain unchanged

When called with string literals, the program attempts to write to read-only memory, resulting in a segmentation fault and undefined behavior.

## How to Reproduce

```c
// Example 1: basename with string literal
const char *base = basename("/path/to/file.txt");  // SEGFAULT
printf("basename: %s\n", base);

// Example 2: dirname with string literal
const char *dir = dirname("/path/to/file.txt");    // SEGFAULT
printf("dirname: %s\n", dir);

// Example 3: More obvious with const char*
const char *filename = "/usr/bin/gcc";
basename(filename);  // Undefined behavior - writes to read-only memory
```

## Test Recommendations

1. **Test with string literals** (should not segfault):
   ```c
   ASSERT_NOT_NULL(basename("/path/to/file.txt"), "basename with literal");
   ASSERT_NOT_NULL(dirname("/path/to/file.txt"), "dirname with literal");
   ```

2. **Test with modifiable strings** (ensure input is unchanged):
   ```c
   char path[] = "/path/to/file.txt";
   char *original = strdup(path);
   basename(path);
   ASSERT_STR_EQ(path, original, "input unchanged after basename");
   free(original);
   ```

3. **Test with const char pointers** (should not cause UB):
   ```c
   const char *cpath = "/usr/bin/gcc";
   // Should not segfault or cause undefined behavior
   const char *result = basename((char *)cpath);
   ```

4. **Edge cases:**
   - Root path: `/`
   - Single component: `filename.txt`
   - Empty string: ``
   - Path with trailing slashes: `/path/to/dir/`

## Fixing Recommendations

### Option 1: Use Static Buffer (Non-reentrant)
Use a static buffer to store the result, similar to POSIX implementations:
```c
char *dirname(char *path) {
    static char buf[PATH_MAX];
    // ... logic to compute dirname into buf ...
    return buf;
}
```

**Pros:** Simple, matches POSIX behavior
**Cons:** Not thread-safe or reentrant; buffer size limited

### Option 2: Require Caller-Provided Buffer
Require the caller to provide output buffer:
```c
char *dirname(char *path, char *buf, size_t size)
```

**Pros:** Thread-safe, flexible
**Cons:** Changes API signature, breaks existing callers

### Option 3: Return New Allocated String
Allocate memory for the result:
```c
char *dirname(const char *path)  // Takes const input
{
    char *result = malloc(...);
    // ... logic to compute dirname into result ...
    return result;  // Caller must free
}
```

**Pros:** Safe with literals, thread-safe
**Cons:** Adds allocation overhead, caller must free

### Option 4: Modify to Not Mutate (Minimal Change)
Avoid writing to the input string at all. Use stack-allocated buffer or pointer arithmetic without modification:
```c
char *dirname(char *path) {
    static char result[PATH_MAX];
    // ... copy path and operate on result, not path ...
    return result;
}
```

**Recommendation:** Option 1 or 4 to match POSIX semantics and be compatible with string literals.

## Related Standards

- POSIX.1-2017 `basename()`: "The basename() function may modify the string pointed to by path."
- POSIX.1-2017 `dirname()`: "The dirname() function may modify the string pointed to by path."

**Note:** While POSIX permits modification, it's non-idiomatic to actually do so in modern code. Most implementations use static buffers to avoid segfaults with string literals.

## Implementation Notes

When fixing this issue:
1. Ensure the functions can accept string literals without undefined behavior
2. Maintain backward compatibility if possible
3. Add comprehensive test cases for all edge cases
4. Document the behavior clearly (reentrant or not, thread-safe or not)
5. Consider moving to a safer signature if API change is acceptable
