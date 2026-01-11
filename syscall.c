#include "syscall.h"
#include "printf.h"
#include "process.h"
#include "uart.h"

// Syscall function pointer type (following xv6 pattern)
typedef long (*syscall_fn_t)(void);

// Syscall table (sparse array, most entries NULL)
static syscall_fn_t syscall_table[256] = {0};

// Current context pointer for argument extraction
static unsigned long *current_syscall_ctx = 0;

// Helper functions to extract syscall arguments from saved context
// Context frame layout: [0]=sp_el0, [1]=ttbr0_el1, [2]=x2, [3]=xzr, [4]=x3,
// [5]=x4, [6]=x5, [7]=x6, [8]=x7, [9]=x8, [10]=x9, ...
// [30]=x29, [31]=x30, [32]=ELR, [33]=SPSR, [34]=x0, [35]=x1

static unsigned long argraw(int n) {
    switch (n) {
        case 0: return current_syscall_ctx[34];  // x0
        case 1: return current_syscall_ctx[35];  // x1
        case 2: return current_syscall_ctx[2];   // x2
        case 3: return current_syscall_ctx[4];   // x3
        case 4: return current_syscall_ctx[5];   // x4
        case 5: return current_syscall_ctx[6];   // x5
        default: return 0;
    }
}

static int argint(int n) {
    return (int)argraw(n);
}

static void *argptr(int n) {
    return (void *)argraw(n);
}

void syscall_init(void) {
    syscall_table[SYS_exit] = sys_exit;
    syscall_table[SYS_write] = sys_write;
    syscall_table[SYS_read] = sys_read;
    syscall_table[SYS_getpid] = sys_getpid;

    printf("[SYSCALL] Syscall interface initialized (4 syscalls registered)\n");
}

// Syscall dispatcher - sets context and dispatches to handler
void *syscall_handler(void *stack_ptr) {
    unsigned long *ctx = (unsigned long *)stack_ptr;

    // Context frame layout (36 quad-words):
    // [0]=sp_el0, [1]=ttbr0_el1, [2]=x2, [3]=xzr, [4]=x3, [5]=x4,
    // [6]=x5, [7]=x6, [8]=x7, [9]=x8, [10]=x9, ...
    // [30]=x29, [31]=x30, [32]=ELR, [33]=SPSR, [34]=x0, [35]=x1

    unsigned long syscall_nr = ctx[9];   // x8
    long result = -1;  // Default: ENOSYS (not implemented)

    // Set current context for arg extraction
    current_syscall_ctx = ctx;

    if (syscall_nr < 256 && syscall_table[syscall_nr]) {
        syscall_fn_t handler = syscall_table[syscall_nr];
        result = handler();
    } else {
        printf("[SYSCALL] Unknown syscall number: %lu\n", syscall_nr);
    }

    // Clear context pointer
    current_syscall_ctx = 0;

    // Store result in x0 position
    ctx[34] = result;

    return stack_ptr;
}

// Syscall implementations - extract args from context using helpers

long sys_exit(void) {
    int status = argint(0);
    printf("[SYSCALL] sys_exit(%d) - terminating process\n", status);
    process_exit();
    // Never reached - process_exit() calls WFE loop
    return 0;
}

long sys_write(void) {
    int fd = argint(0);
    const void *buf = argptr(1);
    unsigned long count = argraw(2);

    // Only support stdout (fd=1) and stderr (fd=2) for now
    if (fd != 1 && fd != 2) {
        printf("[SYSCALL] sys_write: invalid fd=%d\n", fd);
        return -9;  // EBADF: Bad file descriptor
    }

    // Write to UART character by character
    const char *str = (const char *)buf;
    for (unsigned long i = 0; i < count; i++) {
        uart_putchar(str[i]);
    }

    // Return number of bytes written
    return (long)count;
}

long sys_read(void) {
    int fd = argint(0);
    void *buf = argptr(1);
    unsigned long count = argraw(2);
    printf("[SYSCALL] sys_read(fd=%d, buf=%p, count=%lu) - stub, returning -1\n",
           fd, buf, count);
    return -1;
}

long sys_getpid(void) {
    process_t *current = process_get_current();
    long pid = current ? current->pid : 0;

    printf("[SYSCALL] sys_getpid() -> %ld\n", pid);
    return pid;
}
