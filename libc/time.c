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

char *ctime_r(const time_t *timep, char *buf) {
	(void)timep;
	const char *stub = "Thu Jan  1 00:00:00 1970\n";
	char *p = buf;
	while (*stub) {
		*p++ = *stub++;
	}
	*p = '\0';
	return buf;
}
