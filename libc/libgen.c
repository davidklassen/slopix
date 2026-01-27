#include <string.h>

char *dirname(char *path) {
	static char dot[] = ".";

	if (!path || !*path) {
		return dot;
	}

	// Remove trailing slashes
	char *end = path + strlen(path) - 1;
	while (end > path && *end == '/') {
		end--;
	}

	// Find last slash before end
	while (end > path && *end != '/') {
		end--;
	}

	if (end == path) {
		if (*end == '/') {
			return "/";
		}
		return dot;
	}

	// Remove trailing slashes from directory
	while (end > path && *end == '/') {
		end--;
	}

	*(end + 1) = '\0';
	return path;
}

char *basename(char *path) {
	static char dot[] = ".";
	static char slash[] = "/";

	if (!path || !*path) {
		return dot;
	}

	// Remove trailing slashes
	char *end = path + strlen(path) - 1;
	while (end > path && *end == '/') {
		*end-- = '\0';
	}

	// Handle root directory
	if (path[0] == '/' && path[1] == '\0') {
		return slash;
	}

	// Find last slash
	char *base = strrchr(path, '/');
	return base ? base + 1 : path;
}
