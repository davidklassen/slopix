# Signals and Job Control

A sub-roadmap for adding Unix-style signals and shell job control to slopix.

## Technical Reference

### Standard Signal Numbers

| Signal | Number | Default Action | Catchable | Description |
|--------|--------|----------------|-----------|-------------|
| SIGHUP | 1 | Terminate | Yes | Hangup |
| SIGINT | 2 | Terminate | Yes | Interrupt (Ctrl+C) |
| SIGQUIT | 3 | Core dump | Yes | Quit (Ctrl+\) |
| SIGILL | 4 | Core dump | Yes | Illegal instruction |
| SIGTRAP | 5 | Core dump | Yes | Trace trap |
| SIGABRT | 6 | Core dump | Yes | Abort |
| SIGBUS | 7 | Core dump | Yes | Bus error |
| SIGFPE | 8 | Core dump | Yes | Floating-point exception |
| **SIGKILL** | **9** | Terminate | **No** | Kill (cannot be caught) |
| SIGUSR1 | 10 | Terminate | Yes | User-defined 1 |
| SIGSEGV | 11 | Core dump | Yes | Segmentation fault |
| SIGUSR2 | 12 | Terminate | Yes | User-defined 2 |
| SIGPIPE | 13 | Terminate | Yes | Broken pipe |
| SIGALRM | 14 | Terminate | Yes | Alarm clock |
| SIGTERM | 15 | Terminate | Yes | Termination request |
| SIGCHLD | 17 | Ignore | Yes | Child status changed |
| SIGCONT | 18 | Continue | Yes | Continue if stopped |
| **SIGSTOP** | **19** | Stop | **No** | Stop (cannot be caught) |
| SIGTSTP | 20 | Stop | Yes | Terminal stop (Ctrl+Z) |
| SIGTTIN | 21 | Stop | Yes | Background read from tty |
| SIGTTOU | 22 | Stop | Yes | Background write to tty |

