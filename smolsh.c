// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define MAX_CMDLINE 256
#define MAX_TOKENS 16

/* These aren't really builtin's, this just quicker lookup */
static const char builtin_ls[] = "ls";
static const char builtin_dmesg[] = "dmesg";
static const char builtin_cat[] = "cat";
static const char builtin_mkdir[] = "mkdir";

static const char ls_path[] = "/bin/ls";
static const char dmesg_path[] = "/bin/dmesg";
static const char cat_path[] = "/bin/cat";
static const char mkdir_path[] = "/bin/mkdir";

static void run_cmd(const char *bin)
{
	pid_t pid = vfork();

	/* We are smolsh */
	if (pid) {
		wait(NULL);
	}
	/* We are the new process */
	else {
		char * const newargv[] = {
			bin,
                        NULL
		};
		char *newenviron[] = { NULL };

		execve(bin, newargv, newenviron);
		printf("execve failed\n");
	}
}

static bool try_builtin(const char *cmdline, char **path)
{
	const char *_path = NULL;

	if (STARTS_WITH(cmdline, builtin_ls))
		_path = ls_path;
	else if (STARTS_WITH(cmdline, builtin_dmesg))
		_path = dmesg_path;

	if (_path) {
		*path = _path;
		return true;
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

int main (int argc, char **argv, char **envp)
{
	char line[MAX_CMDLINE];
	char *tokens[MAX_TOKENS];
	unsigned num_tokens;

	while (1) {
		char *path;
		char *cmd;
		int len;

		printf("smolsh> ");
		len = read(STDIN_FILENO, &line, ARRAY_SIZE(line) - 1);

		/* Terminate the end of the string, this should be \n */
		line[len - 1] = '\0';

		debug("Got command line: \"%s\"\n", line);

		toktoktok(line, len, tokens, ARRAY_SIZE(tokens), &num_tokens);

		/* mm */
		if (!num_tokens)
			continue;

		cmd = tokens[0];

		if (try_builtin(cmd, &path)) {
			run_cmd(path);
		}
		else
			printf("Sorry, don't know how to: \"%s\"\n", cmd);
	}

	return 0;
}
