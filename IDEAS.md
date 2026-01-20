# Ideas

## Testing

### Userspace tests
- Create `cmd/test_runner.c` that tests syscalls from actual userspace
- Tests: write, read, fork, wait, exec, exit, getpid, sleep
- Tests: bad pointers return -1 (user pointer validation)
- Exit with status 0 on success, 1 on failure

### Post-scheduler tests
- Tests that run after scheduler is initialized
- Test context switching between processes
- Test yield causes switch
- Test process exit and reaping

### Kernel arguments
- Parse kernel command line from QEMU (`-append "test"`)
- QEMU puts args in device tree `/chosen/bootargs`
- Or use `-device loader` to put args at known address
- Kernel checks args to decide: run shell vs run test suite

## Sleep and Scheduling

### Sleep channels
Current sleep is busy-wait with yield. Process stays RUNNABLE, keeps getting scheduled.

Proper sleep:
```c
void sleep(void *chan) {
    current->chan = chan;
    current->state = SLEEPING;
    sched();
    current->chan = 0;
}

void wakeup(void *chan) {
    for each proc p:
        if p->state == SLEEPING && p->chan == chan:
            p->state = RUNNABLE;
}
```

SLEEPING processes not scheduled, saves CPU, foundation for blocking I/O.

### Blocking read
- `read()` sleeps until data available
- UART RX interrupt calls `wakeup()` on waiting processes
- Requires sleep channels

### Waiting on multiple events
Ticker doesn't respond to 'q' while sleeping because it can only wait on one thing. Solutions:
- `select()`/`poll()` - wait on multiple fds/events
- `read()` with timeout
- Signals (SIGINT to interrupt sleep)

## MCP Integration

### QEMU wrapper MCP
Tools for Claude to interact with running kernel:
- Start/stop kernel
- Send keystrokes
- Read serial output

### GDB MCP
Connect to QEMU's gdbstub:
- Set breakpoints
- Inspect memory/registers
- Step through code

## Syscalls

### More syscalls
| Syscall | Purpose |
|---------|---------|
| pipe | IPC, shell pipelines |
| dup/dup2 | fd manipulation for redirection |
| kill | send signals to processes |
| sbrk | heap allocation |

### File descriptor abstraction
Currently read/write hardcode fd 0/1 to UART. Need proper fd table:
```c
struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
    int readable;
    int writable;
};

struct proc {
    struct file *ofile[NOFILE];
};
```

## Signals

- SIGKILL - terminate process immediately
- SIGTERM - request termination
- SIGINT - interrupt (Ctrl-C)

## Self-Hosting

Goal: compile code on Slopix itself (TCC + text editor).

### Text editor options

| Editor | Size | Notes |
|--------|------|-------|
| kilo | ~1000 lines C | Purpose-built minimal editor, VT100 only, no dependencies. https://github.com/antirez/kilo |
| ed | tiny | Classic line editor. Painful but absolute minimum. |
| levee | small | Tiny vi clone |

kilo is a good choice - uses only read, write, and termios for raw mode.

### Syscalls needed for self-hosting

File I/O:
- open, close, read, write, lseek
- stat, fstat
- unlink, rename

Directories:
- mkdir
- getdents (or opendir/readdir)

Process:
- fork, exec, wait, exit, getpid
- pipe, dup, dup2 (for shell pipelines)

Memory:
- sbrk or mmap (for malloc)

Terminal:
- ioctl (raw mode, window size)

Time:
- time or gettimeofday (optional)

### Libc options

- Custom minimal - implement only what's needed
- musl - clean, portable, well-documented
- newlib - designed for embedded/bare-metal

## Other

- errno - proper error codes instead of just -1
- Process groups / sessions for job control
- Console/TTY abstraction separate from raw UART
