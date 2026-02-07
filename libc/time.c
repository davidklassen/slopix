#include <time.h>

time_t time(time_t *tloc) {
	time_t t = _time_syscall();
	if (tloc) {
		*tloc = t;
	}
	return t;
}

static int is_leap(int year) {
	return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int days_in_month(int mon, int year) {
	static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if (mon == 1 && is_leap(year)) {
		return 29;
	}
	return mdays[mon];
}

static struct tm static_tm;

struct tm *localtime(const time_t *timep) {
	time_t t = *timep;
	int secs = t % 86400;
	if (secs < 0) {
		secs += 86400;
	}
	static_tm.tm_hour = secs / 3600;
	static_tm.tm_min = (secs % 3600) / 60;
	static_tm.tm_sec = secs % 60;

	int days = t / 86400;
	// Jan 1 1970 was Thursday (wday=4)
	static_tm.tm_wday = ((days % 7) + 4) % 7;

	int year = 1970;
	for (;;) {
		int yd = is_leap(year) ? 366 : 365;
		if (days < yd) {
			break;
		}
		days -= yd;
		year++;
	}
	static_tm.tm_year = year - 1900;
	static_tm.tm_yday = days;

	int mon = 0;
	while (mon < 11 && days >= days_in_month(mon, year)) {
		days -= days_in_month(mon, year);
		mon++;
	}
	static_tm.tm_mon = mon;
	static_tm.tm_mday = days + 1;
	static_tm.tm_isdst = 0;

	return &static_tm;
}

static void fmt_int(char *buf, int val, int width) {
	for (int i = width - 1; i >= 0; i--) {
		buf[i] = '0' + (val % 10);
		val /= 10;
	}
}

char *ctime_r(const time_t *timep, char *buf) {
	static const char *wdays = "SunMonTueWedThuFriSat";
	static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";

	struct tm *tm = localtime(timep);
	char *p = buf;

	const char *w = wdays + tm->tm_wday * 3;
	*p++ = w[0];
	*p++ = w[1];
	*p++ = w[2];
	*p++ = ' ';

	const char *m = months + tm->tm_mon * 3;
	*p++ = m[0];
	*p++ = m[1];
	*p++ = m[2];
	*p++ = ' ';

	if (tm->tm_mday < 10) {
		*p++ = ' ';
		*p++ = '0' + tm->tm_mday;
	} else {
		*p++ = '0' + tm->tm_mday / 10;
		*p++ = '0' + tm->tm_mday % 10;
	}
	*p++ = ' ';

	fmt_int(p, tm->tm_hour, 2);
	p += 2;
	*p++ = ':';
	fmt_int(p, tm->tm_min, 2);
	p += 2;
	*p++ = ':';
	fmt_int(p, tm->tm_sec, 2);
	p += 2;
	*p++ = ' ';

	int year = tm->tm_year + 1900;
	fmt_int(p, year, 4);
	p += 4;
	*p++ = '\n';
	*p = '\0';

	return buf;
}
