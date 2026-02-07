#include <stdio.h>
#include <time.h>

int main(void) {
	time_t t = time(0);
	char buf[32];
	ctime_r(&t, buf);
	// ctime_r produces "Sat Feb  7 06:41:39 2026\n"
	// Insert "UTC " before the year: "Sat Feb  7 06:41:39 UTC 2026\n"
	buf[19] = '\0';
	printf("%s UTC %s", buf, buf + 20);
	return 0;
}
