#ifndef STRING_H
#define STRING_H

typedef unsigned long size_t;

size_t strlen(const char *s);
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int strcmp(const char *s1, const char *s2);
int atoi(const char *s);
int itoa(int n, char *buf);

#endif
