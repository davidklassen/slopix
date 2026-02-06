# ISSUE_22: Code Duplication in Process Cleanup Logic

## Title
Process cleanup code duplicated across three exit paths

## Severity
**High**

## Files and Line Numbers

| File | Function | Lines | Context |
|------|----------|-------|---------|
| `/Users/davidklassen/work/davidklassen/slopix/kernel/proc.c` | `proc_check_signals()` | 375-391 | Signal-based process exit (SIGKILL, SIGTERM, etc.) |
| `/Users/davidklassen/work/davidklassen/slopix/kernel/syscall.c` | `sys_exit()` | 37-55 | Exit syscall (explicit process termination) |
| `/Users/davidklassen/work/davidklassen/slopix/kernel/exception.c` | `sync_exception_handler_user()` | 207-225 | Fault-based process termination (SEGFAULT, access violations) |

## Description

Process cleanup logic is copy-pasted across three separate code paths. All three perform the same sequence of operations with only minor variations:

### Common Cleanup Pattern
```c
// Close all open file descriptors
for (int fd = 0; fd < NOFILE; fd++) {
    if (current->ofile[fd]) {
        fileclose(current->ofile[fd]);
        current->ofile[fd] = 0;
    }
}

// Release working directory
if (current->cwd) {
    fs_iput(current->cwd);
    current->cwd = 0;
}

// Set exit status and state
current->exit_status = <value>;
if (current->parent) {
    current->state = ZOMBIE;
    proc_wakeup(current->parent);
} else {
    current->state = UNUSED;
}

// Yield to scheduler
proc_sched();
```

### Variations

| Location | exit_status | Notes |
|----------|-------------|-------|
| `proc_check_signals()` | Derived from signal (`-SIGKILL` or `-term_sigs[i]`) | Exit via pending signal |
| `sys_exit()` | User-provided `status` parameter | Explicit exit() syscall |
| `exception.c` | Hardcoded `-1` | Fault/exception termination |

## Root Cause

The cleanup sequence was implemented independently in each code path without extracting the common logic into a shared helper function. This occurred as different exit mechanisms (signals, syscalls, exceptions) were implemented at different times.

## Risk of Divergence

**Critical Risk**: The three implementations can easily diverge, leading to:

1. **Inconsistent cleanup**: One path might add new cleanup logic (e.g., for new resource types) without updating others
2. **Resource leaks**: File descriptors or inodes might not be released consistently across all exit paths
3. **Zombie state inconsistency**: Parent wakeup logic could diverge, causing processes to remain in ZOMBIE state indefinitely
4. **Maintenance burden**: Any bug fix in the cleanup sequence must be applied to all three locations or the system will have inconsistent behavior

### Historical Risk Example
If a new resource (e.g., memory mapping cleanup, event descriptor cleanup, etc.) needs to be added to process cleanup, a developer might fix only one or two locations, leaving the third broken.

## Current State

All three implementations are currently correct and consistent, but the code duplication creates friction:
- Developers must remember to update all three locations
- Code review becomes error-prone (easy to miss one location)
- Testing must verify all three paths independently

## Test Recommendations

When fixing this issue, ensure test coverage for all three exit paths:

1. **Signal-based exit** (`SIGKILL`, `SIGTERM`, `SIGINT`)
   - Verify file descriptors are closed
   - Verify cwd is released
   - Verify parent is awakened from wait

2. **Syscall exit** (explicit `exit()` call with various status codes)
   - Verify file descriptors are closed
   - Verify cwd is released
   - Verify exit status is preserved correctly
   - Test with parentless process (init) going to UNUSED

3. **Exception-based exit** (segmentation fault, instruction abort, data abort)
   - Verify file descriptors are closed
   - Verify cwd is released
   - Verify process transitions to ZOMBIE/UNUSED correctly
   - Verify exit_status is set to `-1`

## Fixing Recommendations

### Recommended Solution: Extract Helper Function

Create a new function in `proc.c`:

```c
// Process cleanup: close file descriptors and release resources
static void proc_cleanup(int exit_status) {
    // Close all open file descriptors
    for (int fd = 0; fd < NOFILE; fd++) {
        if (current->ofile[fd]) {
            fileclose(current->ofile[fd]);
            current->ofile[fd] = 0;
        }
    }

    // Release working directory
    if (current->cwd) {
        fs_iput(current->cwd);
        current->cwd = 0;
    }

    // Set exit status and transition to ZOMBIE/UNUSED
    current->exit_status = exit_status;
    if (current->parent) {
        current->state = ZOMBIE;
        proc_wakeup(current->parent);
    } else {
        current->state = UNUSED;
    }

    proc_sched();
}
```

### Changes Required

1. **kernel/proc.c** - Add `proc_cleanup()` helper, update `proc_check_signals()` (line 374 onward) to call it
2. **kernel/syscall.c** - Update `sys_exit()` (lines 37-55) to call `proc_cleanup(status)`
3. **kernel/exception.c** - Update `sync_exception_handler_user()` (lines 207-225) to call `proc_cleanup(-1)`

### Benefits

- Single source of truth for process cleanup logic
- Easier to add new cleanup requirements (e.g., resource tracking, audit logging)
- Consistent behavior across all exit paths
- Simpler code review and maintenance
- Clear contract: `proc_cleanup()` always results in process yielding to scheduler

### Considerations

- Function should be `static` (internal to proc.c)
- Document that `proc_cleanup()` does NOT return (calls `proc_sched()`)
- Add comment: "Must be called with IRQs in correct state (usually disabled for syscall/exception paths)"
