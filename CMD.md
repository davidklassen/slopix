# Basic Userspace Implementation

Goal: Run user programs compiled from C with a minimal shell.

## Target Programs

| Program | Description |
|---------|-------------|
| **init** | Interactive shell based on prompt.c, spawns cursor_blink |
| **cursor_blink** | Blinks cursor every 500ms, spawned by init |
| **echo** | Outputs arguments to stdout |
| **ticker [ms]** | Prints "tick N" every ms milliseconds, exits on 'q' |

## Current Infrastructure

### What We Have

| Component | Status | Notes |
|-----------|--------|-------|
| User mode execution | ✓ | EL0 with TTBR0 per-process |
| Syscall dispatch | ✓ | x8=number, x0-x7=args, x0=return |
| `write(fd, buf, len)` | ✓ | SYS_write=0, stdout only |
| `exit(status)` | ✓ | SYS_exit=1 |
| `read(fd, buf, len)` | ✓ | SYS_read=2, stdin only, non-blocking |
| `sleep(ticks)` | ✓ | SYS_sleep=3, calls ksleep() |
| `getpid()` | ✓ | SYS_getpid=4 |
| Page table management | ✓ | uvm_create/map_page/free |
| Process scheduling | ✓ | Round-robin with timer preemption |
| Scheduler reaping | ✓ | Frees dead process resources |
| Timer | ✓ | 10ms tick, ksleep() in kernel |
| User build system | ✓ | cmd/ with Makefile, libc, crt0 |

### What We Need

| Component | Priority | Effort |
|-----------|----------|--------|
| `fork()` syscall | High | Medium |
| `wait()` syscall | High | Small |
| `exec()` syscall | High | Large |
| ELF loader | High | Medium |
| Initramfs | High | Medium |
| argc/argv setup | High | Small |

---

## Design

### Syscall Interface

```
SYS_write   0   write(fd, buf, len) -> bytes written
SYS_exit    1   exit(status) -> never returns
SYS_read    2   read(fd, buf, len) -> bytes read (0 if empty)
SYS_sleep   3   sleep(ticks) -> 0
SYS_getpid  4   getpid() -> pid
SYS_fork    5   fork() -> pid (0 in child, >0 in parent)
SYS_wait    6   wait() -> child pid
SYS_exec    7   exec(name) -> never returns on success, -1 on error
```

### User Memory Layout

```
0x0000_0000_0000_0000  +------------------+
                       | ELF .text        | (RX)
                       | ELF .rodata      | (RO)
                       | ELF .data        | (RW)
                       | ELF .bss         | (RW)
                       +------------------+
                       |                  |
                       | (unmapped)       |
                       |                  |
0x0000_0000_7FFF_F000  +------------------+
                       | User Stack       | (RW) grows down
0x0000_0000_8000_0000  +------------------+
```

### Initramfs Format

Simple custom format (easier than cpio):

```
Header:
  u32 magic      "SRAM" (0x4D415253)
  u32 count      number of files

Per file:
  u32 name_len   length of name (no null)
  u32 data_len   length of file data
  char name[]    file name (no null, padded to 4 bytes)
  char data[]    file contents (padded to 4 bytes)
```

Embedded in kernel via objcopy, accessed via linker symbols.

### ELF Loading

Minimal loader for static PIE executables:

1. Validate ELF magic and e_machine (EM_AARCH64 = 183)
2. Iterate program headers via e_phoff
3. For each PT_LOAD (p_type = 1):
   - Allocate pages at p_vaddr
   - Copy p_filesz bytes from p_offset
   - Zero remaining (p_memsz - p_filesz) for BSS
   - Set permissions from p_flags (PF_R=4, PF_W=2, PF_X=1)
4. Set up stack with argc/argv
5. Jump to e_entry

### argc/argv Stack Setup

```
sp+24: NULL           (end of argv)
sp+16: argv[1]        (pointer to first arg string)
sp+8:  argv[0]        (pointer to program name)
sp:    (16-byte aligned)

Strings stored below argv pointers.

Registers on entry to _start:
  x0 = argc
  x1 = argv (pointer to sp+8)
  sp = stack pointer (16-byte aligned)
```

### User Build System

```
cmd/
  Makefile           # Builds all user programs
  link.ld            # User linker script
  crt0.S             # _start entry point
  syscall.S          # Syscall stubs
  libc.c             # Minimal libc (strlen, memset, etc.)
  libc.h
  init.c
  cursor_blink.c
  echo.c
  ticker.c
```

**Linker script** (cmd/link.ld):
```ld
ENTRY(_start)
SECTIONS {
    . = 0x10000;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
}
```

**Entry point** (cmd/crt0.S):
```asm
.global _start
_start:
    mov x29, #0
    mov x30, #0
    bl main
    mov x8, #1      // SYS_exit
    svc #0
```

**Syscall stubs** (cmd/syscall.S):
```asm
.global write, exit, read, sleep, getpid, fork, wait, exec

write:  mov x8, #0; svc #0; ret
exit:   mov x8, #1; svc #0; ret
read:   mov x8, #2; svc #0; ret
sleep:  mov x8, #3; svc #0; ret
getpid: mov x8, #4; svc #0; ret
fork:   mov x8, #5; svc #0; ret
wait:   mov x8, #6; svc #0; ret
exec:   mov x8, #7; svc #0; ret
```

