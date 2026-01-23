#ifndef UNISTD_H
#define UNISTD_H

typedef unsigned long size_t;

long write(int fd, const void *buf, size_t len);
long read(int fd, void *buf, size_t len);
void exit(int status);
void sleep(unsigned long ms);
int getpid(void);
int fork(void);
int wait(void);
int exec(const char *name);
int poll(int fd, long timeout_ms);
void poweroff(void);
void *sbrk(long n);
int open(const char *path, int flags);
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

#endif
