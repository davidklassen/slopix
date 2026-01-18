#ifndef KPRINTF_H
#define KPRINTF_H

void kprintf(const char *fmt, ...);
int ksnprintf(char *buf, int size, const char *fmt, ...);
__attribute__((noreturn)) void kpanic(const char *fmt, ...);

#endif
