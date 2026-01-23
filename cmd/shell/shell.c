#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAXARGS 10

static int readline(char *buf, int max) {
	int n = 0;
	while (n < max - 1) {
		char c;
		if (read(0, &c, 1) > 0) {
			if (c == '\r' || c == '\n') {
				write(1, "\n", 1);
				break;
			} else if (c == 127 || c == '\b') {
				if (n > 0) {
					n--;
					write(1, "\b \b", 3);
				}
			} else if (c >= ' ') {
				buf[n++] = c;
				write(1, &c, 1);
			}
		}
	}
	buf[n] = '\0';
	return n;
}

// Parse whitespace-separated tokens into argv
// Returns argc (number of tokens)
static int parseline(char *line, char **argv) {
	int argc = 0;
	char *p = line;

	while (*p && argc < MAXARGS - 1) {
		while (*p && isspace(*p)) {
			p++;
		}
		if (*p == '\0') {
			break;
		}

		argv[argc++] = p;

		while (*p && !isspace(*p)) {
			p++;
		}

		if (*p) {
			*p++ = '\0';
		}
	}
	argv[argc] = 0;
	return argc;
}

// cd [dir] - change directory (default: "/")
static int builtin_cd(int argc, char **argv) {
	const char *path = (argc > 1) ? argv[1] : "/";
	if (chdir(path) < 0) {
		printf("cd: %s: No such directory\n", path);
		return 1;
	}
	return 0;
}

// pwd - print working directory
static int builtin_pwd(int argc, char **argv) {
	(void)argc;
	(void)argv;
	char buf[128];
	if (getcwd(buf, sizeof(buf)) == 0) {
		printf("pwd: cannot get current directory\n");
		return 1;
	}
	printf("%s\n", buf);
	return 0;
}

// exit [status] - exit shell (default: 0)
static int builtin_exit(int argc, char **argv) {
	int status = 0;
	if (argc > 1) {
		status = atoi(argv[1]);
	}
	exit(status);
	return 0;
}

struct builtin {
	const char *name;
	int (*func)(int argc, char **argv);
};

static struct builtin builtins[] = {
    {"cd", builtin_cd},
    {"pwd", builtin_pwd},
    {"exit", builtin_exit},
    {0, 0},
};

// Check if command is a built-in and run it
// Returns: 1 if built-in was executed, 0 if not a built-in
static int run_builtin(int argc, char **argv) {
	if (argc == 0) {
		return 0;
	}

	for (struct builtin *b = builtins; b->name; b++) {
		if (strcmp(argv[0], b->name) == 0) {
			b->func(argc, argv);
			return 1;
		}
	}
	return 0;
}

int main(void) {
	if (fork() == 0) {
		exec("cursor_blink");
		exit(1);
	}

	char buf[128];
	char *argv[MAXARGS];

	printf("slopix> ");

	for (;;) {
		int n = readline(buf, sizeof(buf));
		if (n > 0) {
			int argc = parseline(buf, argv);

			if (argc > 0 && !run_builtin(argc, argv)) {
				int pid = fork();
				if (pid == 0) {
					exec(argv[0]);
					printf("%s: command not found\n", argv[0]);
					exit(1);
				}
				wait();
			}
		}
		printf("slopix> ");
	}
}
