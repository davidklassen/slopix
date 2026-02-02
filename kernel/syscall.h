#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_write     0
#define SYS_exit      1
#define SYS_read      2
#define SYS_sleep     3
#define SYS_getpid    4
#define SYS_fork      5
#define SYS_wait      6
#define SYS_exec      7
#define SYS_poll      8
#define SYS_poweroff  9
#define SYS_sbrk      10
#define SYS_open      11
#define SYS_close     12
#define SYS_fstat     13
#define SYS_dup	      14
#define SYS_mkdir     15
#define SYS_mknod     16
#define SYS_link      17
#define SYS_unlink    18
#define SYS_chdir     19
#define SYS_pipe      20
#define SYS_stat      21
#define SYS_getcwd    22
#define SYS_lseek     23
#define SYS_rename    24
#define SYS_mmap      25
#define SYS_munmap    26
#define SYS_kill      27
#define SYS_getprocs  28
#define SYS_getppid   29
#define SYS_waitpid   30
#define SYS_setpgid   31
#define SYS_getpgid   32
#define SYS_tcsetpgrp 33
#define SYS_tcgetpgrp 34
#define SYS_tcsetraw  35
#define SYS_tcgetraw  36
#define SYS_ftruncate 37
#define SYS_getdents  38
#define SYS_reboot    39

struct procinfo {
	int pid;
	int ppid;
	int state;
	char name[16];
};

struct trap_frame;
void syscall(struct trap_frame *tf);

#endif
