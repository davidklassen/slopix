#ifndef CONSOLE_H
#define CONSOLE_H

void consoleinit(void);
int consoleread(char *dst, int n);
int consolewrite(const char *src, int n);

#endif
