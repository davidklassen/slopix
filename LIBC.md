# Libc Extensions Roadmap (Phase 2 of CC.md)

A detailed implementation plan for extending Slopix libc to support running chibicc natively.

## Overview

**Goal**: Extend Slopix libc so chibicc can run natively on Slopix (Phase 5 of CC.md).

**Scope**: This roadmap covers Phase 2 only - adding libc functions that chibicc depends on:
- Memory allocation (malloc, calloc, realloc, free)
- FILE* abstraction with buffered I/O
- open_memstream (critical for chibicc's `format()` function)
- Additional string and utility functions

**Current State**: Slopix libc already has syscalls, basic printf, string functions (strlen, strcmp, strcpy, strcat, strchr, strstr, memset, memcpy, memmove, memcmp, atoi), and basic ctype (isspace, isdigit, isalpha).

**Key References**:
- [Dan Luu's Malloc Tutorial](https://danluu.com/malloc-tutorial/)
- [open_memstream man page](https://man7.org/linux/man-pages/man3/open_memstream.3.html)
- [POSIX stdio specification](https://pubs.opengroup.org/onlinepubs/9699919799/functions/fopen.html)

---

## Chibicc Libc Usage Analysis

### Critical Functions (chibicc fails without these)

| Function | Usage Count | Used In |
|----------|-------------|---------|
| calloc | 37 | All files - primary allocation method |
| realloc | 5 | strings.c, tokenize.c, hashmap.c |
| open_memstream | 3 | strings.c (format), main.c, tokenize.c |
| vfprintf | 8 | codegen.c, strings.c, tokenize.c |
| fprintf | 50+ | Error messages, debug output |
| fopen/fclose | 10+ | File I/O |
| fread/fwrite | 5 | tokenize.c (read source files) |

### Functions Already in Slopix Libc

```
strcmp, strncmp, strlen, strcpy, strncpy, strcat, strchr, strstr
memcpy, memset, memmove, memcmp
isspace, isdigit, isalpha
atoi, itoa
printf, puts
fork, wait, exec, exit
open, close, read, write, lseek, stat, unlink, dup, pipe
sbrk, mmap, munmap
```

### Functions to Implement

| Category | Functions | Priority |
|----------|-----------|----------|
| Memory | malloc, calloc, realloc, free | Step 1 |
| Strings | strdup, strndup, strrchr, strtok | Step 2 |
| Numeric | strtol, strtoul | Step 3 |
| ctype | ispunct, isalnum, isxdigit, tolower, toupper | Step 4 |
| Error | errno, strerror | Step 5 |
| FILE basics | FILE struct, stdin, stdout, stderr | Step 6 |
| FILE I/O | fopen, fclose, fread, fwrite, fflush, fgetc, fputc | Step 7 |
| Formatted I/O | fprintf, vfprintf, sprintf, snprintf | Step 8 |
| Streams | open_memstream | Step 9 |
| Path | dirname, basename | Step 10 |
| Filesystem | mkstemp | Step 11 |
| Process | atexit, _exit | Step 12 |
| Time | time, localtime (stub) | Step 13 |

---

## Step 1: Memory Allocation

**Goal**: Implement malloc/calloc/realloc/free on top of sbrk.

### Design

Use a free-list allocator with block headers:

```c
typedef struct block_header {
    size_t size;              // Size of user data (not including header)
    struct block_header *next; // Next free block (only used when free)
    int free;                 // 1 if free, 0 if allocated
} block_header_t;

#define HEADER_SIZE sizeof(block_header_t)
#define ALIGN_TO(size, align) (((size) + (align) - 1) & ~((align) - 1))
#define MIN_ALLOC 16  // Minimum allocation size
```

### Algorithm

1. **malloc(size)**:
   - Search free list for first-fit block >= size
   - If found: mark as used, split if much larger, return pointer after header
   - If not found: extend heap with sbrk(size + HEADER_SIZE)
   - All allocations aligned to 16 bytes (AArch64 requirement)

2. **free(ptr)**:
   - Get header at (ptr - HEADER_SIZE)
   - Mark block as free
   - Add to free list
   - Coalesce with adjacent free blocks

3. **calloc(count, size)**:
   - Call malloc(count * size)
   - Zero the memory with memset

4. **realloc(ptr, size)**:
   - If ptr is NULL: return malloc(size)
   - If size is 0: free(ptr), return NULL
   - If current block large enough: return ptr
   - Otherwise: malloc new block, copy data, free old block

### Work

1. Create `libc/malloc.c`:
   ```c
   #include <unistd.h>
   #include <string.h>

   static block_header_t *free_list = NULL;
   static void *heap_end = NULL;

   void *malloc(size_t size);
   void free(void *ptr);
   void *calloc(size_t count, size_t size);
   void *realloc(void *ptr, size_t size);
   ```

2. Add declarations to `libc/include/stdlib.h` (create if needed)

3. Update `libc/Makefile` to include `malloc.o`

### Testing Strategy

Create `cmd/tests/test_malloc.c`:
```c
TEST(malloc_basic) {
    void *p = malloc(100);
    ASSERT_NOT_NULL(p, "malloc returned NULL");
    free(p);
    return 0;
}

TEST(calloc_zeroed) {
    int *p = calloc(10, sizeof(int));
    ASSERT_NOT_NULL(p, "calloc returned NULL");
    for (int i = 0; i < 10; i++)
        ASSERT_EQ(p[i], 0, "calloc not zeroed");
    free(p);
    return 0;
}

TEST(realloc_grow) {
    char *p = malloc(10);
    strcpy(p, "hello");
    p = realloc(p, 100);
    ASSERT_NOT_NULL(p, "realloc returned NULL");
    ASSERT_EQ(strcmp(p, "hello"), 0, "data corrupted");
    free(p);
    return 0;
}

TEST(malloc_multiple) {
    void *ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = malloc(i * 10 + 1);
        ASSERT_NOT_NULL(ptrs[i], "malloc failed");
    }
    for (int i = 0; i < 100; i++)
        free(ptrs[i]);
    return 0;
}
```

### Exit Criteria

1. malloc/calloc/realloc/free compile and link
2. All test_malloc tests pass on Slopix
3. Can allocate, use, and free memory repeatedly without crash
4. Allocations are 16-byte aligned

---

## Step 2: Additional String Functions

**Goal**: Implement strdup, strndup, strrchr, strtok.

### Work

Add to `libc/string.c`:

```c
char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *dup = malloc(len);
    if (dup)
        memcpy(dup, s, len);
    return dup;
}

char *strndup(const char *s, size_t n) {
    size_t len = strlen(s);
    if (len > n) len = n;
    char *dup = malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len);
        dup[len] = '\0';
    }
    return dup;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == c)
            last = s;
        s++;
    }
    if (c == '\0')
        return (char *)s;
    return (char *)last;
}

static char *strtok_state = NULL;

char *strtok(char *str, const char *delim) {
    if (str)
        strtok_state = str;
    if (!strtok_state)
        return NULL;

    // Skip leading delimiters
    while (*strtok_state && strchr(delim, *strtok_state))
        strtok_state++;

    if (*strtok_state == '\0') {
        strtok_state = NULL;
        return NULL;
    }

    char *token_start = strtok_state;

    // Find end of token
    while (*strtok_state && !strchr(delim, *strtok_state))
        strtok_state++;

    if (*strtok_state) {
        *strtok_state = '\0';
        strtok_state++;
    } else {
        strtok_state = NULL;
    }

    return token_start;
}
```

### Testing Strategy

Add tests to `cmd/tests/test_libc.c`:
```c
TEST(strdup_test) {
    char *s = strdup("hello");
    ASSERT_NOT_NULL(s, "strdup returned NULL");
    ASSERT_EQ(strcmp(s, "hello"), 0, "strdup content");
    free(s);
    return 0;
}

TEST(strrchr_test) {
    const char *s = "/path/to/file.txt";
    ASSERT_EQ(strcmp(strrchr(s, '/'), "/file.txt"), 0, "strrchr");
    ASSERT_NULL(strrchr(s, 'z'), "strrchr not found");
    return 0;
}

TEST(strtok_test) {
    char buf[] = "a,b,c";
    ASSERT_EQ(strcmp(strtok(buf, ","), "a"), 0, "first token");
    ASSERT_EQ(strcmp(strtok(NULL, ","), "b"), 0, "second token");
    ASSERT_EQ(strcmp(strtok(NULL, ","), "c"), 0, "third token");
    ASSERT_NULL(strtok(NULL, ","), "no more tokens");
    return 0;
}
```

### Exit Criteria

1. strdup, strndup, strrchr, strtok implemented
2. All string tests pass on Slopix
3. strdup correctly allocates and copies

---

## Step 3: Numeric Parsing

**Goal**: Implement strtol and strtoul.

### Work

Add to `libc/stdlib.c` (create if needed):

```c
unsigned long strtoul(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long result = 0;
    int neg = 0;

    // Skip whitespace
    while (isspace(*s)) s++;

    // Handle sign
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }

    // Auto-detect base
    if (base == 0) {
        if (*s == '0') {
            s++;
            if (*s == 'x' || *s == 'X') { base = 16; s++; }
            else { base = 8; }
        } else {
            base = 10;
        }
    } else if (base == 16 && *s == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    // Parse digits
    while (*s) {
        int digit;
        if (isdigit(*s))
            digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z')
            digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z')
            digit = *s - 'A' + 10;
        else
            break;

        if (digit >= base)
            break;

        result = result * base + digit;
        s++;
    }

    if (endptr)
        *endptr = (char *)s;

    return neg ? -result : result;
}

long strtol(const char *nptr, char **endptr, int base) {
    return (long)strtoul(nptr, endptr, base);
}
```

### Testing Strategy

```c
TEST(strtoul_decimal) {
    ASSERT_EQ(strtoul("123", NULL, 10), 123, "decimal");
    ASSERT_EQ(strtoul("  456", NULL, 10), 456, "leading space");
    return 0;
}

TEST(strtoul_hex) {
    ASSERT_EQ(strtoul("0xff", NULL, 0), 255, "auto hex");
    ASSERT_EQ(strtoul("ff", NULL, 16), 255, "explicit hex");
    return 0;
}

TEST(strtol_negative) {
    ASSERT_EQ(strtol("-42", NULL, 10), -42, "negative");
    return 0;
}
```

### Exit Criteria

1. strtol and strtoul work with bases 0, 8, 10, 16
2. Handle leading whitespace and sign
3. Tests pass on Slopix

---

## Step 4: Extended ctype Functions

**Goal**: Implement remaining character classification functions.

### Work

Add to `libc/ctype.c`:

```c
int ispunct(int c) {
    return (c >= '!' && c <= '/') ||
           (c >= ':' && c <= '@') ||
           (c >= '[' && c <= '`') ||
           (c >= '{' && c <= '~');
}

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int isxdigit(int c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int isupper(int c) {
    return c >= 'A' && c <= 'Z';
}

int islower(int c) {
    return c >= 'a' && c <= 'z';
}

int tolower(int c) {
    if (isupper(c))
        return c + ('a' - 'A');
    return c;
}

int toupper(int c) {
    if (islower(c))
        return c - ('a' - 'A');
    return c;
}
```

Update `libc/include/ctype.h` with declarations.

### Testing Strategy

```c
TEST(ispunct_test) {
    ASSERT(ispunct('!'), "! is punct");
    ASSERT(ispunct('.'), ". is punct");
    ASSERT(!ispunct('a'), "a not punct");
    ASSERT(!ispunct(' '), "space not punct");
    return 0;
}

TEST(isxdigit_test) {
    ASSERT(isxdigit('0'), "0 is xdigit");
    ASSERT(isxdigit('f'), "f is xdigit");
    ASSERT(isxdigit('F'), "F is xdigit");
    ASSERT(!isxdigit('g'), "g not xdigit");
    return 0;
}
```

### Exit Criteria

1. All ctype functions implemented
2. Tests pass on Slopix
3. tokenize.c's character classification works

---

## Step 5: Error Handling

**Goal**: Implement errno and strerror.

### Design

Slopix is single-threaded, so errno can be a simple global variable.

### Work

1. Create `libc/errno.c`:
   ```c
   int errno = 0;

   static const char *error_messages[] = {
       [0] = "Success",
       [1] = "Operation not permitted",
       [2] = "No such file or directory",
       [3] = "No such process",
       [4] = "Interrupted system call",
       [5] = "I/O error",
       [9] = "Bad file descriptor",
       [12] = "Out of memory",
       [13] = "Permission denied",
       [14] = "Bad address",
       [17] = "File exists",
       [20] = "Not a directory",
       [21] = "Is a directory",
       [22] = "Invalid argument",
       [28] = "No space left on device",
   };

   char *strerror(int errnum) {
       if (errnum >= 0 && errnum < sizeof(error_messages)/sizeof(error_messages[0])
           && error_messages[errnum])
           return (char *)error_messages[errnum];
       return "Unknown error";
   }
   ```

2. Create `libc/include/errno.h`:
   ```c
   #ifndef _ERRNO_H
   #define _ERRNO_H

   extern int errno;

   #define EPERM   1
   #define ENOENT  2
   #define ESRCH   3
   #define EINTR   4
   #define EIO     5
   #define EBADF   9
   #define ENOMEM  12
   #define EACCES  13
   #define EFAULT  14
   #define EEXIST  17
   #define ENOTDIR 20
   #define EISDIR  21
   #define EINVAL  22
   #define ENOSPC  28

   #endif
   ```

### Testing Strategy

```c
TEST(errno_test) {
    errno = 0;
    ASSERT_EQ(errno, 0, "errno initially 0");
    errno = ENOENT;
    ASSERT_EQ(errno, 2, "errno set");
    return 0;
}

TEST(strerror_test) {
    ASSERT(strcmp(strerror(0), "Success") == 0, "strerror 0");
    ASSERT(strcmp(strerror(ENOENT), "No such file or directory") == 0, "strerror ENOENT");
    return 0;
}
```

### Exit Criteria

1. errno global variable accessible
2. strerror returns appropriate messages
3. Error codes match POSIX values

---

## Step 6: FILE Structure

**Goal**: Define FILE struct and create stdin, stdout, stderr.

### Design

Minimal FILE structure for buffered I/O:

```c
#define BUFSIZ 1024

typedef struct _FILE {
    int fd;                 // Underlying file descriptor
    int flags;              // Mode flags (read/write/append)
    int error;              // Error indicator
    int eof;                // EOF indicator

    // Read buffer
    char *rbuf;             // Read buffer (NULL for unbuffered)
    int rbuf_size;          // Buffer size
    int rbuf_pos;           // Current position in buffer
    int rbuf_len;           // Valid data length in buffer

    // Write buffer
    char *wbuf;             // Write buffer
    int wbuf_size;          // Buffer size
    int wbuf_pos;           // Current position (amount of data)

    // For open_memstream
    char **memstream_ptr;   // Pointer to user's buffer pointer
    size_t *memstream_size; // Pointer to user's size variable
    int is_memstream;       // Flag for memstream mode
} FILE;

// Mode flags
#define _FILE_READ   0x01
#define _FILE_WRITE  0x02
#define _FILE_APPEND 0x04
#define _FILE_BINARY 0x08
#define _FILE_UNBUF  0x10
```

### Work

1. Update `libc/include/stdio.h`:
   ```c
   #ifndef _STDIO_H
   #define _STDIO_H

   #include <stddef.h>
   #include <stdarg.h>

   #define BUFSIZ 1024
   #define EOF (-1)

   typedef struct _FILE FILE;

   extern FILE *stdin;
   extern FILE *stdout;
   extern FILE *stderr;

   // Basic I/O (existing)
   int printf(const char *fmt, ...);
   int puts(const char *s);

   // FILE operations (new)
   FILE *fopen(const char *path, const char *mode);
   int fclose(FILE *stream);
   size_t fread(void *ptr, size_t size, size_t count, FILE *stream);
   size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
   int fflush(FILE *stream);
   int fgetc(FILE *stream);
   int fputc(int c, FILE *stream);
   int fputs(const char *s, FILE *stream);
   char *fgets(char *s, int size, FILE *stream);
   int fseek(FILE *stream, long offset, int whence);
   long ftell(FILE *stream);
   void rewind(FILE *stream);
   int feof(FILE *stream);
   int ferror(FILE *stream);
   void clearerr(FILE *stream);
   int fileno(FILE *stream);

   // Formatted I/O
   int fprintf(FILE *stream, const char *fmt, ...);
   int vfprintf(FILE *stream, const char *fmt, va_list ap);
   int sprintf(char *str, const char *fmt, ...);
   int snprintf(char *str, size_t size, const char *fmt, ...);
   int vsprintf(char *str, const char *fmt, va_list ap);
   int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

   // Memory streams
   FILE *open_memstream(char **ptr, size_t *sizeloc);

   #endif
   ```

2. Create `libc/stdio_file.c` with FILE struct definition and stdin/stdout/stderr initialization:
   ```c
   #include <stdio.h>
   #include <stdlib.h>
   #include <unistd.h>
   #include <fcntl.h>

   // Internal FILE structure (matches header)
   struct _FILE {
       int fd;
       int flags;
       int error;
       int eof;
       char *rbuf;
       int rbuf_size;
       int rbuf_pos;
       int rbuf_len;
       char *wbuf;
       int wbuf_size;
       int wbuf_pos;
       char **memstream_ptr;
       size_t *memstream_size;
       int is_memstream;
   };

   // Pre-allocated FILE structures for standard streams
   static FILE _stdin  = { .fd = 0, .flags = _FILE_READ };
   static FILE _stdout = { .fd = 1, .flags = _FILE_WRITE };
   static FILE _stderr = { .fd = 2, .flags = _FILE_WRITE | _FILE_UNBUF };

   FILE *stdin  = &_stdin;
   FILE *stdout = &_stdout;
   FILE *stderr = &_stderr;
   ```

### Testing Strategy

```c
TEST(stdio_streams) {
    ASSERT_NOT_NULL(stdin, "stdin exists");
    ASSERT_NOT_NULL(stdout, "stdout exists");
    ASSERT_NOT_NULL(stderr, "stderr exists");
    ASSERT_EQ(fileno(stdin), 0, "stdin fd");
    ASSERT_EQ(fileno(stdout), 1, "stdout fd");
    ASSERT_EQ(fileno(stderr), 2, "stderr fd");
    return 0;
}
```

### Exit Criteria

1. FILE struct defined in stdio.h
2. stdin, stdout, stderr initialized
3. fileno() returns correct fd

---

## Step 7: Basic FILE I/O

**Goal**: Implement fopen, fclose, fread, fwrite, fflush.

### Work

Add to `libc/stdio_file.c`:

```c
static FILE *file_alloc(void) {
    FILE *f = calloc(1, sizeof(FILE));
    return f;
}

FILE *fopen(const char *path, const char *mode) {
    int flags = 0;
    int fd_flags = 0;

    // Parse mode string
    if (mode[0] == 'r') {
        flags = _FILE_READ;
        fd_flags = O_RDONLY;
        if (mode[1] == '+') {
            flags |= _FILE_WRITE;
            fd_flags = O_RDWR;
        }
    } else if (mode[0] == 'w') {
        flags = _FILE_WRITE;
        fd_flags = O_WRONLY | O_CREAT | O_TRUNC;
        if (mode[1] == '+') {
            flags |= _FILE_READ;
            fd_flags = O_RDWR | O_CREAT | O_TRUNC;
        }
    } else if (mode[0] == 'a') {
        flags = _FILE_WRITE | _FILE_APPEND;
        fd_flags = O_WRONLY | O_CREAT | O_APPEND;
        if (mode[1] == '+') {
            flags |= _FILE_READ;
            fd_flags = O_RDWR | O_CREAT | O_APPEND;
        }
    } else {
        return NULL;
    }

    int fd = open(path, fd_flags);
    if (fd < 0)
        return NULL;

    FILE *f = file_alloc();
    if (!f) {
        close(fd);
        return NULL;
    }

    f->fd = fd;
    f->flags = flags;

    // Allocate buffers
    if (flags & _FILE_READ) {
        f->rbuf = malloc(BUFSIZ);
        f->rbuf_size = BUFSIZ;
    }
    if (flags & _FILE_WRITE) {
        f->wbuf = malloc(BUFSIZ);
        f->wbuf_size = BUFSIZ;
    }

    return f;
}

int fclose(FILE *stream) {
    if (!stream)
        return EOF;

    fflush(stream);

    int result = 0;
    if (stream->fd >= 0)
        result = close(stream->fd);

    if (stream->rbuf) free(stream->rbuf);
    if (stream->wbuf) free(stream->wbuf);

    // Don't free stdin/stdout/stderr
    if (stream != stdin && stream != stdout && stream != stderr)
        free(stream);

    return result;
}

int fflush(FILE *stream) {
    if (!stream || !(stream->flags & _FILE_WRITE))
        return 0;

    if (stream->is_memstream) {
        // Update user's pointer and size
        if (stream->memstream_ptr)
            *stream->memstream_ptr = stream->wbuf;
        if (stream->memstream_size)
            *stream->memstream_size = stream->wbuf_pos;
        return 0;
    }

    if (stream->wbuf_pos > 0) {
        long written = write(stream->fd, stream->wbuf, stream->wbuf_pos);
        if (written < 0) {
            stream->error = 1;
            return EOF;
        }
        stream->wbuf_pos = 0;
    }
    return 0;
}

size_t fread(void *ptr, size_t size, size_t count, FILE *stream) {
    if (!stream || !(stream->flags & _FILE_READ))
        return 0;

    size_t total = size * count;
    size_t nread = 0;
    char *buf = ptr;

    while (nread < total) {
        // Refill buffer if empty
        if (stream->rbuf_pos >= stream->rbuf_len) {
            long n = read(stream->fd, stream->rbuf, stream->rbuf_size);
            if (n <= 0) {
                if (n == 0) stream->eof = 1;
                else stream->error = 1;
                break;
            }
            stream->rbuf_pos = 0;
            stream->rbuf_len = n;
        }

        // Copy from buffer
        size_t avail = stream->rbuf_len - stream->rbuf_pos;
        size_t need = total - nread;
        size_t copy = (avail < need) ? avail : need;
        memcpy(buf + nread, stream->rbuf + stream->rbuf_pos, copy);
        stream->rbuf_pos += copy;
        nread += copy;
    }

    return nread / size;
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
    if (!stream || !(stream->flags & _FILE_WRITE))
        return 0;

    size_t total = size * count;
    const char *buf = ptr;

    for (size_t i = 0; i < total; i++) {
        if (fputc(buf[i], stream) == EOF)
            return i / size;
    }

    return count;
}

int fputc(int c, FILE *stream) {
    if (!stream || !(stream->flags & _FILE_WRITE))
        return EOF;

    // Handle memstream growth
    if (stream->is_memstream && stream->wbuf_pos >= stream->wbuf_size - 1) {
        size_t new_size = stream->wbuf_size * 2;
        char *new_buf = realloc(stream->wbuf, new_size);
        if (!new_buf)
            return EOF;
        stream->wbuf = new_buf;
        stream->wbuf_size = new_size;
    }

    // Add character to buffer
    stream->wbuf[stream->wbuf_pos++] = c;

    // Maintain null terminator for memstreams
    if (stream->is_memstream)
        stream->wbuf[stream->wbuf_pos] = '\0';

    // Flush on newline (line buffered) or full buffer
    if (!stream->is_memstream) {
        if (c == '\n' || stream->wbuf_pos >= stream->wbuf_size) {
            if (fflush(stream) < 0)
                return EOF;
        }
    }

    return (unsigned char)c;
}

int fgetc(FILE *stream) {
    if (!stream || !(stream->flags & _FILE_READ))
        return EOF;

    // Refill buffer if empty
    if (stream->rbuf_pos >= stream->rbuf_len) {
        long n = read(stream->fd, stream->rbuf, stream->rbuf_size);
        if (n <= 0) {
            if (n == 0) stream->eof = 1;
            else stream->error = 1;
            return EOF;
        }
        stream->rbuf_pos = 0;
        stream->rbuf_len = n;
    }

    return (unsigned char)stream->rbuf[stream->rbuf_pos++];
}
```

### Testing Strategy

Create `cmd/tests/test_stdio.c`:
```c
TEST(fopen_fclose) {
    FILE *f = fopen("/test.txt", "w");
    ASSERT_NOT_NULL(f, "fopen write");
    ASSERT_EQ(fclose(f), 0, "fclose");
    return 0;
}

TEST(fwrite_fread) {
    FILE *f = fopen("/test.txt", "w");
    ASSERT_NOT_NULL(f, "fopen write");
    char data[] = "hello world";
    ASSERT_EQ(fwrite(data, 1, 11, f), 11, "fwrite");
    fclose(f);

    f = fopen("/test.txt", "r");
    ASSERT_NOT_NULL(f, "fopen read");
    char buf[20] = {0};
    ASSERT_EQ(fread(buf, 1, 11, f), 11, "fread");
    ASSERT_EQ(strcmp(buf, "hello world"), 0, "content");
    fclose(f);
    return 0;
}
```

### Exit Criteria

1. fopen/fclose work with all modes
2. fread/fwrite handle buffering correctly
3. fflush writes pending data
4. Tests pass on Slopix

---

## Step 8: Formatted I/O

**Goal**: Implement fprintf, vfprintf, sprintf, snprintf.

### Design

Build on existing printf infrastructure:
- vfprintf writes to FILE* using fputc
- fprintf wraps vfprintf
- vsnprintf writes to a bounded buffer
- sprintf/snprintf wrap vsnprintf

### Work

Add to `libc/stdio_file.c`:

```c
int vfprintf(FILE *stream, const char *fmt, va_list ap) {
    int count = 0;

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            if (fputc(*p, stream) == EOF) return -1;
            count++;
            continue;
        }

        p++; // Skip '%'

        // Parse width
        int width = 0;
        int zero_pad = 0;
        if (*p == '0') { zero_pad = 1; p++; }
        while (isdigit(*p)) {
            width = width * 10 + (*p - '0');
            p++;
        }

        // Handle length modifiers
        int is_long = 0;
        if (*p == 'l') { is_long = 1; p++; }
        if (*p == 'l') { is_long = 2; p++; } // long long

        switch (*p) {
        case 'd': case 'i': {
            long val = is_long ? va_arg(ap, long) : va_arg(ap, int);
            char buf[32];
            int len = 0;
            int neg = 0;
            if (val < 0) { neg = 1; val = -val; }
            do { buf[len++] = '0' + val % 10; val /= 10; } while (val);
            if (neg) buf[len++] = '-';
            while (len < width) { fputc(zero_pad ? '0' : ' ', stream); count++; }
            while (len--) { fputc(buf[len], stream); count++; }
            break;
        }
        case 'u': {
            unsigned long val = is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
            char buf[32];
            int len = 0;
            do { buf[len++] = '0' + val % 10; val /= 10; } while (val);
            while (len < width) { fputc(zero_pad ? '0' : ' ', stream); count++; }
            while (len--) { fputc(buf[len], stream); count++; }
            break;
        }
        case 'x': case 'X': {
            unsigned long val = is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
            char buf[32];
            int len = 0;
            const char *digits = (*p == 'x') ? "0123456789abcdef" : "0123456789ABCDEF";
            do { buf[len++] = digits[val % 16]; val /= 16; } while (val);
            while (len < width) { fputc(zero_pad ? '0' : ' ', stream); count++; }
            while (len--) { fputc(buf[len], stream); count++; }
            break;
        }
        case 'p': {
            unsigned long val = (unsigned long)va_arg(ap, void *);
            fputc('0', stream); fputc('x', stream); count += 2;
            char buf[32];
            int len = 0;
            do { buf[len++] = "0123456789abcdef"[val % 16]; val /= 16; } while (val);
            while (len--) { fputc(buf[len], stream); count++; }
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            int len = strlen(s);
            while (len < width) { fputc(' ', stream); count++; width--; }
            while (*s) { fputc(*s++, stream); count++; }
            break;
        }
        case 'c': {
            int c = va_arg(ap, int);
            fputc(c, stream);
            count++;
            break;
        }
        case '%':
            fputc('%', stream);
            count++;
            break;
        default:
            fputc('%', stream);
            fputc(*p, stream);
            count += 2;
            break;
        }
    }

    return count;
}

int fprintf(FILE *stream, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int result = vfprintf(stream, fmt, ap);
    va_end(ap);
    return result;
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    // Create a fake FILE that writes to the string buffer
    struct _FILE fake = {
        .fd = -1,
        .flags = _FILE_WRITE,
        .wbuf = str,
        .wbuf_size = size - 1,  // Leave room for null terminator
        .wbuf_pos = 0,
        .is_memstream = 1,
    };

    int result = vfprintf(&fake, fmt, ap);
    if (str && size > 0)
        str[fake.wbuf_pos < size ? fake.wbuf_pos : size - 1] = '\0';
    return result;
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int result = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return result;
}

int sprintf(char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int result = vsnprintf(str, 0x7fffffff, fmt, ap);
    va_end(ap);
    return result;
}
```

### Testing Strategy

```c
TEST(fprintf_test) {
    FILE *f = fopen("/test.txt", "w");
    fprintf(f, "num=%d str=%s\n", 42, "hello");
    fclose(f);

    f = fopen("/test.txt", "r");
    char buf[50];
    fread(buf, 1, 50, f);
    ASSERT_EQ(strcmp(buf, "num=42 str=hello\n"), 0, "fprintf");
    fclose(f);
    return 0;
}

TEST(sprintf_test) {
    char buf[100];
    sprintf(buf, "%d + %d = %d", 2, 3, 5);
    ASSERT_EQ(strcmp(buf, "2 + 3 = 5"), 0, "sprintf");
    return 0;
}

TEST(snprintf_test) {
    char buf[10];
    int n = snprintf(buf, 10, "hello world");
    ASSERT_EQ(n, 11, "snprintf returns full length");
    ASSERT_EQ(strcmp(buf, "hello wor"), 0, "snprintf truncates");
    return 0;
}
```

### Exit Criteria

1. fprintf/vfprintf write to FILE*
2. sprintf/snprintf write to string buffer
3. Width specifiers work
4. All format specifiers (%d, %x, %s, %c, %p, %%) work
5. Tests pass on Slopix

---

## Step 9: open_memstream

**Goal**: Implement open_memstream for chibicc's format() function.

### Design

open_memstream creates a FILE* that:
- Writes to a dynamically growing buffer
- Updates caller's pointer and size on fflush/fclose
- Maintains a null terminator at the end

### Work

Add to `libc/stdio_file.c`:

```c
FILE *open_memstream(char **ptr, size_t *sizeloc) {
    if (!ptr || !sizeloc)
        return NULL;

    FILE *f = file_alloc();
    if (!f)
        return NULL;

    // Initial buffer size (64 bytes, same as many implementations)
    size_t initial_size = 64;
    f->wbuf = malloc(initial_size);
    if (!f->wbuf) {
        free(f);
        return NULL;
    }

    f->fd = -1;  // No underlying file descriptor
    f->flags = _FILE_WRITE;
    f->wbuf_size = initial_size;
    f->wbuf_pos = 0;
    f->wbuf[0] = '\0';  // Null terminator

    f->is_memstream = 1;
    f->memstream_ptr = ptr;
    f->memstream_size = sizeloc;

    // Initialize caller's variables
    *ptr = f->wbuf;
    *sizeloc = 0;

    return f;
}
```

The key behaviors already implemented in fputc and fclose:
- fputc grows buffer with realloc when full
- fputc maintains null terminator
- fflush updates *ptr and *sizeloc
- fclose calls fflush then updates pointers

### Testing Strategy

```c
TEST(open_memstream_basic) {
    char *buf;
    size_t size;
    FILE *f = open_memstream(&buf, &size);
    ASSERT_NOT_NULL(f, "open_memstream");

    fprintf(f, "hello");
    fflush(f);
    ASSERT_EQ(size, 5, "size after hello");
    ASSERT_EQ(strcmp(buf, "hello"), 0, "content");

    fprintf(f, " world");
    fclose(f);
    ASSERT_EQ(size, 11, "size after world");
    ASSERT_EQ(strcmp(buf, "hello world"), 0, "final content");

    free(buf);
    return 0;
}

TEST(open_memstream_grow) {
    char *buf;
    size_t size;
    FILE *f = open_memstream(&buf, &size);

    // Write more than initial buffer size
    for (int i = 0; i < 100; i++)
        fprintf(f, "x");

    fclose(f);
    ASSERT_EQ(size, 100, "size");
    ASSERT_EQ(strlen(buf), 100, "strlen");
    free(buf);
    return 0;
}

// Test chibicc's actual usage pattern
TEST(format_function_pattern) {
    char *buf;
    size_t buflen;
    FILE *out = open_memstream(&buf, &buflen);

    fprintf(out, "  mov x0, #%d\n", 42);
    fprintf(out, "  ret\n");
    fclose(out);

    ASSERT(strstr(buf, "mov x0, #42") != NULL, "format contains mov");
    ASSERT(strstr(buf, "ret") != NULL, "format contains ret");
    free(buf);
    return 0;
}
```

### Exit Criteria

1. open_memstream creates writable FILE*
2. Buffer grows automatically
3. Pointer and size updated on flush/close
4. Null terminator maintained
5. chibicc's format() function works
6. Tests pass on Slopix

---

## Step 10: Path Manipulation

**Goal**: Implement dirname and basename.

### Work

Create `libc/libgen.c`:

```c
#include <string.h>
#include <stdlib.h>

char *dirname(char *path) {
    static char dot[] = ".";

    if (!path || !*path)
        return dot;

    // Remove trailing slashes
    char *end = path + strlen(path) - 1;
    while (end > path && *end == '/')
        end--;

    // Find last slash before end
    while (end > path && *end != '/')
        end--;

    if (end == path) {
        if (*end == '/')
            return "/";
        return dot;
    }

    // Remove trailing slashes from directory
    while (end > path && *end == '/')
        end--;

    *(end + 1) = '\0';
    return path;
}

char *basename(char *path) {
    static char dot[] = ".";

    if (!path || !*path)
        return dot;

    // Remove trailing slashes
    char *end = path + strlen(path) - 1;
    while (end > path && *end == '/')
        *end-- = '\0';

    // Find last slash
    char *base = strrchr(path, '/');
    return base ? base + 1 : path;
}
```

Create `libc/include/libgen.h`:
```c
#ifndef _LIBGEN_H
#define _LIBGEN_H

char *dirname(char *path);
char *basename(char *path);

#endif
```

### Testing Strategy

```c
TEST(dirname_test) {
    char path1[] = "/usr/lib";
    ASSERT_EQ(strcmp(dirname(path1), "/usr"), 0, "dirname /usr/lib");

    char path2[] = "/usr/";
    ASSERT_EQ(strcmp(dirname(path2), "/"), 0, "dirname /usr/");

    char path3[] = "usr";
    ASSERT_EQ(strcmp(dirname(path3), "."), 0, "dirname usr");

    char path4[] = "/";
    ASSERT_EQ(strcmp(dirname(path4), "/"), 0, "dirname /");
    return 0;
}

TEST(basename_test) {
    char path1[] = "/usr/lib";
    ASSERT_EQ(strcmp(basename(path1), "lib"), 0, "basename /usr/lib");

    char path2[] = "/usr/";
    ASSERT_EQ(strcmp(basename(path2), "usr"), 0, "basename /usr/");
    return 0;
}
```

### Exit Criteria

1. dirname extracts directory component
2. basename extracts filename component
3. Handle edge cases (trailing slashes, no slash, root)
4. Tests pass on Slopix

---

## Step 11: Temporary Files

**Goal**: Implement mkstemp.

### Design

mkstemp creates a unique temporary file:
- Takes a template like "/tmp/tmpXXXXXX"
- Replaces XXXXXX with unique characters
- Opens the file and returns the fd

### Work

Add to `libc/stdlib.c`:

```c
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static unsigned int rand_state = 1;

static unsigned int simple_rand(void) {
    rand_state = rand_state * 1103515245 + 12345;
    return (rand_state >> 16) & 0x7fff;
}

int mkstemp(char *template) {
    size_t len = strlen(template);
    if (len < 6)
        return -1;

    char *suffix = template + len - 6;
    for (int i = 0; i < 6; i++) {
        if (suffix[i] != 'X')
            return -1;
    }

    static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    for (int attempt = 0; attempt < 100; attempt++) {
        for (int i = 0; i < 6; i++)
            suffix[i] = chars[simple_rand() % 62];

        int fd = open(template, O_RDWR | O_CREAT | O_EXCL);
        if (fd >= 0)
            return fd;
    }

    return -1;
}
```

### Testing Strategy

```c
TEST(mkstemp_test) {
    char template[] = "/tmpXXXXXX";
    int fd = mkstemp(template);
    ASSERT(fd >= 0, "mkstemp returns valid fd");
    ASSERT(template[4] != 'X', "template modified");

    // Write and read back
    write(fd, "test", 4);
    lseek(fd, 0, SEEK_SET);
    char buf[10] = {0};
    read(fd, buf, 4);
    ASSERT_EQ(strcmp(buf, "test"), 0, "file content");

    close(fd);
    unlink(template);
    return 0;
}
```

### Exit Criteria

1. mkstemp creates unique file
2. Returns valid file descriptor
3. Template is modified with unique name
4. File is opened for read/write
5. Tests pass on Slopix

---

## Step 12: Process Control

**Goal**: Implement atexit and _exit.

### Work

1. **_exit** - Already available via exit() syscall, but add explicit _exit:
   ```c
   // In libc/syscall.S - already exists as exit
   // In libc/include/unistd.h:
   void _exit(int status) __attribute__((noreturn));
   ```

2. **atexit** - Register functions to call at exit:
   ```c
   // libc/stdlib.c
   #define ATEXIT_MAX 32
   static void (*atexit_funcs[ATEXIT_MAX])(void);
   static int atexit_count = 0;

   int atexit(void (*func)(void)) {
       if (atexit_count >= ATEXIT_MAX)
           return -1;
       atexit_funcs[atexit_count++] = func;
       return 0;
   }

   // Called by exit() before syscall
   void __call_atexit(void) {
       while (atexit_count > 0)
           atexit_funcs[--atexit_count]();
   }
   ```

3. Modify exit to call atexit handlers:
   ```c
   // New wrapper in libc/stdlib.c
   void exit(int status) {
       __call_atexit();
       _exit(status);  // Calls the syscall directly
   }
   ```

### Testing Strategy

```c
static int atexit_called = 0;
static void atexit_handler(void) {
    atexit_called = 1;
    // Can't easily test this without forking
}

TEST(atexit_register) {
    ASSERT_EQ(atexit(atexit_handler), 0, "atexit returns 0");
    return 0;
}
```

### Exit Criteria

1. _exit terminates immediately
2. atexit registers handlers
3. exit() calls handlers in reverse order before terminating

---

## Step 13: Time Functions (Stubs)

**Goal**: Provide stub implementations for time functions chibicc includes but doesn't use.

### Work

Create `libc/time.c`:

```c
#include <time.h>

time_t time(time_t *tloc) {
    // Stub: return 0 (Unix epoch)
    time_t t = 0;
    if (tloc)
        *tloc = t;
    return t;
}

static struct tm static_tm;

struct tm *localtime(const time_t *timep) {
    // Stub: return zeroed struct
    static_tm = (struct tm){0};
    return &static_tm;
}
```

Create `libc/include/time.h`:
```c
#ifndef _TIME_H
#define _TIME_H

typedef long time_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

time_t time(time_t *tloc);
struct tm *localtime(const time_t *timep);

#endif
```

### Exit Criteria

1. time() and localtime() compile and link
2. Programs using them don't crash
3. Note: These are stubs - real implementation requires RTC hardware support

---

## Phase 2 Summary

### Implementation Order and Dependencies

```
Step 1: malloc/free     ← Foundation for everything
Step 2: strdup etc      ← Needs malloc
Step 3: strtol/strtoul  ← Independent
Step 4: ctype           ← Independent
Step 5: errno           ← Independent
Step 6: FILE struct     ← Needs malloc
Step 7: fopen/fread     ← Needs FILE, malloc
Step 8: fprintf         ← Needs FILE
Step 9: open_memstream  ← Needs FILE, realloc (CRITICAL for chibicc)
Step 10: dirname        ← Independent
Step 11: mkstemp        ← Needs open
Step 12: atexit         ← Independent
Step 13: time stubs     ← Independent
```

### Files to Create/Modify

| File | Action | Contents |
|------|--------|----------|
| libc/malloc.c | Create | malloc, calloc, realloc, free |
| libc/string.c | Modify | Add strdup, strndup, strrchr, strtok |
| libc/stdlib.c | Create | strtol, strtoul, mkstemp, atexit, exit |
| libc/ctype.c | Modify | Add ispunct, isalnum, isxdigit, etc. |
| libc/errno.c | Create | errno, strerror |
| libc/stdio_file.c | Create | FILE struct, fopen, fread, fprintf, open_memstream |
| libc/libgen.c | Create | dirname, basename |
| libc/time.c | Create | time, localtime stubs |
| libc/include/stdlib.h | Create | malloc, strtol, atexit declarations |
| libc/include/stdio.h | Modify | FILE, fopen, fprintf declarations |
| libc/include/errno.h | Create | errno, error codes |
| libc/include/libgen.h | Create | dirname, basename |
| libc/include/time.h | Create | time_t, struct tm |
| cmd/tests/test_malloc.c | Create | Memory allocation tests |
| cmd/tests/test_stdio.c | Create | FILE I/O tests |

### Test Files to Create

| Test File | Tests |
|-----------|-------|
| test_malloc.c | malloc, calloc, realloc, free, multiple allocs |
| test_stdio.c | fopen, fclose, fread, fwrite, fprintf, open_memstream |
| test_libc.c | Extend with strdup, strtol, errno tests |

### Exit Criteria (Phase 2 Complete)

1. **Memory**: malloc/calloc/realloc/free work correctly
2. **Strings**: strdup, strndup, strrchr, strtok implemented
3. **FILE I/O**: Complete FILE* abstraction with buffering
4. **open_memstream**: Critical function for chibicc works
5. **All tests pass**: New test suites pass on Slopix
6. **Integration test**: Can compile and run this program on Slopix:
   ```c
   #include <stdio.h>
   #include <stdlib.h>
   #include <string.h>

   int main() {
       char *buf;
       size_t size;
       FILE *f = open_memstream(&buf, &size);
       fprintf(f, "Hello %s!", "World");
       fclose(f);

       char *dup = strdup(buf);
       printf("%s (size=%zu)\n", dup, size);

       free(dup);
       free(buf);
       return 0;
   }
   ```

---

## References

- [POSIX stdio](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/stdio.h.html)
- [open_memstream](https://man7.org/linux/man-pages/man3/open_memstream.3.html)
- [Dan Luu's Malloc Tutorial](https://danluu.com/malloc-tutorial/)
- [Implementing malloc and free](https://medium.com/@andrestc/implementing-malloc-and-free-ba7e7704a473)
- [musl libc](https://musl.libc.org/) - Clean, minimal libc implementation
- [newlib](https://sourceware.org/newlib/) - Embedded systems libc
