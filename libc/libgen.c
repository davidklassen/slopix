#include <string.h>

#define PATH_MAX 1024

char *dirname(char *path) {
	static char buf[PATH_MAX];

	if (!path || !*path)
		return ".";

	size_t len = strlen(path);
	if (len >= PATH_MAX)
		len = PATH_MAX - 1;
	memcpy(buf, path, len);
	buf[len] = '\0';

	char *end = buf + len - 1;
	while (end > buf && *end == '/')
		end--;

	while (end > buf && *end != '/')
		end--;

	if (end == buf) {
		if (*end == '/')
			return "/";
		return ".";
	}

	while (end > buf && *end == '/')
		end--;

	*(end + 1) = '\0';
	return buf;
}

char *basename(char *path) {
	static char buf[PATH_MAX];

	if (!path || !*path)
		return ".";

	size_t len = strlen(path);
	if (len >= PATH_MAX)
		len = PATH_MAX - 1;
	memcpy(buf, path, len);
	buf[len] = '\0';

	char *end = buf + len - 1;
	while (end > buf && *end == '/')
		*end-- = '\0';

	if (buf[0] == '/' && buf[1] == '\0')
		return "/";

	char *base = strrchr(buf, '/');
	return base ? base + 1 : buf;
}
