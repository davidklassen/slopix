#include <time.h>

time_t time(time_t *tloc) {
	time_t t = 0;
	if (tloc) {
		*tloc = t;
	}
	return t;
}

static struct tm static_tm;

struct tm *localtime(const time_t *timep) {
	(void)timep;
	static_tm = (struct tm){0};
	return &static_tm;
}
