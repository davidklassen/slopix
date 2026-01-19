#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_write  0
#define SYS_exit   1
#define SYS_read   2
#define SYS_sleep  3
#define SYS_getpid 4

struct trap_frame;
void syscall(struct trap_frame *tf);

#endif
