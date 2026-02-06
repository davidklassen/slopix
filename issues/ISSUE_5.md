# ISSUE 5: Race Condition in fork() Between Child State Transition and Resource Initialization

## Severity
**Critical**

## Files and Line Numbers
- `/Users/davidklassen/work/davidklassen/slopix/kernel/proc.c:20` - `proc_alloc()` sets child state to RUNNABLE
- `/Users/davidklassen/work/davidklassen/slopix/kernel/syscall.c:257-302` - `sys_fork()` allocates child process and later copies file descriptors and cwd

## Description

A race condition exists in the fork system call where a child process becomes schedulable before its file descriptors and current working directory are properly initialized.

### The Problem

1. At **proc.c:20**, `proc_alloc()` immediately sets the newly allocated process to RUNNABLE state
2. In **syscall.c:257**, `sys_fork()` calls `proc_alloc()` which makes the child RUNNABLE
3. At **syscall.c:292-302**, much later, `sys_fork()` copies:
   - The current working directory (`child->cwd`)
   - All file descriptors from parent (`child->ofile[fd]`)

Between steps 1 and 3, a timer interrupt can fire and the scheduler (proc.c:103-143) can select the child process as RUNNABLE. The child would then run with:
- `child->cwd = 0` (uninitialized, set to 0 in proc_alloc at line 37)
- `child->ofile[fd]` completely uninitialized (full of garbage or zeros)

### Impact

If the child process gets scheduled and attempts to:
- Access any file descriptor before initialization completes
- Perform operations that depend on cwd (chdir, open relative paths, getcwd)
- Call any syscall that reads `current->ofile[fd]` or `current->cwd`

The kernel may read invalid pointers, dereference NULL, or crash. Even if the child doesn't immediately syscall, the scheduler can switch between child and parent multiple times while the child's resources are being set up.

### Why It Matters

The proc.c code initializes these fields defensively (cwd and ofile implicitly to 0), but this only works if the process never runs until initialization is complete. The RUNNABLE state makes this assumption invalid.

## How to Reproduce

This is a probabilistic race condition that may not manifest consistently:

1. Fork a process
2. Immediately after fork returns to parent, cause a timer interrupt
3. Repeat until the scheduler picks the child process before sys_fork() completes
4. The child will attempt to execute user code with uninitialized cwd and ofile[]
5. Any syscall accessing file descriptors or cwd will likely fail or crash

A more reliable reproduction would require:
- Reducing timer tick frequency or using instrumentation to inject delays
- Creating heavy fork load to increase probability of interrupt
- Adding test code that explicitly accesses cwd/ofile in child immediately after fork

## Test Recommendations

### Automated Test
Create a test in `/Users/davidklassen/work/davidklassen/slopix/tests/test.h` or a userspace test that:
1. Forks a child process
2. Child immediately opens a file descriptor (relative to cwd)
3. Child immediately calls getcwd()
4. Child prints success message
5. Verify the test passes reliably under stress (repeated forks)

### Stress Test
- Fork 100+ processes rapidly
- Each child attempts to access inherited file descriptors
- Each child performs cwd operations
- Run under timer interrupt load

## Fixing Recommendations

### Solution: Keep Process BLOCKED Until Fully Initialized

Change `proc_alloc()` to mark new processes as non-schedulable during initialization:

**Option 1: Use a BLOCKED state**
1. Modify `proc_alloc()` to set initial state to a non-schedulable state (e.g., BLOCKED/SUSPENDED or keep UNUSED until fully initialized)
2. After parent process fully initializes child resources in `sys_fork()`, change child state to RUNNABLE
3. This ensures the child cannot run until all parent-set fields are initialized

**Option 2: Defer RUNNABLE state to caller**
1. Introduce a new process state (e.g., INIT) or use existing BLOCKED
2. `proc_alloc()` sets state to INIT instead of RUNNABLE
3. Each caller (`proc_create`, `proc_create_user`, `sys_fork`) is responsible for setting state to RUNNABLE after initialization
4. This gives callers fine-grained control over when process becomes schedulable

**Option 3: Initialize ofile and cwd immediately**
1. Have `proc_alloc()` initialize `ofile[]` array to all NULL explicitly (currently implicit via 0)
2. Inherit cwd from parent in `proc_alloc()` if called from fork context
3. Less clean separation of concerns, but minimal code changes

### Recommended Fix: Option 1 (Minimal Impact)

Modify `proc_alloc()`:
```c
struct proc *proc_alloc(void) {
	for (int i = 0; i < NPROC; i++) {
		struct proc *p = &procs[i];
		if (p->state == UNUSED) {
			p->state = BLOCKED;  // Changed from RUNNABLE
			// ... rest of initialization ...
		}
	}
	return 0;
}
```

Modify `sys_fork()` to complete initialization, then set state to RUNNABLE:
```c
	// Copy file descriptors
	for (int fd = 0; fd < NOFILE; fd++) {
		if (current->ofile[fd]) {
			child->ofile[fd] = filedup(current->ofile[fd]);
		}
	}

	// NOW ready to run - make it schedulable
	child->state = RUNNABLE;

	// Parent returns child's pid
	return child->pid;
```

Modify `proc_create()` and `proc_create_user()` to set RUNNABLE after their initialization:
```c
void proc_create(proc_func func) {
	struct proc *p = proc_alloc();
	if (!p) {
		return;
	}
	// ... existing initialization ...
	p->state = RUNNABLE;  // Now schedulable
}
```

This ensures:
- All resource initialization happens before any state change to RUNNABLE
- Scheduler will never see a partially-initialized process
- Minimal changes to existing code