---

## Program Specifications

### init

```c
// Spawn cursor_blink as background process
// Display prompt, read commands, fork+exec+wait

int main() {
    // Spawn cursor blink
    if (fork() == 0) {
        exec("cursor_blink");
        exit(1);
    }

    char buf[64];
    write(1, "slopix> ", 8);

    while (1) {
        int n = readline(buf, sizeof(buf));
        if (n > 0) {
            if (fork() == 0) {
                exec(buf);
                write(1, "not found\n", 10);
                exit(1);
            }
            wait();
        }
        write(1, "slopix> ", 8);
    }
}
```

### cursor_blink

```c
int main() {
    while (1) {
        write(1, "\x1b[?25h", 6);  // show cursor
        sleep(5);                   // 50ms
        write(1, "\x1b[?25l", 6);  // hide cursor
        sleep(5);                   // 50ms
    }
}
```

### echo

```c
int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) write(1, " ", 1);
        write(1, argv[i], strlen(argv[i]));
    }
    write(1, "\n", 1);
    return 0;
}
```

### ticker

```c
int main(int argc, char **argv) {
    int interval = 100;  // default 1 second (100 ticks)
    if (argc > 1) {
        interval = atoi(argv[1]) / 10;  // convert ms to ticks
    }

    int n = 0;
    char buf[32];
    while (1) {
        // Check for 'q' (non-blocking)
        char c;
        if (read(0, &c, 1) > 0 && c == 'q') {
            break;
        }

        // Print tick
        int len = snprintf(buf, sizeof(buf), "tick %d\n", n++);
        write(1, buf, len);
        sleep(interval);
    }
    return 0;
}
```

---

## Implementation Roadmap

### Step 1: User Build System

Create cmd/ directory with build infrastructure.

Files:
- cmd/Makefile
- cmd/link.ld
- cmd/crt0.S
- cmd/syscall.S
- cmd/libc.h, cmd/libc.c

Deliverables:
- [x] Can compile a minimal "hello" program to ELF
- [x] `make` in cmd/ produces .elf files

### Step 2: New Syscalls (read, sleep, getpid)

Add syscalls needed for basic programs.

Files:
- syscall.h (add SYS_read, SYS_sleep, SYS_getpid)
- syscall.c (implement sys_read, sys_sleep, sys_getpid)

Deliverables:
- [x] sys_read returns bytes from UART or 0 if empty
- [x] sys_sleep calls ksleep()
- [x] sys_getpid returns current->pid
- [x] Tests pass

### Step 3: ELF Loader

Load ELF binaries into user address space.

Files:
- elf.h (ELF structures)
- elf.c (load_elf function)

Deliverables:
- [ ] Parse ELF header and program headers
- [ ] Load PT_LOAD segments with correct permissions
- [ ] Return entry point

### Step 4: Initramfs

Package and embed user programs in kernel.

Files:
- initramfs.h
- initramfs.c
- cmd/Makefile (generates initramfs)
- Makefile (links initramfs.o)

Deliverables:
- [ ] Build initramfs from cmd/*.elf
- [ ] Embed in kernel image
- [ ] initramfs_find(name) returns file data

### Step 5: exec() Syscall

Load and run program from initramfs.

Files:
- syscall.c (sys_exec)
- proc.c (helper functions)

Deliverables:
- [ ] exec(name) finds program in initramfs
- [ ] Creates new address space, loads ELF
- [ ] Sets up argc/argv (initially just argv[0])
- [ ] Jumps to entry point

### Step 6: Simple Test Program

Validate infrastructure with cursor_blink.

Files:
- cmd/cursor_blink.c

Deliverables:
- [ ] cursor_blink runs as first user process
- [ ] Cursor blinks via write() and sleep()
- [ ] Remove kernel cursor_blink

### Step 7: fork() and wait() Syscalls

Process creation and synchronization.

Files:
- syscall.c (sys_fork, sys_wait)
- proc.c (proc_fork)
- mmu.c (uvm_copy)

Deliverables:
- [ ] fork() copies address space
- [ ] Returns 0 in child, pid in parent
- [ ] wait() blocks until child exits, returns pid

### Step 8: Argument Passing

Pass command line arguments to programs.

Files:
- syscall.c (update sys_exec)
- cmd/libc.c (string parsing)

Deliverables:
- [ ] exec() parses arguments
- [ ] Sets up argc/argv on user stack
- [ ] echo prints its arguments

### Step 9: init Shell

Full interactive shell.

Files:
- cmd/init.c
- Remove kernel prompt.c dependency

Deliverables:
- [ ] init spawns cursor_blink
- [ ] Reads commands, fork+exec+wait
- [ ] Kernel boots directly to init

### Step 10: ticker Program

Final test program with all features.

Files:
- cmd/ticker.c

Deliverables:
- [ ] Parses interval argument
- [ ] Prints ticks periodically
- [ ] Exits on 'q' keypress
- [ ] All programs work together

---

## Verification

After completing all steps:

- [ ] `init` starts automatically on boot
- [ ] Cursor blinks in background
- [ ] `echo hello world` prints "hello world"
- [ ] `ticker 500` prints ticks every 500ms
- [ ] Pressing 'q' stops ticker
- [ ] Multiple commands work in sequence
- [ ] No kernel code for prompt/cursor_blink
