#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
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
#define KEY_TAB	      264

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
static int last_exit_status = 0;

#define HISTORY_SIZE 16

static char history[HISTORY_SIZE][128];
static int history_count;

static void history_add(const char *line) {
	if (history_count > 0 && strcmp(history[history_count - 1], line) == 0) {
		return;
	}
	if (history_count >= HISTORY_SIZE) {
		memmove(history[0], history[1], (HISTORY_SIZE - 1) * 128);
		history_count = HISTORY_SIZE - 1;
	}
	strncpy(history[history_count], line, 127);
	history[history_count][127] = '\0';
	history_count++;
}

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
		int status;
		int pid = waitpid(jobs[i].pgid, &status, WNOHANG | WUNTRACED);
		if (pid > 0) {
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

static int encode_exit_status(int status) {
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	if (WIFSIGNALED(status)) {
		return 128 + WTERMSIG(status);
	}
	return 0;
}

static void expand_variables(char *buf, int bufsize) {
	char tmp[128];
	char *src = buf;
	char *dst = tmp;
	char *end = tmp + sizeof(tmp) - 1;

	while (*src && dst < end) {
		if (src[0] == '$' && src[1] == '?') {
			char num[12];
			snprintf(num, sizeof(num), "%d", last_exit_status);
			for (char *p = num; *p && dst < end; p++) {
				*dst++ = *p;
			}
			src += 2;
		} else {
			*dst++ = *src++;
		}
	}
	*dst = '\0';
	strncpy(buf, tmp, bufsize - 1);
	buf[bufsize - 1] = '\0';
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
		wait(NULL);
		int status;
		wait(&status);
		exit(WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128 + WTERMSIG(status)
										   : 0);
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
	if (c == '\t') {
		return KEY_TAB;
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

static void line_replace(char *buf, int *len, int *pos, const char *entry, int elen) {
	for (int i = 0; i < *pos; i++) {
		write(1, "\b", 1);
	}
	write(1, entry, elen);
	int oldlen = *len;
	for (int i = elen; i < oldlen; i++) {
		write(1, " ", 1);
	}
	for (int i = elen; i < oldlen; i++) {
		write(1, "\b", 1);
	}
	memcpy(buf, entry, elen);
	*len = elen;
	*pos = elen;
}

#define MAX_COMPLETIONS 64

struct builtin {
	const char *name;
	int (*func)(int argc, char **argv);
};

static struct builtin builtins[];

static int last_was_tab;

static void tab_complete(char *buf, int *len, int *pos) {
	static char matches[MAX_COMPLETIONS][NAME_MAX + 1];
	int match_count = 0;

	int token_start = *pos;
	while (token_start > 0 && buf[token_start - 1] != ' ') {
		token_start--;
	}
	int token_len = *pos - token_start;
	char token[128];
	memcpy(token, buf + token_start, token_len);
	token[token_len] = '\0';

	char dir[128];
	char prefix[128];
	int dir_len;
	char *last_slash = 0;
	for (int i = token_len - 1; i >= 0; i--) {
		if (token[i] == '/') {
			last_slash = token + i;
			break;
		}
	}

	if (last_slash) {
		dir_len = last_slash - token + 1;
		memcpy(dir, token, dir_len);
		dir[dir_len] = '\0';
		strncpy(prefix, last_slash + 1, sizeof(prefix) - 1);
		prefix[sizeof(prefix) - 1] = '\0';
	} else {
		strcpy(dir, ".");
		dir_len = 0;
		strncpy(prefix, token, sizeof(prefix) - 1);
		prefix[sizeof(prefix) - 1] = '\0';
	}
	int prefix_len = strlen(prefix);

	int first_word = (token_start == 0);
	int has_slash = (last_slash != 0);

	if (first_word && !has_slash) {
		for (struct builtin *b = builtins; b->name; b++) {
			if (strncmp(b->name, prefix, prefix_len) == 0 && match_count < MAX_COMPLETIONS) {
				strncpy(matches[match_count], b->name, NAME_MAX);
				matches[match_count][NAME_MAX] = '\0';
				match_count++;
			}
		}
		DIR *dp = opendir("/bin");
		if (dp) {
			struct dirent *de;
			while ((de = readdir(dp)) != 0) {
				if (de->d_name[0] == '.') {
					continue;
				}
				if (strncmp(de->d_name, prefix, prefix_len) == 0 && match_count < MAX_COMPLETIONS) {
					int dup = 0;
					for (int i = 0; i < match_count; i++) {
						if (strcmp(matches[i], de->d_name) == 0) {
							dup = 1;
							break;
						}
					}
					if (!dup) {
						strncpy(matches[match_count], de->d_name, NAME_MAX);
						matches[match_count][NAME_MAX] = '\0';
						match_count++;
					}
				}
			}
			closedir(dp);
		}
	}

	DIR *dp = opendir(dir);
	if (dp) {
		struct dirent *de;
		while ((de = readdir(dp)) != 0) {
			if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
				continue;
			}
			if (strncmp(de->d_name, prefix, prefix_len) == 0 && match_count < MAX_COMPLETIONS) {
				int dup = 0;
				for (int i = 0; i < match_count; i++) {
					if (strcmp(matches[i], de->d_name) == 0) {
						dup = 1;
						break;
					}
				}
				if (!dup) {
					strncpy(matches[match_count], de->d_name, NAME_MAX);
					matches[match_count][NAME_MAX] = '\0';
					match_count++;
				}
			}
		}
		closedir(dp);
	}

	if (match_count == 0) {
		return;
	}

	char common[NAME_MAX + 1];
	strncpy(common, matches[0], NAME_MAX);
	common[NAME_MAX] = '\0';
	int common_len = strlen(common);
	for (int i = 1; i < match_count; i++) {
		int j = 0;
		while (j < common_len && matches[i][j] == common[j]) {
			j++;
		}
		common_len = j;
		common[common_len] = '\0';
	}

	if (match_count == 1) {
		char full[256];
		int flen;
		if (has_slash) {
			flen = snprintf(full, sizeof(full), "%s%s", dir, matches[0]);
		} else if (first_word) {
			flen = snprintf(full, sizeof(full), "%s", matches[0]);
		} else {
			flen = snprintf(full, sizeof(full), "%s%s", dir_len ? dir : "", matches[0]);
		}
		char path[256];
		if (has_slash) {
			snprintf(path, sizeof(path), "%s%s", dir, matches[0]);
		} else {
			snprintf(path, sizeof(path), "%s/%s", strcmp(dir, ".") == 0 ? "." : dir, matches[0]);
		}
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode) && flen < (int)sizeof(full) - 1) {
			full[flen++] = '/';
			full[flen] = '\0';
		}

		int old_tail = *len - *pos;
		for (int i = 0; i < *pos - token_start; i++) {
			write(1, "\b", 1);
		}
		write(1, full, flen);
		int old_from_token = *len - token_start;
		for (int i = flen; i < old_from_token; i++) {
			write(1, " ", 1);
		}
		for (int i = flen; i < old_from_token; i++) {
			write(1, "\b", 1);
		}
		if (old_tail > 0) {
			memmove(buf + token_start + flen, buf + *pos, old_tail);
			write(1, buf + token_start + flen, old_tail);
			for (int i = 0; i < old_tail; i++) {
				write(1, "\b", 1);
			}
		}
		memcpy(buf + token_start, full, flen);
		*len = token_start + flen + old_tail;
		*pos = token_start + flen;
		return;
	}

	if (common_len > prefix_len) {
		char full[256];
		int flen;
		if (has_slash) {
			flen = snprintf(full, sizeof(full), "%s%s", dir, common);
		} else {
			flen = snprintf(full, sizeof(full), "%s", common);
		}
		int old_tail = *len - *pos;
		for (int i = 0; i < *pos - token_start; i++) {
			write(1, "\b", 1);
		}
		write(1, full, flen);
		int old_from_token = *len - token_start;
		for (int i = flen; i < old_from_token; i++) {
			write(1, " ", 1);
		}
		for (int i = flen; i < old_from_token; i++) {
			write(1, "\b", 1);
		}
		if (old_tail > 0) {
			memmove(buf + token_start + flen, buf + *pos, old_tail);
			write(1, buf + token_start + flen, old_tail);
			for (int i = 0; i < old_tail; i++) {
				write(1, "\b", 1);
			}
		}
		memcpy(buf + token_start, full, flen);
		*len = token_start + flen + old_tail;
		*pos = token_start + flen;
		last_was_tab = 0;
		return;
	}

	if (!last_was_tab) {
		last_was_tab = 1;
		return;
	}

	write(1, "\n", 1);
	for (int i = 0; i < match_count; i++) {
		if (i > 0) {
			write(1, "  ", 2);
		}
		write(1, matches[i], strlen(matches[i]));
	}
	write(1, "\n", 1);
	write(1, "slopix> ", 8);
	write(1, buf, *len);
	for (int i = 0; i < *len - *pos; i++) {
		write(1, "\b", 1);
	}
}

