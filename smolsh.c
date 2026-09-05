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

static int try_absolute(const char *cmd, char **path)
{
	/* A bare name is for the search. Only something with a / is a path */
	if (!strchr(cmd, '/'))
		return 0;

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

#define DEFAULT_PATH		"/bin"
#define DEFAULT_PATH_ROOT	"/bin:/sbin"

static const char *shell_path(void)
{
	static const char *fallback;
	const char *env = getenv("PATH");

	if (env && *env)
		return env;

	/* Add sbin for root, the shells user shouldn't change */
	if (!fallback)
		fallback = getuid() ? DEFAULT_PATH : DEFAULT_PATH_ROOT;

	return fallback;
}

static bool try_search(const char *cmd, char **path)
{
	/* this should be PATH_MAX I guess.. seems like a waste of 4K */
	static char _path[1024];
	const char *at = shell_path();

	while (*at) {
		const char *end = strchr(at, ':');
		int len;

		if (!end)
			end = at + strlen(at);

		len = end - at;

		if (len && snprintf(_path, sizeof(_path), "%.*s/%s",
				    len, at, cmd) < (int) sizeof(_path) &&
		    !access(_path, X_OK)) {
			*path = _path;
			return true;
		}

		at = *end ? end + 1 : end;
	}

	return false;
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
		char *out = NULL;
		bool to_stderr = false;
		bool is_append = false;
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

		/*
		 * word is overwritten in place to make things extra confusing,
		 * reading a character in doesn't mean it will end up in the final
		 * word, see: escapes.
		 */
		if (!is_special(*p)) {
			word = p;
			out = p;

			while (*p) {
				/* A \ makes whatever is next just a character, including a \ */
				if (*p == '\\') {
					p++;

					/* We don't do continuations */
					if (!*p)
						return SEP_BAD;

					*out++ = *p++;
					continue;
				}

				if (*p == ' ' || is_special(*p))
					break;

				/* No expansion for now */
				if (*p == '$')
					return SEP_BAD;

				*out++ = *p++;
			}
		}

		/*
		 * Whatever stopped the word gets read now, because
		 * terminating the word lands on top of it
		 */
		op = is_special(*p) ? *p : '\0';
		is_append = op == '>' && p[1] == '>';

		if (*p)
			p += is_append ? 2 : 1;

		if (out)
			*out = '\0';

		if (word && op == '>' && strcmp(word, "2") == 0) {
			to_stderr = true;
			word = NULL;
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
			want_path = to_stderr ? STDERR_FILENO : STDOUT_FILENO;
			cmd->append[want_path] = is_append;
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

static void run_command(struct command *cmd, int in_fd, int out_fd)
{
	int fds[3] = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };
	int opened[3] = { -1, -1, -1 };
	char *path;
	int ret;
	int i;

	/* Nothing to run, so nothing to open either */
	if (!cmd->argc)
		return;

	if (in_fd >= 0)
		fds[STDIN_FILENO] = in_fd;

	if (out_fd >= 0)
		fds[STDOUT_FILENO] = out_fd;

	if (open_redirects(cmd, opened))
		goto out;

	/* A < or > on the command beats whatever it was handed */
	for (i = 0; i < 3; i++) {
		if (opened[i] >= 0)
			fds[i] = opened[i];
	}

	/* We'll use the words as the argv, so add the terminator */
	cmd->argv[cmd->argc] = NULL;

	if (try_builtin(cmd->argv, cmd->argc, fds[STDOUT_FILENO]))
		goto out;

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
		if (opened[i] >= 0)
			close(opened[i]);
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

		run_command(&cmd, -1, -1);
	}

	debug("Exiting\n");

	return exit_status;
}
