#ifndef STDIO_H
#define STDIO_H

#define BUFSIZ 1024
#define EOF    (-1)

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

int puts(const char *s);
int printf(const char *fmt, ...);
int fileno(FILE *stream);

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
int fflush(FILE *stream);
size_t fread(void *ptr, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
int fgetc(FILE *stream);
int fputc(int c, FILE *stream);

#endif
