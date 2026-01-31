#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>

#define BUFSIZ 1024
#define EOF    (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define _FILE_READ   0x01
#define _FILE_WRITE  0x02
#define _FILE_APPEND 0x04
#define _FILE_BINARY 0x08
#define _FILE_UNBUF  0x10

typedef struct _FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

typedef unsigned long size_t;
typedef long ssize_t;

ssize_t getline(char **lineptr, size_t *n, FILE *stream);

int puts(const char *s);
int printf(const char *fmt, ...);
int fileno(FILE *stream);

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
int fflush(FILE *stream);
size_t fread(void *ptr, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
int fgetc(FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int feof(FILE *stream);

int fprintf(FILE *stream, const char *fmt, ...);
int vfprintf(FILE *stream, const char *fmt, va_list ap);
int sprintf(char *str, const char *fmt, ...);
int snprintf(char *str, size_t size, const char *fmt, ...);
int vsprintf(char *str, const char *fmt, va_list ap);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

FILE *open_memstream(char **ptr, size_t *sizeloc);

void perror(const char *s);

#endif
