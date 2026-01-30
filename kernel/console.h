#ifndef CONSOLE_H
#define CONSOLE_H

void console_init(void);
int console_read(char *dst, int n);
int console_write(const char *src, int n);

void console_set_fg_pgid(int pgid);
int console_get_fg_pgid(void);

void console_set_raw(int raw);
int console_get_raw(void);

#endif
