#ifndef STRING_H
#define STRING_H

unsigned int strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, unsigned int n);
char *strncpy(char *dst, const char *src, unsigned int n);
char *strchr(const char *s, int c);
void *memset(void *s, int c, unsigned int n);
void *memcpy(void *dst, const void *src, unsigned int n);
void *memmove(void *dst, const void *src, unsigned int n);
char *strstr(const char *haystack, const char *needle);

#endif
