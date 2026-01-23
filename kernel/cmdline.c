#include "cmdline.h"
#include "string.h"

#define CMDLINE_MAX 256

static char cmdline_buf[CMDLINE_MAX];

void cmdline_init(const char *bootargs) {
	if (bootargs == 0) {
		cmdline_buf[0] = '\0';
		return;
	}

	unsigned int len = strlen(bootargs);
	if (len >= CMDLINE_MAX) {
		len = CMDLINE_MAX - 1;
	}
	memcpy(cmdline_buf, bootargs, len);
	cmdline_buf[len] = '\0';
}

const char *cmdline_get(const char *key) {
	unsigned int keylen = strlen(key);
	char *p = cmdline_buf;

	while (*p) {
		while (*p == ' ') {
			p++;
		}
		if (*p == '\0') {
			break;
		}

		if (strncmp(p, key, keylen) == 0 && p[keylen] == '=') {
			return p + keylen + 1;
		}

		while (*p && *p != ' ') {
			p++;
		}
	}

	return 0;
}
