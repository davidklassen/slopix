# Shell Implementation Plan

Implementation plan for the Slopix shell and userspace utilities.

**Parent**: [ROADMAP.md](ROADMAP.md) Milestone 12: Shell

## Overview

The shell provides an interactive command interpreter with support for I/O redirection and pipes. It builds on the filesystem layer and includes essential Unix utilities.

```
+------------------+
|   User Input     |  keyboard → readline()
+------------------+
        |
+------------------+
|   Shell Parser   |  tokenize, build command tree
+------------------+
        |
+------------------+
|   Command Exec   |  fork(), exec(), wait()
+------------------+
        |
+------------------+
|   Syscall Layer  |  pipe(), dup(), open(), close()
+------------------+
```

## Architecture

### Command Types

The shell uses a recursive command structure (xv6-style):

```c
#define EXEC  1   // Simple command: argv[]
#define REDIR 2   // Redirection: cmd + file + fd
#define PIPE  3   // Pipeline: left | right

struct cmd {
    int type;
};

struct execcmd {
    int type;           // EXEC
    char *argv[MAXARGS];
};

struct redircmd {
    int type;           // REDIR
    struct cmd *cmd;    // Command to redirect
    char *file;         // File for redirection
    int mode;           // O_RDONLY, O_WRONLY|O_CREAT, etc.
    int fd;             // fd to redirect (0=stdin, 1=stdout)
};

struct pipecmd {
    int type;           // PIPE
    struct cmd *left;   // Left side of pipe
    struct cmd *right;  // Right side of pipe
};
```

### I/O Redirection Pattern

Uses the xv6 close+dup approach (dup returns lowest available fd):

```c
// Output redirect: cmd > file
close(1);                           // Close stdout
open(file, O_WRONLY|O_CREAT|O_TRUNC); // Opens as fd 1
exec(cmd);

// Input redirect: cmd < file
close(0);                           // Close stdin
open(file, O_RDONLY);               // Opens as fd 0
exec(cmd);
```

### Pipe Execution

```c
// cmd1 | cmd2
int p[2];
pipe(p);

if (fork() == 0) {  // Left child
    close(1);       // Close stdout
    dup(p[1]);      // Dup write end to stdout
    close(p[0]);
    close(p[1]);
    exec(cmd1);
}

if (fork() == 0) {  // Right child
    close(0);       // Close stdin
    dup(p[0]);      // Dup read end to stdin
    close(p[0]);
    close(p[1]);
    exec(cmd2);
}

close(p[0]);
close(p[1]);
wait(); wait();     // Wait for both children
```

## Device Infrastructure

### Character Devices (devsw)

Sequential byte stream devices.

| Major | Device | Description |
|-------|--------|-------------|
| 1 | console | UART terminal |
| 2 | null | Discard writes, EOF on read |

```c
struct devsw {
    int (*read)(char *dst, int n);
    int (*write)(const char *src, int n);
};

extern struct devsw devsw[NDEV];
```

### Block Devices (bdevsw)

Random-access block devices.

| Major | Device | Description |
|-------|--------|-------------|
| 1 | disk | Virtio block device |

```c
struct bdevsw {
    int (*read)(uint32_t blockno, char *buf);
    int (*write)(uint32_t blockno, const char *buf);
};

extern struct bdevsw bdevsw[NBDEV];
```

### /dev Directory

Created at filesystem image build time by mkfs:

```
/dev/
├── console    T_DEVICE  (major=1, char)
├── null       T_DEVICE  (major=2, char)
└── disk       T_BDEVICE (major=1, block)
```

## Milestones

### S1: libc String Extensions

Add missing string and character functions.

