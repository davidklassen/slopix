#ifndef STDLIB_H
#define STDLIB_H

typedef unsigned long size_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);

long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);

int mkstemp(char *templ);

int atexit(void (*func)(void));

#endif
