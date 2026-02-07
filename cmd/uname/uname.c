#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

int main(int argc, char **argv) {
	struct utsname buf;
	if (uname(&buf) < 0) {
		write(2, "uname: syscall failed\n", 22);
		return 1;
	}

	int show_s = 0, show_n = 0, show_r = 0, show_v = 0, show_m = 0;

	if (argc < 2) {
		show_s = 1;
	}

	for (int i = 1; i < argc; i++) {
		char *arg = argv[i];
		if (arg[0] == '-') {
			for (int j = 1; arg[j]; j++) {
				switch (arg[j]) {
				case 's':
					show_s = 1;
					break;
				case 'n':
					show_n = 1;
					break;
				case 'r':
					show_r = 1;
					break;
				case 'v':
					show_v = 1;
					break;
				case 'm':
					show_m = 1;
					break;
				case 'a':
					show_s = show_n = show_r = show_v = show_m = 1;
					break;
				default:
					write(2, "uname: unknown option\n", 22);
					return 1;
				}
			}
		}
	}

	int first = 1;
	if (show_s) {
		write(1, buf.sysname, strlen(buf.sysname));
		first = 0;
	}
	if (show_n) {
		if (!first) write(1, " ", 1);
		write(1, buf.nodename, strlen(buf.nodename));
		first = 0;
	}
	if (show_r) {
		if (!first) write(1, " ", 1);
		write(1, buf.release, strlen(buf.release));
		first = 0;
	}
	if (show_v) {
		if (!first) write(1, " ", 1);
		write(1, buf.version, strlen(buf.version));
		first = 0;
	}
	if (show_m) {
		if (!first) write(1, " ", 1);
		write(1, buf.machine, strlen(buf.machine));
		first = 0;
	}
	write(1, "\n", 1);
	return 0;
}
