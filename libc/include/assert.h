#ifndef ASSERT_H
#define ASSERT_H

#include <stdio.h>
#include <unistd.h>

#define assert(expr)                                                      \
	do {                                                              \
		if (!(expr)) {                                            \
			fprintf(stderr, "assertion failed: %s\n", #expr); \
			_exit(1);                                         \
		}                                                         \
	} while (0)

#endif
