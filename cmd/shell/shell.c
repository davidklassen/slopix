#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXARGS 10
#define MAXCMDS 4
#define MAXJOBS 8

#define CMD_EXEC  1
#define CMD_REDIR 2
#define CMD_PIPE  3

#define JOB_FREE    0
#define JOB_RUNNING 1
#define JOB_STOPPED 2
#define JOB_DONE    3

#define KEY_UP	      256
#define KEY_DOWN      257
#define KEY_RIGHT     258
#define KEY_LEFT      259
#define KEY_CTRL_C    260
#define KEY_CTRL_D    261
#define KEY_ENTER     262
#define KEY_BACKSPACE 263

struct cmd {
	int type;
};

struct execcmd {
	int type;
	char *argv[MAXARGS];
	char *eargv[MAXARGS];
};

struct redircmd {
	int type;
	struct cmd *cmd;
	char *file;
	char *efile;
	int mode;
	int fd;
};

struct pipecmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};

static struct execcmd exec_pool[MAXCMDS];
static struct redircmd redir_pool[MAXCMDS];
static struct pipecmd pipe_pool[MAXCMDS];
static int exec_next, redir_next, pipe_next;

struct job {
	int jid;
	int pgid;
	int state;
	int status;
	char cmd[64];
};

static struct job jobs[MAXJOBS];
static int next_jid = 1;
static int background;
static int shell_pgid;

static char *ps;
static char *es;

static int add_job(int pgid, const char *cmdstr) {
	for (int i = 0; i < MAXJOBS; i++) {
		if (jobs[i].state == JOB_FREE) {
			jobs[i].jid = next_jid++;
			jobs[i].pgid = pgid;
			jobs[i].state = JOB_RUNNING;
			jobs[i].status = 0;
			strncpy(jobs[i].cmd, cmdstr, 63);
			jobs[i].cmd[63] = '\0';
			return jobs[i].jid;
		}
	}
	return -1;
}

static struct job *find_job_by_jid(int jid) {
	for (int i = 0; i < MAXJOBS; i++) {
		if (jobs[i].state != JOB_FREE && jobs[i].jid == jid) {
			return &jobs[i];
		}
	}
	return 0;
}

static void remove_job(struct job *j) {
	j->state = JOB_FREE;
}

static void update_jobs(void) {
	for (int i = 0; i < MAXJOBS; i++) {
		if (jobs[i].state == JOB_FREE || jobs[i].state == JOB_DONE) {
			continue;
		}
		int ret = waitpid(jobs[i].pgid, WNOHANG | WUNTRACED);
		if (ret > 0) {
			int status = ret & 0xffff;
			if (WIFSTOPPED(status)) {
				jobs[i].state = JOB_STOPPED;
			} else {
				jobs[i].state = JOB_DONE;
				jobs[i].status = status;
			}
		}
	}
}

static void report_done_jobs(void) {
	for (int i = 0; i < MAXJOBS; i++) {
		if (jobs[i].state == JOB_DONE) {
			if (WIFSIGNALED(jobs[i].status)) {
				printf("[%d] killed  %s\n", jobs[i].jid, jobs[i].cmd);
			} else {
				printf("[%d] done    %s\n", jobs[i].jid, jobs[i].cmd);
			}
			remove_job(&jobs[i]);
		}
	}
}

static void reset_pools(void) {
	exec_next = redir_next = pipe_next = 0;
}

static struct cmd *execcmd_alloc(void) {
	if (exec_next >= MAXCMDS) {
		return 0;
	}
	struct execcmd *cmd = &exec_pool[exec_next++];
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = CMD_EXEC;
	return (struct cmd *)cmd;
}

static struct cmd *redircmd_alloc(struct cmd *subcmd, char *file, char *efile, int mode, int fd) {
	if (redir_next >= MAXCMDS) {
		return 0;
	}
	struct redircmd *cmd = &redir_pool[redir_next++];
	cmd->type = CMD_REDIR;
	cmd->cmd = subcmd;
	cmd->file = file;
	cmd->efile = efile;
	cmd->mode = mode;
	cmd->fd = fd;
	return (struct cmd *)cmd;
}

static struct cmd *pipecmd_alloc(struct cmd *left, struct cmd *right) {
	if (pipe_next >= MAXCMDS) {
		return 0;
	}
	struct pipecmd *cmd = &pipe_pool[pipe_next++];
	cmd->type = CMD_PIPE;
	cmd->left = left;
	cmd->right = right;
	return (struct cmd *)cmd;
}

static int peek(char *toks) {
	while (ps < es && isspace(*ps)) {
		ps++;
	}
	if (ps >= es) {
		return 0;
	}
	return strchr(toks, *ps) != 0;
}

