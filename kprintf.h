#ifndef KPRINTF_H
#define KPRINTF_H

void kprintf(const char *fmt, ...);
int ksnprintf(char *buf, int size, const char *fmt, ...);

#endif
