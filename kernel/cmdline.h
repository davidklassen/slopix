#ifndef CMDLINE_H
#define CMDLINE_H

void cmdline_init(const char *bootargs);
const char *cmdline_get(const char *key);

#endif