static int gettoken(char **q, char **eq) {
	while (ps < es && isspace(*ps)) {
		ps++;
	}
	if (ps >= es) {
		return 0;
	}

	int tok = *ps;
	if (q) {
		*q = ps;
	}

	if (*ps == '>' && ps + 1 < es && *(ps + 1) == '>') {
		ps += 2;
		tok = '+';
	} else if (strchr("<>|", *ps)) {
		ps++;
	} else {
		tok = 'w';
		while (ps < es && !isspace(*ps) && !strchr("<>|&", *ps)) {
			ps++;
		}
	}

	if (eq) {
		*eq = ps;
	}
	return tok;
}

static struct cmd *parsepipe(void);
static struct cmd *parseexec(void);
static struct cmd *parseredirs(struct cmd *cmd);

static struct cmd *parsecmd(char *buf) {
	reset_pools();
	background = 0;
	ps = buf;
	es = buf + strlen(buf);

	struct cmd *cmd = parsepipe();
	if (cmd == 0) {
		return 0;
	}

	while (ps < es && isspace(*ps)) {
		ps++;
	}
	if (ps < es && *ps == '&') {
		background = 1;
		ps++;
	}
	while (ps < es && isspace(*ps)) {
		ps++;
	}
	if (ps < es) {
		return 0;
	}

	return cmd;
}

static struct cmd *parsepipe(void) {
	struct cmd *cmd = parseexec();
	if (cmd == 0) {
		return 0;
	}

	while (peek("|")) {
		gettoken(0, 0);
		struct cmd *right = parseexec();
		if (right == 0) {
			return 0;
		}
		cmd = pipecmd_alloc(cmd, right);
		if (cmd == 0) {
			return 0;
		}
	}
	return cmd;
}

static struct cmd *parseexec(void) {
	struct cmd *cmd = execcmd_alloc();
	if (cmd == 0) {
		return 0;
	}
	struct execcmd *ecmd = (struct execcmd *)cmd;

	cmd = parseredirs(cmd);
	if (cmd == 0) {
		return 0;
	}

	int argc = 0;
	while (!peek("<>|&")) {
		char *q, *eq;
		int tok = gettoken(&q, &eq);
		if (tok == 0) {
			break;
		}
		if (tok != 'w') {
			return 0;
		}
		if (argc >= MAXARGS - 1) {
			printf("too many args\n");
			return 0;
		}
		ecmd->argv[argc] = q;
		ecmd->eargv[argc] = eq;
		argc++;

		cmd = parseredirs(cmd);
		if (cmd == 0) {
			return 0;
		}
	}
	ecmd->argv[argc] = 0;
	ecmd->eargv[argc] = 0;
	return cmd;
}

static struct cmd *parseredirs(struct cmd *cmd) {
	while (peek("<>")) {
		int tok = gettoken(0, 0);
		char *q, *eq;
		if (gettoken(&q, &eq) != 'w') {
			return 0;
		}

		int fd, mode;
		if (tok == '<') {
			fd = 0;
			mode = O_RDONLY;
		} else if (tok == '>') {
			fd = 1;
			mode = O_WRONLY | O_CREAT | O_TRUNC;
		} else {
			fd = 1;
			mode = O_WRONLY | O_CREAT | O_APPEND;
		}

		cmd = redircmd_alloc(cmd, q, eq, mode, fd);
		if (cmd == 0) {
			return 0;
		}
	}
	return cmd;
}

static void nulterminate(struct cmd *cmd) {
	if (cmd == 0) {
		return;
	}

	switch (cmd->type) {
	case CMD_EXEC: {
		struct execcmd *ecmd = (struct execcmd *)cmd;
		for (int i = 0; ecmd->argv[i]; i++) {
			*ecmd->eargv[i] = '\0';
		}
		break;
	}
	case CMD_REDIR: {
		struct redircmd *rcmd = (struct redircmd *)cmd;
		*rcmd->efile = '\0';
		nulterminate(rcmd->cmd);
		break;
	}
	case CMD_PIPE: {
		struct pipecmd *pcmd = (struct pipecmd *)cmd;
		nulterminate(pcmd->left);
		nulterminate(pcmd->right);
		break;
	}
	}
}