- [x] Implement `strncmp(s1, s2, n)` in libc/string.c
- [x] Implement `strcpy(dst, src)` in libc/string.c
- [x] Implement `strncpy(dst, src, n)` in libc/string.c
- [x] Implement `strcat(dst, src)` in libc/string.c
- [x] Implement `strchr(s, c)` in libc/string.c
- [x] Implement `strstr(haystack, needle)` in libc/string.c
- [x] Implement `memmove(dst, src, n)` in libc/string.c
- [x] Update libc/include/string.h with declarations
- [x] Create libc/ctype.c with `isspace()`, `isdigit()`, `isalpha()`
- [x] Create libc/include/ctype.h
- [x] Create cmd/tests/test_libc.c with tests for all new functions
- [x] Add test_suite_libc to cmd/tests/tests.c

**Exit criteria**: All string/ctype functions available and tested.

### S2: Additional Syscalls

Implement missing syscalls for shell functionality.

- [x] **stat(path, buf)** - Get file info by path:
  - Add SYS_stat (21) to kernel/syscall.h
  - Implement sys_stat: namei() -> ilock() -> stati() -> copyout
  - Add wrapper to libc
- [x] **getcwd(buf, size)** - Get current working directory:
  - Add SYS_getcwd (22) to kernel/syscall.h
  - Implement sys_getcwd: walk parent chain, build path string
  - Add wrapper to libc
- [x] **lseek(fd, offset, whence)** - Reposition file offset:
  - Add SYS_lseek (23) to kernel/syscall.h
  - Implement sys_lseek: validate fd, update f->off based on whence
  - Support SEEK_SET, SEEK_CUR, SEEK_END
  - Add wrapper to libc
- [x] **rename(oldpath, newpath)** - Rename/move file or directory:
  - Add SYS_rename (24) to kernel/syscall.h
  - Implement sys_rename: link new name, unlink old name
  - Supports directory rename with ".." update and cycle detection
  - Uses global sleeplock for race-free operation
  - Add wrapper to libc
- [x] Add O_APPEND flag support (seeks to end on each write)
- [x] Add syscall tests to cmd/tests/test_syscall.c

**Exit criteria**: stat, getcwd, lseek, rename syscalls work and tests pass.

### S3: exec from Disk Filesystem

Modify exec to load programs from disk instead of initramfs.

- [x] Modify sys_exec() in kernel/syscall.c:
  - Use namei() to find program file
  - Read ELF header from inode via readi()
  - Load segments from disk
  - Keep initramfs as fallback (for testing)
- [x] Extend mkfs to add program binaries:
  - Add ELF files to root directory during image creation
- [x] Update Makefile to build user programs into disk.img
- [x] Test: shell can exec programs from disk

**Exit criteria**: Programs load from disk filesystem.

### S4: Device Infrastructure

Set up /dev with character and block devices.

- [x] **Null device driver**:
  - Add NULLDEV (2) constant to kernel/file.h
  - Implement nullread(): return 0 (EOF)
  - Implement nullwrite(): return n (discard)
  - Register in console_init()
- [x] **Block device infrastructure**:
  - Define struct bdevsw in kernel/file.h
  - Create bdevsw[] array in kernel/disk.c
  - Add T_BDEVICE inode type to kernel/fs.h
  - Add FD_BDEVICE file type to kernel/file.h
  - Implement fileread/filewrite for block devices (byte offset -> block)
  - Update sys_lseek to support block devices
- [x] **Disk block device**:
  - Create kernel/disk.c with disk_read/disk_write functions
  - Register as bdevsw[DISK] in disk_init()
  - Call disk_init() from kernel_main()
- [x] **Extend mkfs for /dev**:
  - Support `:dir:/path` for creating subdirectories
  - Support `:cdev:/path:major:minor` for character device nodes
  - Support `:bdev:/path:major:minor` for block device nodes
  - Create /dev/console, /dev/null, /dev/disk in disk.img and test.img
- [x] Add device tests to cmd/tests/test_devices.c

**Exit criteria**: /dev/console, /dev/null, /dev/disk exist and tests pass.

### S5: Shell Built-in Commands

Commands that run in the shell process itself.

- [x] **cd [dir]**:
  - Parse optional argument (default: "/")
  - Call chdir() syscall
  - Print error if path not found
