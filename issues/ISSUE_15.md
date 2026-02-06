# Buffer Overflow in ls.c: Unchecked strcpy() on fullpath Buffer

## Severity
**Critical**

## File and Location
- **File:** `/Users/davidklassen/work/davidklassen/slopix/cmd/ls/ls.c`
- **Lines:** 44-51 (first pass) and 72-79 (second pass)

## Description
The `ls()` function uses `strcpy()` to build full file paths into a fixed 256-byte buffer without bounds checking. The function constructs paths by concatenating user-provided directory paths with directory entry names (up to NAME_MAX = 255 bytes each).

### Current Code (Lines 44-51)
```c
char fullpath[256];
// ... in loop ...
if (strcmp(path, "/") == 0) {
    fullpath[0] = '/';
    strcpy(fullpath + 1, ent->d_name);
} else {
    strcpy(fullpath, path);
    int len = strlen(fullpath);
    fullpath[len] = '/';
    strcpy(fullpath + len + 1, ent->d_name);
}
```

### Problem
1. **Buffer Size Limitation:** The `fullpath` buffer is fixed at 256 bytes.
2. **Unbounded Input:** Directory entry names (`ent->d_name`) can be up to 255 bytes (NAME_MAX).
3. **Path Concatenation:** When constructing paths:
   - Non-root paths: `strlen(path) + 1 (separator) + strlen(d_name)` can exceed 256 bytes
   - Example: A 200-byte path + "/" + 200-byte filename = 401 bytes needed, but only 256 available
4. **strcpy() Risk:** Neither the first nor second `strcpy()` call checks remaining buffer space before writing.
5. **Code Duplication:** The same vulnerable pattern appears twice (lines 44-51 and 72-79), making the vulnerability present in both directory traversal passes.

### Impact
- **Stack Buffer Overflow:** Writing beyond the 256-byte buffer can overwrite adjacent stack variables or return addresses.
- **Denial of Service:** Crafted directory structures with long paths can crash the `ls` command.
- **Potential Code Execution:** In worst case scenarios, overflowing the buffer could enable arbitrary code execution.

## How to Reproduce
1. Create a deeply nested directory structure or use directories with long names:
   ```bash
   mkdir -p /tmp/test_$(python3 -c "print('a'*200)")
   touch /tmp/test_$(python3 -c "print('a'*200)")/$(python3 -c "print('b'*200)")
   ls /tmp/test_$(python3 -c "print('a'*200)")
   ```
2. The `ls` command will attempt to construct a path exceeding 256 bytes and overflow the stack buffer.
3. Expected behavior: Graceful error handling
4. Actual behavior: Segmentation fault or buffer corruption

## Test Recommendations
1. **Path Length Test:** Create test directories with combined path + name lengths exceeding 256 bytes and verify ls handles them safely.
2. **Stress Test:** Generate nested directories with maximum-length names (NAME_MAX = 255) and verify no buffer overflow occurs.
3. **Boundary Test:** Test paths at exactly 255 bytes, 256 bytes, and 257+ bytes.
4. **Fuzzing:** Use a fuzzer to generate random long path names and verify robustness.

## Fixing Recommendation
Replace `strcpy()` with bounds-checked alternatives:

### Option 1: Use `snprintf()` (Recommended)
```c
char fullpath[256];
// ... in loop ...
if (strcmp(path, "/") == 0) {
    snprintf(fullpath, sizeof(fullpath), "/%s", ent->d_name);
} else {
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);
}

// Check for truncation
if (strlen(path) + 1 + strlen(ent->d_name) >= sizeof(fullpath)) {
    printf("ls: path too long: %s/%s\n", path, ent->d_name);
    continue;
}
```

### Option 2: Use `strlcpy()` (if available)
```c
char fullpath[256];
// ... in loop ...
if (strcmp(path, "/") == 0) {
    fullpath[0] = '/';
    if (strlcpy(fullpath + 1, ent->d_name, sizeof(fullpath) - 1) >= sizeof(fullpath) - 1) {
        printf("ls: path too long: /%s\n", ent->d_name);
        continue;
    }
} else {
    if (strlcpy(fullpath, path, sizeof(fullpath)) >= sizeof(fullpath)) {
        printf("ls: path too long: %s\n", path);
        continue;
    }
    int len = strlen(fullpath);
    fullpath[len] = '/';
    if (strlcpy(fullpath + len + 1, ent->d_name, sizeof(fullpath) - len - 1) >= sizeof(fullpath) - len - 1) {
        printf("ls: path too long: %s/%s\n", path, ent->d_name);
        continue;
    }
}
```

Apply the same fix to both occurrences (lines 44-51 and 72-79).
