#ifndef STRING_H
#define STRING_H

unsigned long strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, unsigned long n);
char *strncpy(char *dst, const char *src, unsigned long n);
char *strchr(const char *s, int c);
void *memset(void *s, int c, unsigned long n);
void *memcpy(void *dst, const void *src, unsigned long n);
void *memmove(void *dst, const void *src, unsigned long n);
char *strstr(const char *haystack, const char *needle);

#endif
