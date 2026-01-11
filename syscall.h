#ifndef SYSCALL_H
#define SYSCALL_H

// Syscall numbers (Linux ARM64 compatible)
#define SYS_exit      93
#define SYS_write     64
#define SYS_read      63
#define SYS_getpid    172

// Syscall dispatcher
void syscall_init(void);
void *syscall_handler(void *stack_ptr);

// Syscall implementations (stubs for now)
long sys_exit(int status);
long sys_write(int fd, const void *buf, unsigned long count);
long sys_read(int fd, void *buf, unsigned long count);
long sys_getpid(void);

#endif
