#ifndef CONSOLE_H
#define CONSOLE_H

void console_init(void);
int consoleread(char *dst, int n);
int consolewrite(const char *src, int n);

#endif
