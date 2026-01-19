#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_write 0
#define SYS_exit  1

struct trap_frame;
void syscall(struct trap_frame *tf);

#endif