Sources: [POSIX signal.h](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/signal.h.html), [signal(7)](https://man7.org/linux/man-pages/man7/signal.7.html)

### waitpid Status Macros

```c
// Status check macros (exactly one is true)
WIFEXITED(status)     // Child exited normally
WIFSIGNALED(status)   // Child terminated by signal
WIFSTOPPED(status)    // Child is stopped (requires WUNTRACED)
WIFCONTINUED(status)  // Child continued (requires WCONTINUED)

// Value extraction macros
WEXITSTATUS(status)   // Exit code (if WIFEXITED)
WTERMSIG(status)      // Signal number (if WIFSIGNALED)
WSTOPSIG(status)      // Stop signal (if WIFSTOPPED)

// waitpid options
WNOHANG               // Return immediately if no child ready
WUNTRACED             // Also report stopped children
WCONTINUED            // Also report continued children
```

Source: [waitpid(2)](https://man7.org/linux/man-pages/man2/waitpid.2.html)

### Process Group Model

```
Session (SID)
├── Foreground Process Group (PGID) ← receives Ctrl+C, Ctrl+Z
├── Background Process Group 1
├── Background Process Group 2
└── ...
```

**Key behaviors:**
- Process group leader: PID == PGID
- Session leader: PID == SID
- `fork()`: Child inherits parent's PGID
- `setpgid(0, 0)`: Create new process group (caller becomes leader)
- `setsid()`: Create new session (must not already be group leader)
- Foreground group set via `tcsetpgrp(fd, pgid)`

Sources: [setpgid(2)](https://man7.org/linux/man-pages/man2/setpgid.2.html), [tcsetpgrp(3)](https://man7.org/linux/man-pages/man3/tcsetpgrp.3.html)

### ARM64 Signal Delivery

Signal delivery works by modifying the trap frame before returning to userspace:

1. **Signal becomes pending** (via kill(), fault, timer, etc.)
2. **Check pending signals** before `eret` to userspace
3. **Build signal frame** on user stack:
   - Save current trap_frame (registers, PC, PSTATE)
   - Set up siginfo_t and ucontext if needed
4. **Modify trap_frame** for handler:
   - `tf->regs[0] = signal_number`
   - `tf->elr = handler_address`
   - `tf->regs[30] = sigreturn_trampoline`
   - `tf->sp_el0 = signal_frame_address`
5. **`eret`** jumps to signal handler
6. **Handler returns** via x30 to sigreturn trampoline
7. **sigreturn syscall** restores original trap_frame
8. **`eret`** to original interrupted location

For slopix's initial implementation, we use a simpler approach: check signals at safe points and handle them synchronously (no user-space handlers yet).

Sources: [Linux ARM64 signal.c](https://github.com/torvalds/linux/blob/master/arch/arm64/kernel/signal.c)

---

## Milestone 1: Process Listing and Termination

**Goal**: Basic process management with ps and kill commands

### Deliverables

- `killed` flag in struct proc (xv6-style deferred termination)
- `sys_kill(pid)` syscall marks process for termination
- `sys_getprocs()` syscall returns process information
- `ps` command displays PID, PPID, STATE, NAME
- `kill` command terminates processes by PID
- Graceful fault handling (user faults terminate process, not hang system)

### Kernel Changes

**proc.h:**
```c
struct proc {
    // ... existing fields ...
    int killed;          // Marked for termination
    char name[16];       // Process name (from exec)
};
```

**proc.c:**
- Initialize `killed = 0` in `proc_alloc()`
- Add `proc_setkilled(pid)`: Find process, set `killed = 1`, wake if sleeping
- Check `killed` flag in `proc_wait()` and before returning to userspace

**syscall.c:**
```c
#define SYS_kill     27
#define SYS_getprocs 28
#define SYS_getppid  29
#define SYS_waitpid  30

struct procinfo {
    int pid;
    int ppid;
    int state;
    char name[16];
};

long sys_kill(int pid);
long sys_getprocs(struct procinfo *buf, int max);
long sys_getppid(void);
long sys_waitpid(int pid);  // Added to fix init.c duplicate shell bug
```

**exception.c:**
- On user fault (IABT_LOWER, DABT_LOWER): Set `current->killed = 1` instead of hanging

### Userspace Changes

**libc/include/unistd.h:**
```c
int kill(int pid);
int getppid(void);
int waitpid(int pid);
```

**cmd/ps/ps.c:** (new)
```c
// Display: PID  PPID  STATE  NAME
// States: R=running, S=sleeping, T=stopped, Z=zombie
```

**cmd/kill/kill.c:** (new)
```c
// Usage: kill <pid>
```

### Tests

| Test | Description | Expected |
|------|-------------|----------|
| ps_shows_processes | Run ps | Shows shell and ps itself |
| kill_sleeping | Start `sleep 1000`, kill it | Process terminates |
| kill_returns_error | Kill nonexistent PID | Returns -1 |
| fault_terminates | Dereference NULL in user program | Process exits, system continues |
| wait_killed_child | Kill child, parent waits | Parent receives exit status |

### Exit Criteria

- [x] `make test` passes (existing tests unbroken)
- [x] `ps` shows correct process list with states
- [x] `kill <pid>` terminates target process
- [x] User null-pointer dereference terminates process cleanly
- [x] Parent can `wait()` for killed child
- [x] `waitpid(pid)` waits for specific child (added to fix init.c bug)

### Implementation Notes

Completed implementation included:
- `killed` flag and `name[16]` fields in struct proc
- `proc_setkilled(pid)` function in proc.c
- Killed check in `proc_wait()` and `proc_wait_timeout()` to prevent race conditions
- Graceful fault handling in exception.c (IABT_LOWER, DABT_LOWER set killed flag)
- `sys_kill`, `sys_getprocs`, `sys_getppid`, `sys_waitpid` syscalls
- `ps` and `kill` commands
- printf width specifier support (`%5d`, `%5s`) added to libc
- init.c updated to use `waitpid(shell_pid)` instead of `wait()` to prevent duplicate shells when killing cursor_blink

---

## Milestone 2: Signal Numbers and Stop/Continue

**Goal**: Standard signal numbers with STOP/CONT support

### Deliverables

- Signal number definitions (SIGKILL, SIGTERM, SIGSTOP, SIGCONT, etc.)
- `kill(pid, sig)` syscall accepts signal number
- `pending` signal bitmask in struct proc
- STOPPED process state
- SIGSTOP pauses process, SIGCONT resumes

### Kernel Changes

**signal.h:** (new)
```c
#ifndef SIGNAL_H
#define SIGNAL_H

#define SIGINT   2
#define SIGKILL  9
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20

#define NSIG 32

#endif
```

**proc.h:**
```c
enum proc_state { UNUSED, RUNNABLE, RUNNING, SLEEPING, STOPPED, ZOMBIE };

struct proc {
    // ... existing fields ...
    unsigned int pending;    // Pending signal bitmask (replaces killed)
};

#define proc_is_killed(p) ((p)->pending & (1 << SIGKILL))
```

**proc.c:**
- `proc_signal(pid, sig)`: Set bit in pending, handle SIGCONT specially
- `proc_check_signals()`: Check pending before returning to user
  - SIGKILL: Exit immediately
  - SIGSTOP: Set state to STOPPED, call `proc_sched()`
  - Others: Default action (terminate)

**syscall.c:**
```c
long sys_kill(int pid, int sig);  // Extended signature
```

### Userspace Changes

**libc/include/signal.h:** (new)
```c
#define SIGINT   2
#define SIGKILL  9
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20

int kill(int pid, int sig);
```

**cmd/kill/kill.c:** Update to support `-9`, `-STOP`, `-CONT`, `-TERM`

### Tests

| Test | Description | Expected |
|------|-------------|----------|
| kill_with_sigterm | `kill -TERM <pid>` | Process terminates |
| kill_with_sigstop | `kill -STOP <pid>` | Process stops |
| kill_with_sigcont | Stop then `kill -CONT <pid>` | Process resumes |
| sigkill_uncatchable | SIGKILL always works | Process terminates |
| signal_zero | `kill -0 <pid>` | Returns 0 if exists, -1 if not |
| ps_shows_stopped | Stop a process | ps shows 'T' state |

### Exit Criteria

- [x] Process can be stopped with SIGSTOP
- [x] Stopped process resumes with SIGCONT
- [x] `ps` shows STOPPED state correctly
- [x] Signal 0 checks process existence without sending signal
- [x] SIGKILL cannot be blocked (always terminates)

### Implementation Notes

Completed implementation included:
- `kernel/signal.h` with POSIX signal number definitions (SIGINT, SIGKILL, SIGTERM, SIGSTOP, SIGCONT, etc.)
- `pending` signal bitmask replacing `killed` flag in struct proc
- `proc_is_killed(p)` macro for checking SIGKILL
- STOPPED state added to proc_state enum
- `proc_signal(pid, sig)` function replacing proc_setkilled
- `proc_check_signals()` function called after syscalls to handle pending signals
- SIGKILL and SIGCONT wake STOPPED processes
- `libc/include/signal.h` for userspace signal constants
- Extended kill command to support `-STOP`, `-CONT`, `-TERM`, `-9` options
- Updated ps command to show 'T' state for STOPPED processes
- Signal 0 checks process existence without delivering signal

---

## Milestone 3: Process Groups and Terminal Signals

**Goal**: Ctrl+C and Ctrl+Z work correctly with shell

### Deliverables

- `pgid` field in struct proc
- `setpgid()`, `getpgid()` syscalls
- Foreground process group tracking in console
- Ctrl+C sends SIGINT to foreground group
- Ctrl+Z sends SIGTSTP to foreground group
- Shell survives terminal signals sent to children

### Kernel Changes

**proc.h:**
```c
struct proc {
    // ... existing fields ...
    int pgid;            // Process group ID
};
```

**proc.c:**
- `proc_alloc()`: Initialize `pgid = pid` (each process starts as own group leader)
- `sys_fork()`: Child inherits parent's pgid
- `proc_setpgid(pid, pgid)`: Set process group
- `proc_getpgid(pid)`: Get process group
- `proc_signal_pgrp(pgid, sig)`: Send signal to all processes in group

**console.c:**
```c
static int fg_pgid;  // Foreground process group

void console_set_fg_pgid(int pgid);
int console_get_fg_pgid(void);
```

**uart.c:** In interrupt handler, detect Ctrl+C (0x03) and Ctrl+Z (0x1A):
```c
if (c == 0x03) {  // Ctrl+C
    proc_signal_pgrp(console_get_fg_pgid(), SIGINT);
    return;  // Don't buffer
}
if (c == 0x1A) {  // Ctrl+Z
    proc_signal_pgrp(console_get_fg_pgid(), SIGTSTP);
    return;
}
```

**syscall.c:**
```c
#define SYS_setpgid   30
#define SYS_getpgid   31
#define SYS_tcsetpgrp 32
#define SYS_tcgetpgrp 33

long sys_setpgid(int pid, int pgid);
long sys_getpgid(int pid);
long sys_tcsetpgrp(int fd, int pgid);
long sys_tcgetpgrp(int fd);
```

### Userspace Changes

**libc/include/unistd.h:**
```c
int setpgid(int pid, int pgid);
int getpgid(int pid);
int tcsetpgrp(int fd, int pgid);
int tcgetpgrp(int fd);
```

**cmd/shell/shell.c:**
```c
// Before fork:
int shell_pgid = getpgid(0);

// After fork, in child:
setpgid(0, 0);  // New process group

// After fork, in parent:
int child_pgid = child_pid;
tcsetpgrp(0, child_pgid);  // Child is foreground

// After wait:
tcsetpgrp(0, shell_pgid);  // Shell regains foreground
```

### Tests

| Test | Description | Expected |
|------|-------------|----------|
| ctrl_c_kills_child | Run command, press Ctrl+C | Command dies, shell survives |
| ctrl_z_stops_child | Run command, press Ctrl+Z | Command stops, shell gets prompt |
| shell_survives_sigint | Ctrl+C to child | Shell not terminated |
| getpgid_returns_correct | Check pgid after fork | Child has different pgid |
| setpgid_creates_group | `setpgid(0, 0)` | Process becomes group leader |

### Exit Criteria

- [x] Ctrl+C terminates foreground command only
- [x] Ctrl+Z stops foreground command
- [x] Shell remains responsive after sending signals
- [x] `getpgid()` returns correct values
- [x] Child processes can be placed in their own process groups

### Implementation Notes

Completed implementation included:
- `pgid` field in struct proc, initialized to pid in proc_alloc()
- `proc_setpgid()`, `proc_getpgid()`, `proc_signal_pgrp()` functions in proc.c
- `fg_pgid` tracking in console.c with `console_set_fg_pgid()` and `console_get_fg_pgid()`
- Ctrl+C (0x03) sends SIGINT and Ctrl+Z (0x1A) sends SIGTSTP to foreground group in uart.c
- `sys_setpgid`, `sys_getpgid`, `sys_tcsetpgrp`, `sys_tcgetpgrp` syscalls
- SIGTTIN check in console_read() stops background processes trying to read terminal
- SIGTTIN/SIGTTOU handling in proc_check_signals() with default Stop action
- Shell creates child process groups with setpgid(0,0) and sets foreground with tcsetpgrp()
- Shell restores itself as foreground after child exits or stops
- Enhanced waitpid() to support WNOHANG and pid=-1 (any child)
- Shell reaps background zombies with waitpid(-1, WNOHANG) before each prompt
- waitpid returns (pid << 16) | status to identify which child was reaped

---

## Milestone 4: Shell Job Control

**Goal**: Full background job support with fg, bg, jobs

### Deliverables

- Background execution with `&`
- `jobs` builtin lists background jobs
- `fg %n` brings job to foreground
- `bg %n` continues job in background
- SIGCHLD notification when child exits or stops
- `waitpid()` syscall with WNOHANG, WUNTRACED options

### Kernel Changes

**proc.c:**
- Send SIGCHLD to parent when child exits or stops

**syscall.c:**
```c
#define SYS_waitpid 34

// pid > 0: wait for specific child
// pid == -1: wait for any child
// pid == 0: wait for any child in same process group
// pid < -1: wait for any child in process group |pid|
long sys_waitpid(int pid, int *status, int options);
```

### Userspace Changes

**libc/include/sys/wait.h:** (new)
```c
#ifndef SYS_WAIT_H
#define SYS_WAIT_H

#define WNOHANG    1
#define WUNTRACED  2
#define WCONTINUED 4

#define WIFEXITED(s)    (((s) & 0x7f) == 0)
#define WEXITSTATUS(s)  (((s) >> 8) & 0xff)
#define WIFSIGNALED(s)  (((s) & 0x7f) != 0 && ((s) & 0x7f) != 0x7f)
#define WTERMSIG(s)     ((s) & 0x7f)
#define WIFSTOPPED(s)   (((s) & 0xff) == 0x7f)
#define WSTOPSIG(s)     (((s) >> 8) & 0xff)

int waitpid(int pid, int *status, int options);

#endif
```

**cmd/shell/shell.c:**
```c
#define MAXJOBS 8
#define JOB_RUNNING 1
#define JOB_STOPPED 2
#define JOB_DONE    3

struct job {
    int jid;           // Job ID [1], [2], etc.
    int pgid;          // Process group ID
    int state;         // JOB_RUNNING, JOB_STOPPED, JOB_DONE
    char cmd[64];      // Command string
};

static struct job jobs[MAXJOBS];

// Builtins:
// jobs - list all jobs
// fg %n - bring job n to foreground
// bg %n - continue job n in background
```

### Tests

| Test | Description | Expected |
|------|-------------|----------|
| background_exec | `sleep 100 &` | Returns immediately, shows [1] pid |
| jobs_lists | Run bg job, then `jobs` | Shows job with state |
| fg_brings_forward | `fg %1` | Job becomes foreground |
| bg_continues | Stop job, `bg %1` | Job continues in background |
| ctrl_z_then_bg | Ctrl+Z, then `bg` | Stopped job continues |
| waitpid_wnohang | Check child without blocking | Returns 0 if not ready |
| waitpid_wuntraced | Wait for stopped child | Returns with WIFSTOPPED |

### Exit Criteria

Full job control workflow:
- [ ] `cmd &` runs command in background
- [ ] `jobs` shows background and stopped jobs
- [ ] Ctrl+Z stops foreground job
- [ ] `bg` continues stopped job in background
- [ ] `fg` brings job to foreground
- [ ] Ctrl+C terminates foreground job
- [ ] Multiple jobs can be tracked simultaneously
- [ ] Background job completion is reported

---

## Implementation Order

```
Milestone 1: killed flag, ps, kill
     │
     ▼
Milestone 2: signal numbers, STOPPED state
     │
     ▼
Milestone 3: process groups, Ctrl+C/Z
     │
     ▼
Milestone 4: job control (&, fg, bg, jobs)
```

Each milestone builds on the previous. Within each milestone:
1. Kernel data structures
2. Kernel functions
3. Syscall handlers
4. libc wrappers
5. Userspace commands
6. Tests

---

## Files Summary

| File | M1 | M2 | M3 | M4 | Description |
|------|----|----|----|----|-------------|
| kernel/proc.h | ✓ | ✓ | ✓ | | Add killed, pending, pgid, name; STOPPED state |
| kernel/proc.c | ✓ | ✓ | ✓ | ✓ | Signal functions, pgid management |
| kernel/signal.h | | ✓ | | | Signal number constants |
| kernel/syscall.h | ✓ | ✓ | ✓ | ✓ | New syscall numbers |
| kernel/syscall.c | ✓ | ✓ | ✓ | ✓ | kill, waitpid, setpgid, getprocs |
| kernel/exception.c | ✓ | ✓ | | | Signal check, graceful faults |
| kernel/console.c | | | ✓ | | Foreground pgid tracking |
| kernel/uart.c | | | ✓ | | Ctrl+C/Z detection |
| libc/include/signal.h | | ✓ | | | Signal constants |
| libc/include/sys/wait.h | | | | ✓ | waitpid, status macros |
| libc/include/unistd.h | ✓ | ✓ | ✓ | ✓ | Function declarations |
| cmd/ps/ps.c | ✓ | | | | Process listing |
| cmd/kill/kill.c | ✓ | ✓ | | | Send signals |
| cmd/shell/shell.c | | | ✓ | ✓ | Process groups, job control |

---

## References

- [POSIX signal.h](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/signal.h.html)
- [Linux signal(7)](https://man7.org/linux/man-pages/man7/signal.7.html)
- [waitpid(2)](https://man7.org/linux/man-pages/man2/waitpid.2.html)
- [setpgid(2)](https://man7.org/linux/man-pages/man2/setpgid.2.html)
- [tcsetpgrp(3)](https://man7.org/linux/man-pages/man3/tcsetpgrp.3.html)
- [xv6-riscv proc.c](https://github.com/mit-pdos/xv6-riscv/blob/riscv/kernel/proc.c)
- [Linux ARM64 signal.c](https://github.com/torvalds/linux/blob/master/arch/arm64/kernel/signal.c)
- [GNU C Library Job Control](https://www.gnu.org/software/libc/manual/html_node/Job-Control.html)
- [MIT 6.S081 Traps Lab](https://pdos.csail.mit.edu/6.S081/2022/labs/traps.html)