static void runcmd(struct cmd *cmd) {
	if (cmd == 0) {
		exit(1);
	}

	switch (cmd->type) {
	case CMD_EXEC: {
		struct execcmd *ecmd = (struct execcmd *)cmd;
		if (ecmd->argv[0] == 0) {
			exit(0);
		}
		char cmdline[128];
		char pathbuf[32];
		int pos = 0;
		for (int i = 0; ecmd->argv[i] && pos < 127; i++) {
			if (i > 0 && pos < 127) {
				cmdline[pos++] = ' ';
			}
			char *arg = ecmd->argv[i];
			if (i == 0 && arg[0] != '/' && !strchr(arg, '/')) {
				strcpy(pathbuf, "/bin/");
				strncpy(pathbuf + 5, arg, sizeof(pathbuf) - 6);
				pathbuf[sizeof(pathbuf) - 1] = '\0';
				struct stat st;
				if (stat(pathbuf, &st) == 0) {
					for (char *p = pathbuf; *p && pos < 127; p++) {
						cmdline[pos++] = *p;
					}
					continue;
				}
				cmdline[pos++] = '/';
			}
			for (char *p = arg; *p && pos < 127; p++) {
				cmdline[pos++] = *p;
			}
		}
		cmdline[pos] = '\0';
		exec(cmdline);
		printf("%s: command not found\n", ecmd->argv[0]);
		exit(1);
	}
	case CMD_REDIR: {
		struct redircmd *rcmd = (struct redircmd *)cmd;
		close(rcmd->fd);
		int fd = open(rcmd->file, rcmd->mode);
		if (fd < 0) {
			printf("cannot open %s\n", rcmd->file);
			exit(1);
		}
		runcmd(rcmd->cmd);
		break;
	}
	case CMD_PIPE: {
		struct pipecmd *pcmd = (struct pipecmd *)cmd;
		int p[2];
		if (pipe(p) < 0) {
			printf("pipe failed\n");
			exit(1);
		}
		if (fork() == 0) {
			close(1);
			dup(p[1]);
			close(p[0]);
			close(p[1]);
			runcmd(pcmd->left);
		}
		if (fork() == 0) {
			close(0);
			dup(p[0]);
			close(p[0]);
			close(p[1]);
			runcmd(pcmd->right);
		}
		close(p[0]);
		close(p[1]);
		wait();
		wait();
		exit(0);
	}
	}
}

static int read_key(void) {
	char c;
	if (read(0, &c, 1) <= 0) {
		return -1;
	}

	if (c == '\r' || c == '\n') {
		return KEY_ENTER;
	}
	if (c == 127 || c == '\b') {
		return KEY_BACKSPACE;
	}
	if (c == 0x03) {
		return KEY_CTRL_C;
	}
	if (c == 0x04) {
		return KEY_CTRL_D;
	}

	if (c == 0x1b) {
		if (poll(0, 50) == 0) {
			return c;
		}
		if (read(0, &c, 1) <= 0 || c != '[') {
			return 0x1b;
		}
		if (poll(0, 50) == 0) {
			return 0x1b;
		}
		if (read(0, &c, 1) <= 0) {
			return 0x1b;
		}
		switch (c) {
		case 'A':
			return KEY_UP;
		case 'B':
			return KEY_DOWN;
		case 'C':
			return KEY_RIGHT;
		case 'D':
			return KEY_LEFT;
		}
		return 0x1b;
	}

	if (c >= ' ') {
		return c;
	}
	return -1;
}

static int readline(char *buf, int max) {
	int len = 0;
	int pos = 0;

	while (len < max - 1) {
		int key = read_key();

		switch (key) {
		case KEY_ENTER:
			write(1, "\n", 1);
			buf[len] = '\0';
			return len;

		case KEY_BACKSPACE:
			if (pos > 0) {
				memmove(buf + pos - 1, buf + pos, len - pos);
				len--;
				pos--;
				write(1, "\b", 1);
				write(1, buf + pos, len - pos);
				write(1, " \b", 2);
				for (int i = 0; i < len - pos; i++) {
					write(1, "\b", 1);
				}
			}
			break;

		case KEY_LEFT:
			if (pos > 0) {
				pos--;
				write(1, "\b", 1);
			}
			break;

		case KEY_RIGHT:
			if (pos < len) {
				write(1, &buf[pos], 1);
				pos++;
			}
			break;

		case KEY_CTRL_C:
			write(1, "^C\n", 3);
			return 0;

		case KEY_CTRL_D:
			if (len == 0) {
				return -1;
			}
			break;

		case KEY_UP:
		case KEY_DOWN:
			break;

		default:
			if (key >= ' ' && key < 256) {
				memmove(buf + pos + 1, buf + pos, len - pos);
				buf[pos] = key;
				len++;
				write(1, buf + pos, len - pos);
				pos++;
				for (int i = 0; i < len - pos; i++) {
					write(1, "\b", 1);
				}
			}
		}
	}
	buf[len] = '\0';
	return len;
}

static int builtin_cd(int argc, char **argv) {
	const char *path = (argc > 1) ? argv[1] : "/";
	if (chdir(path) < 0) {
		printf("cd: %s: No such directory\n", path);
		return 1;
	}
	return 0;
}

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

