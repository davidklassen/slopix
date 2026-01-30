# Editor

Port of the [kilo](https://github.com/antirez/kilo) text editor to slopix as the first TUI application, establishing terminal infrastructure for future interactive programs.

## Why Kilo as Base

Kilo is a minimal text editor (~1300 lines) that:
- Uses VT100 escape sequences directly (no curses dependency)
- Has syntax highlighting for C
- Supports search, basic editing operations
- Is well-documented and understood

It serves as a good test case for terminal capabilities because it exercises:
- Raw character input (no line buffering)
- Cursor positioning and screen clearing
- Terminal size detection
- Color output via ANSI codes

## Gap Analysis

### External Dependencies (from Kilo)

**Headers Required:**
- `<termios.h>` - Terminal control (NOT in slopix)
- `<sys/ioctl.h>` - Device control for TIOCGWINSZ (NOT in slopix)
- `<signal.h>` - Signal handlers (partial: kill() exists, signal() does not)
- `<time.h>` - time() function (EXISTS)
- `<errno.h>` - Error handling (EXISTS)
- `<stdarg.h>` - Variadic functions (EXISTS)
- All other standard headers (EXISTS)

**Functions Used:**

| Function | Status | Notes |
|----------|--------|-------|
| `tcgetattr()` | REMOVED | Replace with tcsetraw() |
| `tcsetattr()` | REMOVED | Replace with tcsetraw() |
| `isatty()` | MISSING | Check if fd is terminal |
| `ioctl(TIOCGWINSZ)` | REMOVED | Use escape sequence fallback |
| `signal()` | REMOVED | No resize support over serial |
| `getline()` | MISSING | Read line from FILE* |
| `ftruncate()` | MISSING | Truncate file to length |
| `sscanf()` | REMOVED | Manual parsing instead |
| `perror()` | MISSING | Print error message |
| `isprint()` | MISSING | Check printable character |
| `time()` | EXISTS | Status message timeout |
| `strerror()` | EXISTS | Error string |
| `malloc/realloc/free` | EXISTS | Memory allocation |
| `strlen/strchr/strstr/memset/memcpy/memmove` | EXISTS | String/memory ops |
| `isspace/isdigit` | EXISTS | Character classification |
| `snprintf/vsnprintf/fprintf` | EXISTS | Formatted output |
| `fopen/fclose` | EXISTS | File streams |
| `open/close/read/write` | EXISTS | File descriptors |
| `exit/atexit` | EXISTS | Process termination |

**Constants Missing:**
- `STDIN_FILENO`, `STDOUT_FILENO`, `STDERR_FILENO`
- `ENOTTY` error code

*Note: SIGWINCH and termios constants not needed - we remove that code entirely.*

### Critical Infrastructure Gaps

#### 1. Raw Terminal Mode (CRITICAL)

**Problem:** The UART driver (`kernel/uart.c:64-82`) intercepts Ctrl-C (0x03) and Ctrl-Z (0x1A), converting them to SIGINT/SIGTSTP signals before they reach userspace. Kilo needs these as raw bytes for its keybindings.

**Current behavior:**
```c
// kernel/uart.c - uart_irq_handler()
if (c == 0x03) {  // Ctrl-C
    proc_signal_pgrp(pgid, SIGINT);
    continue;     // Character never reaches buffer
}
```

**Solution:** Add a raw mode flag to the console that bypasses signal generation.

#### 2. Terminal Size Detection

**Problem:** Kilo uses `ioctl(fd, TIOCGWINSZ, &ws)` to get terminal dimensions. This won't work over serial - there's no out-of-band mechanism.

**Solution:** Kilo already has a fallback that queries terminal size via escape sequences:
1. Save cursor position via `\x1b[6n` (DSR)
2. Move to bottom-right with `\x1b[999C\x1b[999B`
3. Query position again (gives terminal bounds)
4. Restore cursor

We'll remove the ioctl path entirely and use only this escape sequence method. The response `ESC [ rows ; cols R` can be parsed manually without sscanf.

#### 3. Timed Reads for Escape Sequences

**Problem:** Kilo uses termios `VMIN=0, VTIME=1` for 100ms read timeout to detect escape sequences vs raw ESC key.

**Current kilo approach:**
```c
raw.c_cc[VMIN] = 0;   // Return immediately with any data or 0
raw.c_cc[VTIME] = 1;  // 100ms timeout
while ((nread = read(fd,&c,1)) == 0);  // Spin until data
```

**Solution:** Slopix has `poll(fd, timeout_ms)` which provides the same capability:
```c
while (poll(0, 100) == 0);  // Wait up to 100ms for data
read(0, &c, 1);
```

#### 4. File Truncation

**Problem:** Kilo uses `ftruncate(fd, len)` to set exact file size before writing.

**Current filesystem:** `fs_itrunc()` only truncates to zero, not to arbitrary length.

**Solution:** Implement proper `ftruncate()` syscall that can truncate to any size ≤ current size.

### Slopix Strengths (Already Working)

- **poll() with timeout** - Key for escape sequence detection
- **Process groups and foreground control** - tcsetpgrp/tcgetpgrp exist
- **File I/O** - open, read, write, close, lseek all work
- **Memory allocation** - malloc/realloc/free work
- **String functions** - Complete set available
- **ANSI escape output** - Terminal handles VT100 sequences
- **Time functions** - time() for status message timeout

## Implementation Roadmap

### Phase 1: Libc Additions (No Kernel Changes)

Simple additions to complete the C library:

**1.1 Character Functions** (libc/ctype.c)
```c
int isprint(int c) {
    return c >= ' ' && c < 127;
}
```

**1.2 Constants** (libc/include/unistd.h)
```c
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
```

**1.3 Error Codes** (libc/include/errno.h)
```c
#define ENOTTY 25
```

**1.4 perror()** (libc/stdio.c)
```c
void perror(const char *s) {
    if (s && *s)
        fprintf(stderr, "%s: %s\n", s, strerror(errno));
    else
        fprintf(stderr, "%s\n", strerror(errno));
}
```

**1.5 isatty()** (libc/unistd.c)
```c
int isatty(int fd) {
    // In slopix, fd 0/1/2 are always the console
    return fd >= 0 && fd <= 2;
}
```

**1.6 getline()** (libc/stdio_file.c)
```c
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
```
Read characters until newline, auto-growing buffer. Implementation:
- If `*lineptr` is NULL or `*n` is 0, allocate initial buffer
- Read characters via fgetc() until newline or EOF
- Grow buffer with realloc() as needed (double size)
- Store newline in buffer, null-terminate
- Update `*lineptr` and `*n` with final buffer/size
- Return number of characters read (including newline), or -1 on error/EOF with no chars

**1.7 Tests** (cmd/tests/test_libc.c, cmd/tests/test_stdio.c)

Add to `test_libc.c`:
```c
TEST(isprint_printable) {
    ASSERT(isprint(' '), "space is printable");
    ASSERT(isprint('a'), "a is printable");
    ASSERT(isprint('~'), "~ is printable");
    return 0;
}

TEST(isprint_not_printable) {
    ASSERT(!isprint('\0'), "null not printable");
    ASSERT(!isprint('\n'), "newline not printable");
    ASSERT(!isprint(0x1f), "control char not printable");
    ASSERT(!isprint(0x7f), "DEL not printable");
    return 0;
}

TEST(isatty_console) {
    ASSERT(isatty(0), "stdin is tty");
    ASSERT(isatty(1), "stdout is tty");
    ASSERT(isatty(2), "stderr is tty");
    return 0;
}

TEST(isatty_file) {
    int fd = open("/tmp_isatty", O_CREAT | O_WRONLY);
    ASSERT(!isatty(fd), "file is not tty");
    close(fd);
    unlink("/tmp_isatty");
    return 0;
}
```

Add to `test_stdio.c`:
```c
TEST(getline_basic) {
    FILE *f = fopen("/test_getline.txt", "w");
    fprintf(f, "hello\nworld\n");
    fclose(f);

    f = fopen("/test_getline.txt", "r");
    char *line = NULL;
    size_t n = 0;
    ssize_t len = getline(&line, &n, f);
    ASSERT_EQ(len, 6, "first line length");
    ASSERT_EQ(strcmp(line, "hello\n"), 0, "first line content");

    len = getline(&line, &n, f);
    ASSERT_EQ(len, 6, "second line length");
    ASSERT_EQ(strcmp(line, "world\n"), 0, "second line content");

    len = getline(&line, &n, f);
    ASSERT_EQ(len, -1, "EOF returns -1");

    free(line);
    fclose(f);
    unlink("/test_getline.txt");
    return 0;
}

TEST(getline_no_newline) {
    FILE *f = fopen("/test_getline2.txt", "w");
    fprintf(f, "no newline");
    fclose(f);

    f = fopen("/test_getline2.txt", "r");
    char *line = NULL;
    size_t n = 0;
    ssize_t len = getline(&line, &n, f);
    ASSERT_EQ(len, 10, "line without newline");
    ASSERT_EQ(strcmp(line, "no newline"), 0, "content");

    free(line);
    fclose(f);
    unlink("/test_getline2.txt");
    return 0;
}

TEST(getline_long_line) {
    FILE *f = fopen("/test_getline3.txt", "w");
    for (int i = 0; i < 200; i++) fputc('x', f);
    fputc('\n', f);
    fclose(f);

    f = fopen("/test_getline3.txt", "r");
    char *line = NULL;
    size_t n = 0;
    ssize_t len = getline(&line, &n, f);
    ASSERT_EQ(len, 201, "long line length");
    ASSERT(n >= 201, "buffer grew");

    free(line);
    fclose(f);
    unlink("/test_getline3.txt");
    return 0;
}

TEST(perror_basic) {
    // perror writes to stderr, just verify it doesn't crash
    errno = ENOENT;
    perror("test");
    errno = 0;
    perror(NULL);
    return 0;
}
```

### Phase 2: Console Raw Mode (Kernel Change)

Add mechanism to disable Ctrl-C/Ctrl-Z signal generation.

**2.1 Console Mode Flag**

Add to `kernel/console.c`:
```c
static int console_raw_mode = 0;

void console_set_raw(int raw) {
    console_raw_mode = raw;
}

int console_get_raw(void) {
    return console_raw_mode;
}
```

**2.2 Modify UART IRQ Handler**

In `kernel/uart.c`:
```c
void uart_irq_handler(void) {
    while (!(UART_REG(UART_FR_OFFSET) & UART_FR_RXFE)) {
        char c = UART_REG(UART_DR_OFFSET) & 0xFF;

        // Only intercept signals if not in raw mode
        if (!console_get_raw()) {
            if (c == 0x03) {
                int pgid = console_get_fg_pgid();
                if (pgid > 0) proc_signal_pgrp(pgid, SIGINT);
                continue;
            }
            if (c == 0x1A) {
                int pgid = console_get_fg_pgid();
                if (pgid > 0) proc_signal_pgrp(pgid, SIGTSTP);
                continue;
            }
        }
        // ... rest of handler
    }
}
```

**2.3 Syscall Interface**

New syscalls in `kernel/syscall.c`:
```c
#define SYS_tcsetraw 35
#define SYS_tcgetraw 36

static long sys_tcsetraw(int fd, int raw) {
    if (fd < 0 || fd > 2) return -EBADF;
    console_set_raw(raw);
    return 0;
}
```

**2.4 Libc Wrapper** (libc/unistd.c)
```c
int tcsetraw(int fd, int raw);
int tcgetraw(int fd);
```

**2.5 Tests**

Kernel test (kernel/tests/test_console.c):
```c
TEST(console_raw_mode_default) {
    ASSERT_EQ(console_get_raw(), 0, "raw mode off by default");
    return 0;
}

TEST(console_raw_mode_set) {
    console_set_raw(1);
    ASSERT_EQ(console_get_raw(), 1, "raw mode on");
    console_set_raw(0);
    ASSERT_EQ(console_get_raw(), 0, "raw mode off");
    return 0;
}
```

Userspace test (cmd/tests/test_syscall.c):
```c
TEST(tcsetraw_tcgetraw) {
    ASSERT_EQ(tcgetraw(0), 0, "raw mode off initially");
    ASSERT_EQ(tcsetraw(0, 1), 0, "tcsetraw succeeds");
    ASSERT_EQ(tcgetraw(0), 1, "raw mode on");
    ASSERT_EQ(tcsetraw(0, 0), 0, "tcsetraw off");
    ASSERT_EQ(tcgetraw(0), 0, "raw mode off again");
    return 0;
}

TEST(tcsetraw_invalid_fd) {
    ASSERT_EQ(tcsetraw(99, 1), -1, "invalid fd fails");
    return 0;
}
```

### Phase 3: File Truncation (Kernel Change)

**3.1 Filesystem Function**

Add to `kernel/fs.c`:
```c
int fs_itrunc_to(struct inode *ip, unsigned int len);
```
Truncate file to specified length (must be ≤ current size).

**3.2 Syscall**
```c
#define SYS_ftruncate 37

static long sys_ftruncate(int fd, long length) {
    struct file *f = current->ofile[fd];
    if (!f || !f->ip) return -EBADF;
    if (!(f->flags & O_WRONLY) && !(f->flags & O_RDWR))
        return -EINVAL;
    return fs_itrunc_to(f->ip, length);
}
```

**3.3 Libc Wrapper**
```c
int ftruncate(int fd, long length);
```

**3.4 Tests**

Userspace test (cmd/tests/test_filesys.c):
```c
TEST(ftruncate_shrink) {
    int fd = open("/test_trunc.txt", O_CREAT | O_RDWR);
    write(fd, "hello world", 11);
    ASSERT_EQ(ftruncate(fd, 5), 0, "ftruncate succeeds");

    struct stat st;
    fstat(fd, &st);
    ASSERT_EQ(st.size, 5, "file size is 5");

    lseek(fd, 0, SEEK_SET);
    char buf[16] = {0};
    read(fd, buf, 16);
    ASSERT_EQ(strcmp(buf, "hello"), 0, "content truncated");

    close(fd);
    unlink("/test_trunc.txt");
    return 0;
}

TEST(ftruncate_to_zero) {
    int fd = open("/test_trunc2.txt", O_CREAT | O_RDWR);
    write(fd, "test data", 9);
    ASSERT_EQ(ftruncate(fd, 0), 0, "truncate to zero");

    struct stat st;
    fstat(fd, &st);
    ASSERT_EQ(st.size, 0, "file size is 0");

    close(fd);
    unlink("/test_trunc2.txt");
    return 0;
}

TEST(ftruncate_same_size) {
    int fd = open("/test_trunc3.txt", O_CREAT | O_RDWR);
    write(fd, "hello", 5);
    ASSERT_EQ(ftruncate(fd, 5), 0, "truncate to same size");

    struct stat st;
    fstat(fd, &st);
    ASSERT_EQ(st.size, 5, "size unchanged");

    close(fd);
    unlink("/test_trunc3.txt");
    return 0;
}

TEST(ftruncate_readonly_fails) {
    int fd = open("/test_trunc4.txt", O_CREAT | O_WRONLY);
    write(fd, "test", 4);
    close(fd);

    fd = open("/test_trunc4.txt", O_RDONLY);
    ASSERT_EQ(ftruncate(fd, 2), -1, "readonly fd fails");

    close(fd);
    unlink("/test_trunc4.txt");
    return 0;
}
```

### Phase 4: Editor Modifications

Copy kilo.c to `cmd/editor/editor.c` and modify for slopix.

**4.1 Remove Unneeded Headers**

```c
// Remove these:
// #include <termios.h>
// #include <sys/ioctl.h>
// #include <sys/time.h>
// #include <signal.h>
```

**4.2 Remove termios Code**

Delete `struct termios orig_termios` and replace terminal handling:

```c
// Delete enableRawMode() and disableRawMode(), replace with:

static int raw_mode_enabled = 0;

void disableRawMode(void) {
    if (raw_mode_enabled) {
        tcsetraw(STDIN_FILENO, 0);
        raw_mode_enabled = 0;
    }
}

void editorAtExit(void) {
    disableRawMode();
}

int enableRawMode(void) {
    if (raw_mode_enabled) return 0;
    if (!isatty(STDIN_FILENO)) {
        errno = ENOTTY;
        return -1;
    }
    atexit(editorAtExit);
    tcsetraw(STDIN_FILENO, 1);
    raw_mode_enabled = 1;
    E.rawmode = 1;
    return 0;
}
```

**4.3 Replace editorReadKey()**

Use poll() for timed reads instead of VMIN/VTIME:

```c
int editorReadKey(int fd) {
    char c, seq[3];

    // Block until character available
    while (poll(fd, 100) == 0);
    if (read(fd, &c, 1) != 1) exit(1);

    if (c == ESC) {
        // Short timeout to detect escape sequences vs bare ESC
        if (poll(fd, 50) == 0) return ESC;
        if (read(fd, seq, 1) != 1) return ESC;
        if (poll(fd, 50) == 0) return ESC;
        if (read(fd, seq+1, 1) != 1) return ESC;

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (poll(fd, 50) == 0) return ESC;
                if (read(fd, seq+2, 1) != 1) return ESC;
                if (seq[2] == '~') {
                    switch(seq[1]) {
                    case '3': return DEL_KEY;
                    case '5': return PAGE_UP;
                    case '6': return PAGE_DOWN;
                    }
                }
            } else {
                switch(seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch(seq[1]) {
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
            }
        }
        return ESC;
    }
    return c;
}
```

**4.4 Replace getWindowSize()**

Remove ioctl, use escape sequence method only:

```c
int getWindowSize(int ifd, int ofd, int *rows, int *cols) {
    int orig_row, orig_col;

    // Get current cursor position
    if (getCursorPosition(ifd, ofd, &orig_row, &orig_col) == -1)
        return -1;

    // Move to bottom-right corner
    if (write(ofd, "\x1b[999C\x1b[999B", 12) != 12)
        return -1;

    // Query new position (gives screen dimensions)
    if (getCursorPosition(ifd, ofd, rows, cols) == -1)
        return -1;

    // Restore cursor
    char seq[32];
    snprintf(seq, 32, "\x1b[%d;%dH", orig_row, orig_col);
    write(ofd, seq, strlen(seq));

    return 0;
}
```

**4.5 Replace getCursorPosition()**

Parse response manually instead of sscanf:

```c
int getCursorPosition(int ifd, int ofd, int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (poll(ifd, 100) == 0) return -1;
        if (read(ifd, buf + i, 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != ESC || buf[1] != '[') return -1;

    // Parse "ESC [ rows ; cols R" manually
    char *p = buf + 2;
    *rows = 0;
    while (*p >= '0' && *p <= '9') {
        *rows = *rows * 10 + (*p - '0');
        p++;
    }
    if (*p != ';') return -1;
    p++;
    *cols = 0;
    while (*p >= '0' && *p <= '9') {
        *cols = *cols * 10 + (*p - '0');
        p++;
    }
    return 0;
}
```

**4.6 Remove Signal Handler**

Delete SIGWINCH handling entirely (no resize over serial):

```c
// Delete handleSigWinCh() function

// In initEditor(), remove:
// signal(SIGWINCH, handleSigWinCh);
```

### Phase 5: Build Integration

**5.1 Directory Structure**
```
cmd/editor/
├── editor.c    # Based on kilo, modified for slopix
└── Makefile
```

### Phase 6: Testing

**6.1 Unit Tests** (automated via `make test`)

Tests are defined in each phase above. Summary of new tests:

| File | New Tests |
|------|-----------|
| cmd/tests/test_libc.c | `isprint_*`, `isatty_*` |
| cmd/tests/test_stdio.c | `getline_*`, `perror_*` |
| cmd/tests/test_syscall.c | `tcsetraw_*`, `tcgetraw_*` |
| cmd/tests/test_filesys.c | `ftruncate_*` |
| kernel/tests/test_console.c | `console_raw_mode_*` |

Run all tests with `make test` - should pass before each phase is complete.

**6.2 Integration Tests**

Manual verification of raw mode behavior:
```c
// test_raw_mode.c - standalone test program
#include <unistd.h>
#include <stdio.h>

int main(void) {
    tcsetraw(0, 1);  // Enable raw mode
    printf("Press Ctrl-C (should print ^C, not kill): ");
    char c;
    read(0, &c, 1);
    printf("\nGot: 0x%02x\n", c);
    if (c == 0x03) printf("SUCCESS: Ctrl-C received as raw byte\n");
    else printf("FAIL: Expected 0x03\n");
    tcsetraw(0, 0);  // Restore
    return 0;
}
```

**6.3 Editor Manual Testing**

Once editor compiles and runs:
- [ ] Launch: `editor /etc/motd` - should display file
- [ ] Navigation: Arrow keys, Page Up/Down, Home/End
- [ ] Edit: Insert characters, backspace, delete
- [ ] Newline: Enter key splits line correctly
- [ ] Save: Ctrl-S saves file (verify with `cat`)
- [ ] Search: Ctrl-F opens search, arrows navigate matches
- [ ] Quit: Ctrl-Q with unsaved changes shows warning
- [ ] Syntax: Open a .c file, verify colors for keywords/comments
- [ ] Large file: Open file >100 lines, scroll works

## Effort Estimate

| Phase | Components | Tests | Complexity |
|-------|------------|-------|------------|
| Phase 1 | Libc additions | ~10 tests in test_libc.c, test_stdio.c | Low |
| Phase 2 | Console raw mode | ~4 tests in test_console.c, test_syscall.c | Low |
| Phase 3 | ftruncate | ~4 tests in test_filesys.c | Medium |
| Phase 4 | Editor modifications | N/A (manual testing) | Low |
| Phase 5 | Build integration | N/A | Low |
| Phase 6 | Testing | Integration + manual verification | Medium |

## Summary of Editor Changes

Functions removed:
- `enableRawMode()` - replaced with simple `tcsetraw()` call
- `disableRawMode()` - replaced with simple `tcsetraw()` call
- `handleSigWinCh()` - deleted entirely (no resize over serial)

Functions modified:
- `editorReadKey()` - use `poll()` + `read()` instead of VMIN/VTIME
- `getWindowSize()` - remove ioctl, use escape sequence only
- `getCursorPosition()` - manual parsing instead of sscanf
- `initEditor()` - remove signal() call

Headers removed:
- `<termios.h>`, `<sys/ioctl.h>`, `<sys/time.h>`, `<signal.h>`

Approximate diff from kilo: ~100 lines changed in 1300-line file.

## Dependencies

Before starting editor port, ensure these work:
- [x] poll() with timeout
- [x] File I/O (open, read, write, close)
- [x] Memory allocation
- [x] time() function
- [x] strerror() function
- [x] String functions
- [x] printf/snprintf family (including %.Ns precision)
- [ ] isprint() (Phase 1)
- [ ] isatty() (Phase 1)
- [ ] perror() (Phase 1)
- [ ] getline() (Phase 1)
- [ ] STDIN_FILENO constants (Phase 1)
- [ ] ENOTTY error code (Phase 1)
- [ ] Raw mode - tcsetraw/tcgetraw (Phase 2)
- [ ] ftruncate() (Phase 3)

## References

- [Kilo source](https://github.com/antirez/kilo)
- [Build Your Own Text Editor](https://viewsourcecode.org/snaptoken/kilo/) - kilo tutorial
- [VT100 escape codes](https://vt100.net/docs/vt100-ug/chapter3.html)
- [ANSI escape sequences](https://en.wikipedia.org/wiki/ANSI_escape_code)
