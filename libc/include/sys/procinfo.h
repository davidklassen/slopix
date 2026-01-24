#ifndef SYS_PROCINFO_H
#define SYS_PROCINFO_H

struct procinfo {
	int pid;
	int ppid;
	int state;
	char name[16];
};

int getprocs(struct procinfo *buf, int max);

#endif
