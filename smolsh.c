// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define MAX_CMDLINE 256
#define MAX_TOKENS 16

static bool keeprocking = true;

static void run_cmd(const char *bin, char * const *argv)
{
	char *newenviron[] = { NULL };
	bool killed = false;
	int ret;

	ret = spawn_and_wait_full(bin, argv, newenviron, &killed);
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

	if (argc != 2)
		return 1;

	newdir = argv[1];

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
	char tmp[1024];
	char *str;
	int len;

	if (argc != 2)
		return 1;

	str = argv[1];

	len = sprintf(tmp, "%s\n", str);

	write(stdout, tmp, len);

	return 0;
}

static int pwd_handler(int argc, char **argv, int stdout)
{
	char cwd[1024];

	getcwd(cwd, ARRAY_SIZE(cwd));

	printf("%s\n", cwd);

	return 0;
}


static int exit_handler(int argc, char **argv, int stdout)
{
	keeprocking = false;

	return 0;
}

struct builtin builtins[] = {
	{ "cd", cd_handler },
	{ "clear", clear_handler },
	{ "echo", echo_handler },
	{ "pwd", pwd_handler },
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

static int try_search_cb(const char *name, int dir, void *priv)
{
	const char *cmd = (const char *) priv;

	if (strcmp(name, cmd) == 0)
		return 1;

	return 0;
}

static bool try_search(const char *cmd, char **path)
{
	/* this should be PATH_MAX I guess.. seems like a waste of 4K */
	static char _path[1024];
	int ret;

	ret = iterate_dir("/bin", try_search_cb, (void *) cmd);

	if (ret > 0) {
		sprintf(_path, "/bin/%s", cmd);
		*path = _path;
		return true;
	}

	return false;
}

static int parse_handle_stdout_redirection(char *str,
					   char **stdout_path,
					   bool *append)
{
	char next;

	*append = false;

	/* terminate the string just in case */
	*str = '\0';

	/* str pointed at the first > which we just killed */
	str++;

	next = *str;

	if (next == '>') {
		verbose("redirection is appending\n");
		*append = true;
		str++;
	}

	/* fix me, this is just skipping spaces */
	while (*str == ' ') {
		verbose("skipping whitespace\n");
		str++;
	}

	verbose("str should now be the start of the path\n");

	*stdout_path = str;

	return 0;
}

static int toktoktok(char *str, size_t len,
		     char** tokens, unsigned max_tokens, unsigned *num_tokens,
		     char** stdout, bool *append)
{
	char *redirection_stdout = NULL;
	unsigned int token_count = 0;
	unsigned int token_len = 0;
	char *token_start;
	int i;

	for (i = 0; i < len; i++) {
		char ch = *str;

		/* Don't support this crazy stuff */
		if (ch == ';' ||
		    ch == '<' ||
		    ch == '\\' ||
		    ch == '$' ) {
			return -1;
		}

		/* Barely, just, support redirectly stdout */
		if (ch == '>') {
			int ret;

			// FIXME terminate previous token if needed because there wasn't white space
			*str = '\0';

			verbose("Redirection started at %d\n", i);
			ret = parse_handle_stdout_redirection(str, &redirection_stdout, append);
			if (ret)
				return ret;

			printf("oh crap, redirection! %s\n", redirection_stdout);
			break;
		}

		/* Normal part */
		if (ch != ' ' && ch != '\0') {
			/* Start of a new token */
			if (token_len == 0) {
				verbose("Token started at %d\n", i);
				token_start = str;
			}

			/* This char is part of the current token */
			token_len++;
		}
		else {
			/* Was in a token, token is done */
			if (token_len) {
				verbose("Token ended at %d\n", i);
				/* Terminate token */
				*str = '\0';
				token_len = 0;

				tokens[token_count] = token_start;
				token_count++;

				verbose("Token: %s\n", token_start);
			}
			// Junk white space?
		}

		str++;
	}

	*stdout = redirection_stdout;
	*num_tokens = token_count;

	return 0;
}

static void do_prompt(void)
{
	char cwd[1024];

	getcwd(cwd, ARRAY_SIZE(cwd));

	printf("smolsh %s > ", cwd);
}

int main (int argc, char **argv, char **envp)
{
	char line[MAX_CMDLINE];
	char *tokens[MAX_TOKENS + 1];
	char *stdout;
	unsigned num_tokens;
	int ret;

	while (keeprocking) {
		char *path;
		char *cmd;
		int len;
		/* For redirection */
		int _stdout = STDOUT_FILENO;
		int redirected_stdout __cleanup_fd = -1;
		bool append;

		do_prompt();

		len = read(STDIN_FILENO, &line, ARRAY_SIZE(line) - 1);
		if (len <= 0)
			break;

		/* Terminate the end of the string, this should be \n */
		line[len - 1] = '\0';

		verbose("Got command line: \"%s\"\n", line);

		ret = toktoktok(line, len,
				tokens, ARRAY_SIZE(tokens), &num_tokens,
				&stdout, &append);
		if (ret) {
			printf("Syntax error\n");
			continue;
		}

		if (stdout) {
			int flags = O_WRONLY | O_CREAT;

			/* If not appending, truncate any existing file */
			if (!append)
				flags |= O_TRUNC;

			redirected_stdout = open(stdout, flags, 0644);
			if (redirected_stdout < 0) {
				printf("failed to open file for redirection: %d\n", _stdout);
				continue;
			}

			/* If appending, wind the file to end so we append */
			if (append)
				lseek(redirected_stdout, 0, SEEK_END);

			_stdout = redirected_stdout;
		}

		/* No tokens? */
		if (!num_tokens)
			continue;

		cmd = tokens[0];

		/* We'll use the tokens as the argv, so add the terminator */
		tokens[num_tokens] = NULL;

		if (try_builtin(tokens, num_tokens, _stdout))
			continue;

		if (try_fixed(cmd, &path)) {
			run_cmd(path, tokens);
			continue;
		}

		ret = try_absolute(cmd, &path);
		if (ret) {
			if (ret == 1)
				run_cmd(path, tokens);
			else
				printf("ERROR xxx\n");

			continue;
		}

		if (try_search(cmd, &path)) {
			run_cmd(path, tokens);
			continue;
		}

		printf("Sorry, don't know how to: \"%s\"\n", cmd);
	}

	debug("Exiting\n");

	return 0;
}