- [x] **pwd**:
  - Call getcwd()
  - Print result
- [x] **exit [status]**:
  - Parse optional status (default: 0)
  - Call exit()
- [x] Modify shell main loop:
  - Check for built-in before fork/exec
  - Call built-in handler directly

**Exit criteria**: cd, pwd, exit work in shell.

### S6: Shell Parser and I/O Redirection

Implement command parsing with redirection support.

- [x] **Tokenizer**:
  - Implement gettoken(): recognize words, |, <, >, >>
  - Implement peek(): look ahead without consuming
  - Handle whitespace and special characters
- [x] **Parser** (recursive descent):
  - parsecmd(): top-level entry point
  - parsepipe(): handle | operator
  - parseredirs(): handle <, >, >>
  - parseexec(): parse command and arguments
- [x] **Command structures**:
  - Define execcmd, redircmd, pipecmd structs
  - Static memory pools for allocation (no malloc)
- [x] **Redirection execution**:
  - Output (>): close(1), open(file, O_WRONLY|O_CREAT|O_TRUNC)
  - Input (<): close(0), open(file, O_RDONLY)
  - Append (>>): close(1), open(file, O_WRONLY|O_CREAT|O_APPEND)
- [x] **Error handling**:
  - Print "syntax error" for malformed input
  - Print "cannot open file" for redirection failures
- [x] **cat utility** (needed for exit criteria):
  - Create cmd/cat/cat.c
  - Add to cmd/Makefile and kernel/Makefile

**Exit criteria**: `echo hello > file`, `cat < file`, `echo more >> file` work.

### S7: Pipe Support

Parse and execute pipelines.

- [x] Extend parser for | operator
- [x] Implement runcmd() for PIPE type:
  - Create pipe with pipe()
  - Fork left child: redirect stdout to pipe write end
  - Fork right child: redirect stdin to pipe read end
  - Close pipe ends in parent
  - Wait for both children
- [x] Handle multi-stage pipelines (recursive)

**Exit criteria**: `echo hello | cat`, `cat file | grep pattern | wc` work.

### S8: Core Utilities

Essential commands for shell deliverables.

- [x] **ls**:
  - Open directory (argument or ".")
  - Read directory entries
  - For each entry: stat() to get type/size
  - Print: type (d/-), size, name
  - Create cmd/ls/ls.c, add to Makefile
- [x] **cat**:
  - For each file argument: open, read loop, write to stdout, close
  - If no arguments: read from stdin
  - Create cmd/cat/cat.c, add to Makefile

**Output format for ls**:
```
d      0 .
d      0 ..
-     42 hello
d      0 subdir
```

**Exit criteria**: `ls`, `ls /dir`, `cat file`, `cat` (stdin) work.

### S9: File Manipulation Utilities

Commands for file operations.

- [x] **mkdir dir...**:
  - For each argument: call mkdir() syscall
  - Print error if fails
- [x] **rm file...**:
  - For each argument: call unlink() syscall
  - Print error if fails
- [x] **cp src dst**:
  - open() source for reading
  - open() destination with O_CREAT|O_WRONLY|O_TRUNC
  - read/write loop to copy content
  - close() both

**Exit criteria**: mkdir, rm, cp work.

### S10: Additional Utilities

Nice-to-have commands.

- [x] **mv src dst**:
  - Call rename() syscall
  - Print error if fails
- [x] **touch file...**:
  - For each argument: open(file, O_CREAT|O_WRONLY), close()
- [x] **wc [file...]**:
  - Count lines, words, characters
  - Print counts
- [x] **head [-n N] [file...]**:
  - Print first N lines (default 10)
- [x] **grep pattern [file...]**:
  - Simple substring search (not regex)
  - Print matching lines

**Exit criteria**: mv, touch, wc, head, grep work.

### S11: Readline Improvements

Enhanced line editing.

