#ifndef LIBC_H
#define LIBC_H

typedef unsigned long size_t;

// Syscalls
long write(int fd, const void *buf, size_t len);
void exit(int status);
long read(int fd, void *buf, size_t len);
void sleep(unsigned long ticks);
int getpid(void);
int fork(void);
int wait(void);
int exec(const char *name);

// String functions
size_t strlen(const char *s);
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int strcmp(const char *s1, const char *s2);

#endif
