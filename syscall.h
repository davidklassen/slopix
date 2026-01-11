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

// Syscall implementations (extract args from context internally)
long sys_exit(void);
long sys_write(void);
long sys_read(void);
long sys_getpid(void);

#endif