static int readline(char *buf, int max) {
	int len = 0;
	int pos = 0;
	int hidx = -1;
	char saved[128];
	int saved_len = 0;

	while (len < max - 1) {
		int key = read_key();
		if (key < 0) {
			write(2, "\n", 1);
			return 0;
		}

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
			if (hidx + 1 < history_count) {
				if (hidx == -1) {
					memcpy(saved, buf, len);
					saved_len = len;
				}
				hidx++;
				char *entry = history[history_count - 1 - hidx];
				line_replace(buf, &len, &pos, entry, strlen(entry));
			}
			break;

		case KEY_DOWN:
			if (hidx >= 0) {
				hidx--;
				if (hidx == -1) {
					line_replace(buf, &len, &pos, saved, saved_len);
				} else {
					char *entry = history[history_count - 1 - hidx];
					line_replace(buf, &len, &pos, entry, strlen(entry));
				}
			}
			break;

		case KEY_TAB:
			tab_complete(buf, &len, &pos);
			break;

		default:
			last_was_tab = 0;
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
	int status;
	int pid = waitpid(j->pgid, &status, WUNTRACED);
	tcsetpgrp(0, shell_pgid);

	if (pid > 0) {
		if (WIFSTOPPED(status)) {
			j->state = JOB_STOPPED;
			last_exit_status = 128 + WSTOPSIG(status);
			printf("\n[%d] stopped  %s\n", j->jid, j->cmd);
		} else {
			last_exit_status = encode_exit_status(status);
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
		return -1;
	}

	for (struct builtin *b = builtins; b->name; b++) {
		if (strcmp(argv[0], b->name) == 0) {
			return b->func(argc, argv);
		}
	}
	return -1;
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
		fflush(stdout);
		int n = readline(buf, sizeof(buf));
		if (n < 0) {
			printf("\n");
			exit(0);
		}
		if (n > 0) {
			history_add(buf);
			char cmdstr[64];
			strncpy(cmdstr, buf, 63);
			cmdstr[63] = '\0';

			expand_variables(buf, sizeof(buf));
			struct cmd *cmd = parsecmd(buf);
			if (cmd == 0) {
				printf("syntax error\n");
			} else {
				nulterminate(cmd);

				int builtin_status = -1;
				if (cmd->type == CMD_EXEC) {
					struct execcmd *ecmd = (struct execcmd *)cmd;
					if (ecmd->argv[0]) {
						int argc = 0;
						while (ecmd->argv[argc]) {
							argc++;
						}
						builtin_status = run_builtin(argc, ecmd->argv);
					} else {
						builtin_status = 0;
					}
				}
				if (builtin_status >= 0) {
					last_exit_status = builtin_status;
				}

				if (builtin_status < 0) {
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
						int status;
						waitpid(child_pid, &status, WUNTRACED);
						if (WIFSTOPPED(status)) {
							int jid = add_job(child_pid, cmdstr);
							printf("\n[%d] stopped  %s\n", jid, cmdstr);
						} else {
							last_exit_status = encode_exit_status(status);
						}
						tcsetpgrp(0, shell_pgid);
					}
				}
			}
		}
	}
}