- [ ] **Arrow key detection**:
  - Detect escape sequences (ESC [ A/B/C/D)
  - Use poll() with timeout to detect escape sequences
  - Left/right arrows: move cursor position
  - Up/down: history (optional, may skip)
- [ ] **Ctrl+D handling**:
  - At start of empty line: exit shell (EOF)
  - Mid-line: ignore or delete character
- [ ] **Ctrl+C handling** (limited without signals):
  - Detect 0x03 character
  - Print ^C and newline
  - Clear current input, show new prompt

**Exit criteria**: Arrow keys move cursor, Ctrl+D exits on empty line.

## Test Strategy

### libc Tests (cmd/tests/test_libc.c)

```c
TEST(strncmp_equal) {
    ASSERT_EQ(strncmp("hello", "hello", 5), 0, "equal strings");
    return 0;
}
TEST(strncmp_prefix) {
    ASSERT_EQ(strncmp("hello", "help", 3), 0, "equal prefix");
    ASSERT(strncmp("hello", "help", 4) != 0, "different at 4");
    return 0;
}
TEST(strcpy_basic) {
    char buf[16];
    strcpy(buf, "test");
    ASSERT(strcmp(buf, "test") == 0, "copied correctly");
    return 0;
}
TEST(strcat_basic) {
    char buf[16] = "hello";
    strcat(buf, " world");
    ASSERT(strcmp(buf, "hello world") == 0, "concatenated");
    return 0;
}
TEST(strchr_found) {
    ASSERT(strchr("hello", 'e') != 0, "found e");
    ASSERT(*strchr("hello", 'e') == 'e', "points to e");
    return 0;
}
TEST(strstr_found) {
    ASSERT(strstr("hello world", "world") != 0, "found world");
    return 0;
}
TEST(memmove_overlap) {
    char buf[16] = "hello";
    memmove(buf + 2, buf, 5);
    ASSERT(buf[2] == 'h', "overlapping copy works");
    return 0;
}
TEST(isspace_basic) {
    ASSERT(isspace(' '), "space is whitespace");
    ASSERT(isspace('\t'), "tab is whitespace");
    ASSERT(!isspace('a'), "a is not whitespace");
    return 0;
}
TEST(isdigit_basic) {
    ASSERT(isdigit('0'), "0 is digit");
    ASSERT(!isdigit('a'), "a is not digit");
    return 0;
}
TEST(isalpha_basic) {
    ASSERT(isalpha('a'), "a is alpha");
    ASSERT(!isalpha('0'), "0 is not alpha");
    return 0;
}
```

### Syscall Tests (cmd/tests/test_syscall.c)

```c
TEST(stat_file) {
    struct stat st;
    ASSERT_EQ(stat("/hello", &st), 0, "stat succeeds");
    ASSERT(st.size > 0, "file has size");
    ASSERT_EQ(st.type, 1, "type is T_FILE");
    return 0;
}
TEST(getcwd_root) {
    char buf[64];
    ASSERT_NOT_NULL(getcwd(buf, 64), "getcwd succeeds");
    ASSERT(strcmp(buf, "/") == 0, "cwd is root");
    return 0;
}
TEST(lseek_set) {
    int fd = open("/hello", O_RDONLY);
    ASSERT_EQ(lseek(fd, 5, SEEK_SET), 5, "seek to 5");
    close(fd);
    return 0;
}
TEST(rename_basic) {
    int fd = open("/rentest", O_CREAT|O_WRONLY);
    close(fd);
    ASSERT_EQ(rename("/rentest", "/renamed"), 0, "rename succeeds");
    ASSERT_EQ(open("/rentest", O_RDONLY), -1, "old name gone");
    fd = open("/renamed", O_RDONLY);
    ASSERT(fd >= 0, "new name exists");
    close(fd);
    unlink("/renamed");
    return 0;
}
```

### Device Tests (cmd/tests/test_devices.c)

```c
TEST(null_read_eof) {
    int fd = open("/dev/null", O_RDONLY);
    ASSERT(fd >= 0, "open /dev/null");
    char buf[16];
    int n = read(fd, buf, 16);
    ASSERT_EQ(n, 0, "read returns 0 (EOF)");
    close(fd);
    return 0;
}
TEST(null_write_discard) {
    int fd = open("/dev/null", O_WRONLY);
    ASSERT(fd >= 0, "open /dev/null for write");
    int n = write(fd, "hello", 5);
    ASSERT_EQ(n, 5, "write returns count");
    close(fd);
    return 0;
}
TEST(disk_read_superblock) {
    int fd = open("/dev/disk", O_RDONLY);
    ASSERT(fd >= 0, "open /dev/disk");
    char buf[1024];
    lseek(fd, 1024, SEEK_SET);
    int n = read(fd, buf, 1024);
    ASSERT_EQ(n, 1024, "read full block");
    uint32_t magic = *(uint32_t*)buf;
    ASSERT_EQ(magic, 0x10203040, "superblock magic");
    close(fd);
    return 0;
}
```

### Shell Tests (manual)

```
slopix> pwd
/
slopix> mkdir testdir
slopix> cd testdir
slopix> pwd
/testdir
slopix> cd ..
slopix> echo hello > outfile
slopix> cat outfile
hello
slopix> echo world >> outfile
slopix> cat outfile
hello
world
slopix> echo hello | cat
hello
slopix> ls
d      0 .
d      0 ..
-     12 outfile
d      0 testdir
slopix> rm outfile
slopix> rm testdir
slopix> exit
```

## Implementation Order

```
S1 (libc strings)
    |
    v
S2 (syscalls: stat, getcwd, lseek, rename)
    |
    v
S3 (exec from disk) <--> S4 (device infrastructure)
    |
    v
S5 (builtins: cd, pwd, exit)
    |
    v
S6 (parser & redirection)
    |
    v
S7 (pipes)
    |
    v
S8 (ls, cat)
    |
    v
S9 (mkdir, rm, cp)
    |
    v
S10 (mv, touch, wc, head, grep)
    |
    v
S11 (readline improvements)
```

## Critical Files

| File | Changes |
|------|---------|
| kernel/syscall.c | Add stat, getcwd, lseek, rename; modify exec |
| kernel/syscall.h | Add syscall numbers 21-24 |
| kernel/file.c | Block device support in fileread/filewrite |
| kernel/file.h | Add NULLDEV, FD_BDEVICE, NBDEV, DISK, struct bdevsw |
| kernel/fs.h | Add T_BDEVICE |
| kernel/console.c | Add null device driver |
| kernel/disk.h | Block device driver header (new file) |
| kernel/disk.c | Block device driver with bdevsw[] array (new file) |
| libc/string.c | Add strncmp, strcpy, strncpy, strcat, strchr, strstr, memmove |
| libc/ctype.c | Add isspace, isdigit, isalpha (new file) |
| libc/include/*.h | Update headers |
| tools/mkfs.c | Subdirectory and device node support |
| cmd/shell/shell.c | Complete rewrite with parser |
| cmd/ls/ls.c | New utility |
| cmd/cat/cat.c | New utility |
| cmd/mkdir/mkdir.c | New utility |
| cmd/rm/rm.c | New utility |
| cmd/cp/cp.c | New utility |
| cmd/mv/mv.c | New utility |
| cmd/touch/touch.c | New utility |
| cmd/wc/wc.c | New utility |
| cmd/head/head.c | New utility |
| cmd/grep/grep.c | New utility |

## References

- [xv6-public sh.c](https://github.com/mit-pdos/xv6-public/blob/master/sh.c) - Shell reference
- [MIT 6.828 Shell Homework](https://pdos.csail.mit.edu/6.828/2014/homework/xv6-shell.html) - Tutorial
- [CS 61 Shell Tutorial](https://cs61.seas.harvard.edu/site/2018/Shell2/) - Pipe execution
- [Pipes, Forks & Dups](https://www.rozmichelle.com/pipes-forks-dups/) - Explanation
