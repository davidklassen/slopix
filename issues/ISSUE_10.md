# readdir() Infinite Loop on Zero d_reclen

## Severity
**Critical**

## File and Location
- **File:** `/Users/davidklassen/work/davidklassen/slopix/libc/dirent.c`
- **Line:** 57

## Description
The `readdir()` function advances the directory position by the `d_reclen` field from the linux_dirent structure without validating that `d_reclen > 0`. If the kernel's `getdents()` syscall returns malformed directory data with a zero or invalid `d_reclen` value, the position (`dirp->pos`) never advances. This causes an infinite loop where `readdir()` repeatedly returns the same directory entry without progressing through the buffer.

### Current Code
```c
struct linux_dirent *ld = (struct linux_dirent *)(dirp->buf + dirp->pos);
dirp->pos += ld->d_reclen;

entry.d_ino = ld->d_ino;
int i;
for (i = 0; i < NAME_MAX && ld->d_name[i]; i++) {
    entry.d_name[i] = ld->d_name[i];
}
entry.d_name[i] = '\0';

return &entry;
```

### Problem
When `d_reclen` is zero:
- `dirp->pos` does not advance (remains unchanged since adding 0 changes nothing)
- The while loop at line 46 continues
- The condition `dirp->pos >= dirp->end` (line 47) is false (same position, same buffer state)
- The same `linux_dirent` entry is processed again
- The function returns the same entry indefinitely

This creates an infinite loop where the calling program cannot iterate through the directory and gets stuck on a single entry.

### Root Cause
The `d_reclen` field describes the size of the current entry in the buffer. The linux kernel's `getdents()` syscall is documented to always provide valid non-zero `d_reclen` values, but:
1. Defensive programming should validate assumptions about kernel data
2. Filesystem bugs, corrupted directory data, or future kernel changes could produce zero `d_reclen`
3. There is no validation before using `d_reclen` for pointer arithmetic

## Impact
- **Severity:** Causes application hang/infinite loop when iterating directories
- **Scope:** Any application using `opendir()` / `readdir()` / `closedir()` on a directory with corrupted entries
- **Risk:** Applications become unresponsive with no way to break the loop
- **Scope:** Affects all directory scanning code in the system

## How to Reproduce
1. Create a test directory with files
2. Mock or patch the `getdents()` syscall to return a `linux_dirent` with `d_reclen = 0`
3. Call `opendir()` and `readdir()` on that directory
4. Observe: Application hangs; readdir() never returns NULL and never returns the next entry
5. Expected: Either skip the malformed entry or return error

### Minimal Test Case
```c
DIR *dir = opendir(".");
struct dirent *entry;
int count = 0;
while ((entry = readdir(dir)) != NULL) {
    count++;
    if (count > 10000) {
        // If we get here, infinite loop detected
        printf("ERROR: Infinite loop detected\n");
        break;
    }
}
closedir(dir);
```

## Test Recommendations
Add a test in the test suite that injects a malformed dirent with zero `d_reclen`:

```c
// Test: readdir handles zero d_reclen gracefully
{
    DIR *dir = opendir(".");

    // Manually inject a malformed entry with d_reclen = 0
    // This requires mocking getdents or using a special test filesystem
    // For now, test defensive behavior:

    // After the fix, readdir should either:
    // A) Skip entries with d_reclen <= 0
    // B) Return NULL when detecting malformed data
    // C) Prevent infinite loops by advancing by at least 1

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL && count < 100) {
        count++;
    }

    // Should complete without hanging
    ASSERT_TRUE(count < 100, "readdir completed without hanging");
    closedir(dir);
}
```

## Fixing Recommendation
Add validation to ensure `d_reclen > 0` before advancing the position. Change lines 56-57:

### Current Code
```c
struct linux_dirent *ld = (struct linux_dirent *)(dirp->buf + dirp->pos);
dirp->pos += ld->d_reclen;
```

### Fixed Code
```c
struct linux_dirent *ld = (struct linux_dirent *)(dirp->buf + dirp->pos);

// Validate d_reclen to prevent infinite loop on malformed entries
if (ld->d_reclen <= 0) {
    return 0;  // Treat as end of valid entries
}

dirp->pos += ld->d_reclen;
```

### Alternative Approach
If you prefer to skip malformed entries instead of failing:
```c
struct linux_dirent *ld = (struct linux_dirent *)(dirp->buf + dirp->pos);

// Validate d_reclen and skip malformed entries
if (ld->d_reclen <= 0) {
    dirp->pos = dirp->end;  // Skip to end, trigger new getdents
    continue;
}

dirp->pos += ld->d_reclen;
```

### Recommendation
Use the first approach (return 0) as it's simpler and treats corrupted directory data as a clear error condition. The second approach masks the data corruption silently.
