#ifndef KSTRING_H
#define KSTRING_H

int strncmp(const char *s1, const char *s2, unsigned int n);
char *strncpy(char *dst, const char *src, unsigned int n);
void *memmove(void *dst, const void *src, unsigned int n);
void *memset(void *s, int c, unsigned int n);

#endif
