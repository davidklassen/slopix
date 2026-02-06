# Unused Parameter in add_default_include_paths()

## Severity
**Low**

## File and Location
- **File:** `/Users/davidklassen/work/davidklassen/slopix/cmd/cc/main.c`
- **Lines:** 79-92

## Description
The function `add_default_include_paths()` accepts an `argv0` parameter but immediately suppresses it with `(void)argv0` on line 80, indicating it was never intended to be used. The function instead relies on `getenv("CC_INCLUDE_PATH")` and a hardcoded fallback path `/include`.

### Current Code
```c
static void add_default_include_paths(char *argv0) {
    (void)argv0;

    char *include_path = getenv("CC_INCLUDE_PATH");
    if (include_path) {
        strarray_push(&include_paths, include_path);
    } else {
        strarray_push(&include_paths, "/include");
    }

    for (int i = 0; i < include_paths.len; i++) {
        strarray_push(&std_include_paths, include_paths.data[i]);
    }
}
```

### Problem
The `argv0` parameter exists only to be discarded. This is dead code that:
1. Clutters the function signature with an unused parameter
2. Suggests historical intent to compute paths relative to the executable location (e.g., `dirname(argv0)/include`)
3. Creates confusion about function design and intent
4. May indicate incomplete refactoring or removed functionality

The function is called at line 674:
```c
if (opt_cc1) {
    add_default_include_paths(argv[0]);
    cc1();
    return 0;
}
```

## How to Verify
1. Search for all calls to `add_default_include_paths()` - only one exists at line 674
2. Confirm the parameter is never dereferenced or used within the function
3. Verify the function's logic depends only on `getenv("CC_INCLUDE_PATH")` and the hardcoded path

## Test Recommendations
After fixing, verify that:
1. Include path resolution still works correctly with environment variable set:
   ```
   CC_INCLUDE_PATH=/custom/path cc -E test.c
   ```
2. Include path resolution works with the default fallback:
   ```
   cc -E test.c  # Should use /include as default
   ```
3. Standard header includes are resolved correctly in both cases

## Fixing Recommendations
**Option 1: Remove the unused parameter (Recommended)**

Change line 79 from:
```c
static void add_default_include_paths(char *argv0) {
    (void)argv0;
```

To:
```c
static void add_default_include_paths(void) {
```

Then update the call at line 674 from:
```c
add_default_include_paths(argv[0]);
```

To:
```c
add_default_include_paths();
```

**Option 2: Actually use argv0 for relative path computation (if desired)**

If the intent was to support executable-relative include paths (a common pattern), the implementation could be:
```c
static void add_default_include_paths(char *argv0) {
    char *include_path = getenv("CC_INCLUDE_PATH");
    if (include_path) {
        strarray_push(&include_paths, include_path);
    } else {
        // Compute path relative to executable location
        char *dir = dirname(strdup(argv0));
        strarray_push(&include_paths, format("%s/include", dir));
    }
    // ... rest of function
}
```

However, this requires careful path handling and testing. Option 1 is simpler and appropriate given current usage patterns.
