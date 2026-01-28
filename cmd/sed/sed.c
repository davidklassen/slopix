#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXLINE 1024

enum { CMD_NONE,
       CMD_SUBST,
       CMD_PRINT };

struct script {
	int cmd;
	char old[256];
	char new[256];
	int global;
	int print;
};

static int parse_script(const char *s, struct script *sc) {
	sc->cmd = CMD_NONE;
	sc->global = 0;
	sc->print = 0;

	if (s[0] == 'p' && s[1] == '\0') {
		sc->cmd = CMD_PRINT;
		return 0;
	}

	if (s[0] != 's') {
		return -1;
	}

	char delim = s[1];
	if (!delim) {
		return -1;
	}

	const char *p = s + 2;
	int i = 0;
	while (*p && *p != delim && i < 255) {
		sc->old[i++] = *p++;
	}
	sc->old[i] = '\0';

	if (*p != delim) {
		return -1;
	}
	p++;

	i = 0;
	while (*p && *p != delim && i < 255) {
		sc->new[i++] = *p++;
	}
	sc->new[i] = '\0';

	if (*p == delim) {
		p++;
	}

	while (*p) {
		if (*p == 'g') {
			sc->global = 1;
		} else if (*p == 'p') {
			sc->print = 1;
		}
		p++;
	}

	sc->cmd = CMD_SUBST;
	return 0;
}

static void substitute(char *line, struct script *sc) {
	char out[MAXLINE];
	char *src = line;
	char *dst = out;
	char *match;
	int oldlen = strlen(sc->old);
	int newlen = strlen(sc->new);

	if (oldlen == 0) {
		return;
	}

	while ((match = strstr(src, sc->old)) != NULL) {
		int prefix = match - src;
		if (dst - out + prefix + newlen >= MAXLINE - 1) {
			break;
		}
		memcpy(dst, src, prefix);
		dst += prefix;
		memcpy(dst, sc->new, newlen);
		dst += newlen;
		src = match + oldlen;
		if (!sc->global) {
			break;
		}
	}

	int remain = strlen(src);
	if (dst - out + remain < MAXLINE - 1) {
		strcpy(dst, src);
	} else {
		*dst = '\0';
	}
	strcpy(line, out);
}

static void process(int fd, struct script *sc, int suppress) {
	char buf[512];
	char line[MAXLINE];
	int linelen = 0;
	int n;

	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		for (int i = 0; i < n; i++) {
			if (buf[i] == '\n' || linelen >= MAXLINE - 1) {
				line[linelen] = '\0';
				if (sc->cmd == CMD_SUBST) {
					substitute(line, sc);
				}
				if (!suppress || sc->cmd == CMD_PRINT || sc->print) {
					printf("%s\n", line);
				}
				linelen = 0;
			} else {
				line[linelen++] = buf[i];
			}
		}
	}

	if (linelen > 0) {
		line[linelen] = '\0';
		if (sc->cmd == CMD_SUBST) {
			substitute(line, sc);
		}
		if (!suppress || sc->cmd == CMD_PRINT || sc->print) {
			printf("%s\n", line);
		}
	}
}

int main(int argc, char **argv) {
	int suppress = 0;
	int argidx = 1;

	if (argidx < argc && strcmp(argv[argidx], "-n") == 0) {
		suppress = 1;
		argidx++;
	}

	if (argidx >= argc) {
		printf("usage: sed [-n] script [file...]\n");
		exit(1);
	}

	struct script sc;
	if (parse_script(argv[argidx], &sc) < 0) {
		printf("sed: invalid script: %s\n", argv[argidx]);
		exit(1);
	}
	argidx++;

	if (argidx >= argc) {
		process(0, &sc, suppress);
	} else {
		for (int i = argidx; i < argc; i++) {
			int fd = open(argv[i], O_RDONLY);
			if (fd < 0) {
				printf("sed: cannot open %s\n", argv[i]);
				continue;
			}
			process(fd, &sc, suppress);
			close(fd);
		}
	}

	exit(0);
}
