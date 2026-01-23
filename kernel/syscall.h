#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_write    0
#define SYS_exit     1
#define SYS_read     2
#define SYS_sleep    3
#define SYS_getpid   4
#define SYS_fork     5
#define SYS_wait     6
#define SYS_exec     7
#define SYS_poll     8
#define SYS_poweroff 9
#define SYS_sbrk     10
#define SYS_open     11
#define SYS_close    12
#define SYS_fstat    13
#define SYS_dup	     14
#define SYS_mkdir    15
#define SYS_mknod    16
#define SYS_link     17
#define SYS_unlink   18
#define SYS_chdir    19

struct trap_frame;
void syscall(struct trap_frame *tf);

#endif