static int builtin_exit(int argc, char **argv) {
	int status = 0;
	if (argc > 1) {
		status = atoi(argv[1]);
	}
	exit(status);
	return 0;
}

static int builtin_jobs(int argc, char **argv) {
	(void)argc;
	(void)argv;
	for (int i = 0; i < MAXJOBS; i++) {
		if (jobs[i].state == JOB_FREE) {
			continue;
		}
		const char *state;
		switch (jobs[i].state) {
		case JOB_RUNNING:
			state = "running";
			break;
		case JOB_STOPPED:
			state = "stopped";
			break;
		case JOB_DONE:
			state = "done";
			break;
		default:
			state = "unknown";
		}
		printf("[%d] %s  %s\n", jobs[i].jid, state, jobs[i].cmd);
	}
	return 0;
}

static struct job *find_most_recent_job(int stopped_only) {
	struct job *best = 0;
	for (int i = 0; i < MAXJOBS; i++) {
		if (jobs[i].state == JOB_FREE || jobs[i].state == JOB_DONE) {
			continue;
		}
		if (stopped_only && jobs[i].state != JOB_STOPPED) {
			continue;
		}
		if (best == 0 || jobs[i].jid > best->jid) {
			best = &jobs[i];
		}
	}
	return best;
}

static int builtin_fg(int argc, char **argv) {
	struct job *j;
	if (argc > 1 && argv[1][0] == '%') {
		int jid = atoi(argv[1] + 1);
		j = find_job_by_jid(jid);
	} else {
		j = find_most_recent_job(0);
	}

	if (j == 0) {
		printf("fg: no current job\n");
		return 1;
	}

	printf("%s\n", j->cmd);

	if (j->state == JOB_STOPPED) {
		kill(-j->pgid, SIGCONT);
		j->state = JOB_RUNNING;
	}

	tcsetpgrp(0, j->pgid);
	int ret = waitpid(j->pgid, WUNTRACED);
	tcsetpgrp(0, shell_pgid);

	if (ret > 0) {
		int status = ret & 0xffff;
		if (WIFSTOPPED(status)) {
			j->state = JOB_STOPPED;
			printf("\n[%d] stopped  %s\n", j->jid, j->cmd);
		} else {
			remove_job(j);
		}
	}

	return 0;
}

static int builtin_bg(int argc, char **argv) {
	struct job *j;
	if (argc > 1 && argv[1][0] == '%') {
		int jid = atoi(argv[1] + 1);
		j = find_job_by_jid(jid);
	} else {
		j = find_most_recent_job(1);
	}

	if (j == 0 || j->state != JOB_STOPPED) {
		printf("bg: no stopped job\n");
		return 1;
	}

	printf("[%d] %s &\n", j->jid, j->cmd);
	kill(-j->pgid, SIGCONT);
	j->state = JOB_RUNNING;
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
    {"jobs", builtin_jobs},
    {"fg", builtin_fg},
    {"bg", builtin_bg},
    {0, 0},
};

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
	char buf[128];

	setpgid(0, 0);
	shell_pgid = getpid();
	tcsetpgrp(0, shell_pgid);

	for (;;) {
		poll(0, 10);
		update_jobs();
		report_done_jobs();
		printf("slopix> ");
		int n = readline(buf, sizeof(buf));
		if (n < 0) {
			printf("\n");
			exit(0);
		}
		if (n > 0) {
			char cmdstr[64];
			strncpy(cmdstr, buf, 63);
			cmdstr[63] = '\0';

			struct cmd *cmd = parsecmd(buf);
			if (cmd == 0) {
				printf("syntax error\n");
			} else {
				nulterminate(cmd);

				int builtin_handled = 0;
				if (cmd->type == CMD_EXEC) {
					struct execcmd *ecmd = (struct execcmd *)cmd;
					if (ecmd->argv[0]) {
						int argc = 0;
						while (ecmd->argv[argc]) {
							argc++;
						}
						builtin_handled = run_builtin(argc, ecmd->argv);
					} else {
						builtin_handled = 1;
					}
				}

				if (!builtin_handled) {
					int child_pid = fork();
					if (child_pid == 0) {
						setpgid(0, 0);
						runcmd(cmd);
					}
					setpgid(child_pid, child_pid);

					if (background) {
						int jid = add_job(child_pid, cmdstr);
						printf("[%d] %d\n", jid, child_pid);
					} else {
						tcsetpgrp(0, child_pid);
						int status = waitpid(child_pid, WUNTRACED);
						if (WIFSTOPPED(status)) {
							int jid = add_job(child_pid, cmdstr);
							printf("\n[%d] stopped  %s\n", jid, cmdstr);
						}
						tcsetpgrp(0, shell_pgid);
					}
				}
			}
		}
	}
}
