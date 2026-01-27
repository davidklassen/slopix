#ifndef TEST_H
#define TEST_H

#define ASSERT(x, y) assert(x, y, #y)

void assert(int expected, int actual, char *code);
int printf(char *fmt, ...);
void poweroff(void);

#endif
