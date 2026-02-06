#include <stdio.h>
#include <sys/procinfo.h>

static const char *state_str(int state) {
	switch (state) {
	case 2:
	case 3:
		return "R";
	case 4:
		return "S";
	case 5:
		return "T";
	case 6:
		return "Z";
	default:
		return "?";
	}
}

int main(void) {
	struct procinfo procs[16];
	int n = getprocs(procs, 16);

	if (n < 0) {
		printf("ps: getprocs failed\n");
		return 1;
	}

	printf("  PID  PPID STATE NAME\n");
	for (int i = 0; i < n; i++) {
		printf("%5d %5d %5s %s\n",
		       procs[i].pid,
		       procs[i].ppid,
		       state_str(procs[i].state),
		       procs[i].name);
	}
	return 0;
}
