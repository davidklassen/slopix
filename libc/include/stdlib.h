#ifndef STDLIB_H
#define STDLIB_H

typedef unsigned long size_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifdef __chibicc__
#define __attribute__(x)
#endif

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);
void exit(int status) __attribute__((noreturn));
void _exit(int status) __attribute__((noreturn));

long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
long double strtold(const char *nptr, char **endptr);

int atoi(const char *s);

int mkstemp(char *templ);

int atexit(void (*func)(void));

char *getenv(const char *name);

#endif
