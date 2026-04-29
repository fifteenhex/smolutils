// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define MAX_CMDLINE 256
#define MAX_TOKENS 16

static bool keeprocking = true;

static void run_cmd(const char *bin, char * const *argv)
{
	pid_t pid = vfork();

	/* We are smolsh */
	if (pid) {
		wait(NULL);
	}
	/* We are the new process */
	else {
		char *newenviron[] = { NULL };

		execve(bin, argv, newenviron);
		printf("execve failed\n");
	}
}

/* Real builtins */
struct builtin {
	const char *cmd;
	int (*handler)(int argc, char **argv);
};

static int cd_handler(int argc, char **argv)
{
	char *newdir = argv[1];

	chdir(newdir);

	return 0;
}

static int exit_handler(int argc, char **argv)
{
	keeprocking = false;

	return 0;
}

struct builtin builtins[] = {
	{ "cd", cd_handler },
	{ "exit", exit_handler },
};

static bool try_builtin(char **tokens, unsigned num_tokens)
{
	const char *cmd = tokens[0];
	int i;

	for (i = 0; i < ARRAY_SIZE(builtins); i++) {
		struct builtin *bi = &builtins[i];

		if (strcmp(cmd, bi->cmd) == 0) {
			bi->handler(num_tokens, tokens);
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

static void toktoktok(char *str, size_t len, char** tokens, unsigned max_tokens, unsigned *num_tokens)
{
	unsigned int token_count = 0;
	unsigned int token_len = 0;
	char *token_start;
	int i;

	for (i = 0; i < len; i++) {
		char ch = *str;

		if (ch != ' ' && ch != '\0') {
			/* Start of a new token */
			if (token_len == 0) {
				debug("Token started at %d\n", i);
				token_start = str;
			}

			/* This char is part of the current token */
			token_len++;
		}
		else {
			/* Was in a token, token is done */
			if (token_len) {
				debug("Token ended at %d\n", i);
				/* Terminate token */
				*str = '\0';
				token_len = 0;

				tokens[token_count] = token_start;
				token_count++;

				debug("Token: %s\n", token_start);
			}
			// Junk white space?
		}

		str++;
	}

	*num_tokens = token_count;
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
	unsigned num_tokens;

	while (keeprocking) {
		char *path;
		char *cmd;
		int len;

		do_prompt();

		len = read(STDIN_FILENO, &line, ARRAY_SIZE(line) - 1);

		/* Terminate the end of the string, this should be \n */
		line[len - 1] = '\0';

		debug("Got command line: \"%s\"\n", line);

		toktoktok(line, len, tokens, ARRAY_SIZE(tokens), &num_tokens);

		/* No tokens? */
		if (!num_tokens)
			continue;

		cmd = tokens[0];

		/* We'll use the tokens as the argv, so add the terminator */
		tokens[num_tokens] = NULL;

		if (try_builtin(tokens, num_tokens))
			continue;

		if (try_fixed(cmd, &path)) {
			run_cmd(path, tokens);
		}
		else
			printf("Sorry, don't know how to: \"%s\"\n", cmd);
	}

	debug("Exiting\n");

	return 0;
}
