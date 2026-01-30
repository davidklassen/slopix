#ifndef UNISTD_H
#define UNISTD_H

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef unsigned long size_t;
typedef long ssize_t;

#ifdef __chibicc__
#define __attribute__(x)
#endif

long write(int fd, const void *buf, size_t len);
long read(int fd, void *buf, size_t len);
void exit(int status) __attribute__((noreturn));
void _exit(int status) __attribute__((noreturn));
void sleep(unsigned long ms);
int getpid(void);
int fork(void);
int wait(int *wstatus);
int exec(const char *name);
int execvp(const char *file, char *const argv[]);
int poll(int fd, long timeout_ms);
void poweroff(void);
void *sbrk(long n);
int open(const char *path, int flags, ...);
int close(int fd);
int dup(int fd);
int mkdir(const char *path);
int mknod(const char *path, int major, int minor);
int link(const char *oldpath, const char *newpath);
int unlink(const char *path);
int chdir(const char *path);
int pipe(int fd[2]);
long lseek(int fd, long offset, int whence);
char *getcwd(char *buf, unsigned long size);
int rename(const char *oldpath, const char *newpath);
int kill(int pid, int sig);
int getppid(void);
int waitpid(int pid, int options);
int setpgid(int pid, int pgid);
int getpgid(int pid);
int tcsetpgrp(int fd, int pgid);
int tcgetpgrp(int fd);
int tcsetraw(int fd, int raw);
int tcgetraw(int fd);
int access(const char *path, int mode);

#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int isatty(int fd);

#endif
