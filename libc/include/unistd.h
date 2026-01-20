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

#endif
