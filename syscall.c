#include "syscall.h"
#include "printf.h"

// Syscall function pointer type
typedef long (*syscall_fn_t)(unsigned long, unsigned long, unsigned long,
                              unsigned long, unsigned long, unsigned long);

// Syscall table (sparse array, most entries NULL)
static syscall_fn_t syscall_table[256] = {0};

void syscall_init(void) {
    syscall_table[SYS_exit] = (syscall_fn_t)sys_exit;
    syscall_table[SYS_write] = (syscall_fn_t)sys_write;
    syscall_table[SYS_read] = (syscall_fn_t)sys_read;
    syscall_table[SYS_getpid] = (syscall_fn_t)sys_getpid;

    printf("[SYSCALL] Syscall interface initialized (4 syscalls registered)\n");
}

// Syscall dispatcher - extracts args from saved context and dispatches
void *syscall_handler(void *stack_ptr) {
    unsigned long *ctx = (unsigned long *)stack_ptr;

    // Context frame layout (36 quad-words):
    // [0]=sp_el0, [1]=ttbr0_el1, [2]=x2, [3]=xzr, [4]=x3, [5]=x4,
    // [6]=x5, [7]=x6, [8]=x7, [9]=x8, [10]=x9, ...
    // [30]=x29, [31]=x30, [32]=ELR, [33]=SPSR, [34]=x0, [35]=x1

    unsigned long syscall_nr = ctx[9];   // x8
    unsigned long arg0 = ctx[34];        // x0
    unsigned long arg1 = ctx[35];        // x1
    unsigned long arg2 = ctx[2];         // x2
    unsigned long arg3 = ctx[4];         // x3
    unsigned long arg4 = ctx[5];         // x4
    unsigned long arg5 = ctx[6];         // x5

    printf("[SYSCALL] Dispatcher: nr=%lu, args=[%lu, %lu, %lu, ...]\n",
           syscall_nr, arg0, arg1, arg2);

    long result = -1;  // Default: ENOSYS (not implemented)

    if (syscall_nr < 256 && syscall_table[syscall_nr]) {
        syscall_fn_t handler = syscall_table[syscall_nr];
        result = handler(arg0, arg1, arg2, arg3, arg4, arg5);
    } else {
        printf("[SYSCALL] Unknown syscall number: %lu\n", syscall_nr);
    }

    // Store result in x0 position
    ctx[34] = result;

    return stack_ptr;
}

// Stub implementations - return -1 (ENOSYS) for now

long sys_exit(int status) {
    printf("[SYSCALL] sys_exit(%d) - stub, returning -1\n", status);
    return -1;
}

long sys_write(int fd, const void *buf, unsigned long count) {
    printf("[SYSCALL] sys_write(fd=%d, buf=%p, count=%lu) - stub, returning -1\n",
           fd, buf, count);
    return -1;
}

long sys_read(int fd, void *buf, unsigned long count) {
    printf("[SYSCALL] sys_read(fd=%d, buf=%p, count=%lu) - stub, returning -1\n",
           fd, buf, count);
    return -1;
}

long sys_getpid(void) {
    printf("[SYSCALL] sys_getpid() - stub, returning -1\n");
    return -1;
}
