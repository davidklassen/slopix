#ifndef CONSOLE_H
#define CONSOLE_H

void console_init(void);
int console_read(char *dst, int n);
int console_write(const char *src, int n);

#endif
