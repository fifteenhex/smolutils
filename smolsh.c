// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "readln.h"

#include "nolibc_extensions/signal.h"

#define MAX_CMDLINE 256
#define MAX_TOKENS 16

static bool keeprocking = true;
static int exit_status;

/* Catch ctrl-c, don't restart syscall! */
static void handle_sigint(int sig)
{
}

static void setup_signals(void)
{
	struct sigaction act = {
		.sa_handler = handle_sigint,
	};

	if (sigaction(SIGINT, &act, NULL))
		verbose("Failed to setup signals: %d\n", errno);
}

static void run_cmd(const char *bin, char * const *argv, const int *fds)
{
	bool killed = false;
	int ret;

	ret = spawn_and_wait_redirect(bin, argv, environ, &killed, fds);
	if (ret) {
		if (killed)
			error("Killed by signal: %d\n", ret);
		else
			error("Exited with non-zero return code: %d\n", ret);
	}
}

/* Real builtins */
struct builtin {
	const char *cmd;
	int (*handler)(int argc, char **argv, int stdout);
};

static int cd_handler(int argc, char **argv, int stdout)
{
	char *newdir;
	int ret;

	if (argc > 2)
		return 1;

	newdir = (argc == 2) ? argv[1] : "/";

	ret = chdir(newdir);
	if (ret) {
		switch(errno) {
		case ENOENT:
			error("Directory does not exist\n");
			break;
		default:
			error("chdir() failed: %d\n", errno);
			break;
		}
		return 1;
	}

	return 0;
}

static int clear_handler(int argc, char **argv, int stdout)
{
	printf("\033[3J\033[H\033[2J");

	return 0;
}

static int echo_handler(int argc, char **argv, int stdout)
{
	char tmp[MAX_CMDLINE + 1];
	int len = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (i > 1)
			tmp[len++] = ' ';

		len += sprintf(tmp + len, "%s", argv[i]);
	}

	tmp[len++] = '\n';

	write(stdout, tmp, len);

	return 0;
}

static int pwd_handler(int argc, char **argv, int stdout)
{
	char cwd[1024];

	if (!getcwd(cwd, ARRAY_SIZE(cwd)))
		return 1;

	dprintf(stdout, "%s\n", cwd);

	return 0;
}

static int sleep_handler(int argc, char **argv, int stdout)
{
	// FIXME
	sleep(10);

	return 0;
}

static int exit_handler(int argc, char **argv, int stdout)
{
	if (argc > 1)
		exit_status = atoi(argv[1]);

	keeprocking = false;

	return 0;
}

struct builtin builtins[] = {
	{ "cd", cd_handler },
	{ "clear", clear_handler },
	{ "echo", echo_handler },
	{ "pwd", pwd_handler },
	{ "sleep", sleep_handler },
	{ "exit", exit_handler },
};

static bool try_builtin(char **tokens, unsigned num_tokens, int stdout)
{
	const char *cmd = tokens[0];
	int i;

	for (i = 0; i < ARRAY_SIZE(builtins); i++) {
		struct builtin *bi = &builtins[i];

		if (strcmp(cmd, bi->cmd) == 0) {
			bi->handler(num_tokens, tokens, stdout);
			return true;
		}
	}

	return false;
}

/*
 * These aren't really builtin's, we know where they are though
 * so don't bother doing any look up nonsense
 */
struct fixed_path {
	const char *cmd;
	const char *path;
};

struct fixed_path fixed[] = {
	{ "ls", "/bin/ls" },
	{ "dmesg", "/bin/dmesg" },
	{ "cat", "/bin/cat" },
	{ "mkdir", "/bin/mkdir" },
	{ "ps", "/bin/ps" },
};

static bool try_fixed(const char *cmd, char **path)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fixed); i++) {
		struct fixed_path *fp = &fixed[i];
		if (strcmp(cmd, fp->cmd) == 0) {
			*path = fp->path;
			return true;
		}
	}

	return false;
}

static int try_absolute(const char *cmd, char **path)
{
	/* Check if we can execute it ... */
	if (access(cmd, X_OK) == 0) {
		*path = cmd;
		return 1;
	}

	/* Path exists but we aren't allowed to execute */
	if (errno == EACCES)
		return -EPERM;

	return 0;
}

static bool try_search(const char *cmd, char **path)
{
	/* this should be PATH_MAX I guess.. seems like a waste of 4K */
	static char _path[1024];

	if (snprintf(_path, sizeof(_path), "/bin/%s", cmd) >= (int) sizeof(_path))
		return false;

	if (access(_path, X_OK))
		return false;

	*path = _path;

	return true;
}

/* One command and where its input and output should go */
struct command {
	char *argv[MAX_TOKENS + 1];
	unsigned argc;
	char *redir[3];
	bool append[3];
};

/* What ended a command */
enum sep {
	SEP_END,
	SEP_BAD,
};

