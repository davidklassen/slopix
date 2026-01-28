#ifndef __ASSERT_H
#define __ASSERT_H

void _assert_fail(const char *expr, const char *file, int line);

#define assert(expr) \
	((void)((expr) || (_assert_fail(#expr, __FILE__, __LINE__), 0)))

#endif