static bool is_special(char ch)
{
	return ch == '<' || ch == '>' || ch == '|';
}

/*
 * Pick one command out of the line, chopping it up where it sits, and
 * leave *pos on whatever came after it.
 */
static enum sep parse_cmd(char **pos, struct command *cmd)
{
	int want_path = -1;
	char *p = *pos;

	memset(cmd, 0, sizeof(*cmd));

	while (true) {
		char *word = NULL;
		bool dbl = false;
		char op;

		while (*p == ' ')
			p++;

		if (*p == '|')
			return SEP_BAD;

		/* The end of the line */
		if (!*p) {
			*pos = p;

			/* A < or > with nothing after it isn't a command */
			return want_path < 0 ? SEP_END : SEP_BAD;
		}

		if (!is_special(*p)) {
			word = p;

			while (*p && *p != ' ' && !is_special(*p)) {
				/* Don't support this crazy stuff */
				if (*p == '\\' || *p == '$')
					return SEP_BAD;

				p++;
			}
		}

		/*
		 * Whatever stopped the word gets read now, because
		 * terminating the word lands on top of it
		 */
		op = is_special(*p) ? *p : '\0';
		dbl = op == '>' && p[1] == '>';

		if (*p) {
			*p = '\0';
			p += dbl ? 2 : 1;
		}

		if (word) {
			if (want_path >= 0) {
				cmd->redir[want_path] = word;
				want_path = -1;
			} else {
				if (cmd->argc >= MAX_TOKENS)
					return SEP_BAD;

				cmd->argv[cmd->argc++] = word;
			}
		} else if (want_path >= 0) {
			/* Two redirections in a row, nothing to open */
			return SEP_BAD;
		}

		if (op == '<')
			want_path = STDIN_FILENO;

		if (op == '>') {
			want_path = STDOUT_FILENO;
			cmd->append[want_path] = dbl;
		}
	}
}

/*
 * Point the command's fds at whatever it asked for. Anything that ends
 * up different to the fd it replaces is ours to close afterwards.
 */
static int open_redirects(struct command *cmd, int *fds)
{
	int i;

	for (i = 0; i < 3; i++) {
		int flags = O_WRONLY | O_CREAT;
		int fd;

		if (!cmd->redir[i])
			continue;

		if (i == STDIN_FILENO)
			flags = O_RDONLY;
		else
			flags |= cmd->append[i] ? O_APPEND : O_TRUNC;

		fd = open(cmd->redir[i], flags, 0644);
		if (fd < 0) {
			error("Failed to open %s: %d\n", cmd->redir[i], errno);
			return -1;
		}

		fds[i] = fd;
	}

	return 0;
}

static void run_command(struct command *cmd)
{
	int fds[3] = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };
	char *path;
	int ret;
	int i;

	/* Nothing to run, so nothing to open either */
	if (!cmd->argc)
		return;

	if (open_redirects(cmd, fds))
		goto out;

	/* We'll use the words as the argv, so add the terminator */
	cmd->argv[cmd->argc] = NULL;

	if (try_builtin(cmd->argv, cmd->argc, fds[STDOUT_FILENO]))
		goto out;

	if (try_fixed(cmd->argv[0], &path)) {
		run_cmd(path, cmd->argv, fds);
		goto out;
	}

	ret = try_absolute(cmd->argv[0], &path);
	if (ret) {
		if (ret == 1)
			run_cmd(path, cmd->argv, fds);
		else
			error("%s: not executable\n", cmd->argv[0]);

		goto out;
	}

	if (try_search(cmd->argv[0], &path)) {
		run_cmd(path, cmd->argv, fds);
		goto out;
	}

	printf("Sorry, don't know how to: \"%s\"\n", cmd->argv[0]);

out:
	for (i = 0; i < 3; i++) {
		if (fds[i] != i)
			close(fds[i]);
	}
}

static void do_prompt(void)
{
	char cwd[1024];

	if (!getcwd(cwd, ARRAY_SIZE(cwd)))
		strcpy(cwd, "?");

	printf("smolsh %s > ", cwd);
}

int main (int argc, char **argv, char **envp)
{
	char line[MAX_CMDLINE];

	setup_signals();

	while (keeprocking) {
		struct command cmd;
		char *pos = line;
		int len;

		do_prompt();

		len = readln_shell(line, ARRAY_SIZE(line) - 1);

		/* ctrl-c got bashed, newline and start over */
		if (len == READLN_INTERRUPTED) {
			printf("\n");
			continue;
		}

		/* Nothing left to read or some other error */
		if (len < 0)
			break;

		verbose("Got command line: \"%s\"\n", line);

		if (parse_cmd(&pos, &cmd) == SEP_BAD) {
			printf("Syntax error\n");
			continue;
		}

		run_command(&cmd);
	}

	debug("Exiting\n");

	return exit_status;
}
