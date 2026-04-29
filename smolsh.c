// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define MAX_CMDLINE 256

/* These aren't really builtin's, this just quick lookup */
static const char builtin_ls[] = "ls ";
static const char builtin_dmesg[] = "dmesg ";

static const char ls_path[] = "/bin/ls";
static const char dmesg_path[] = "/bin/dmesg";

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

static void try_builtin(const char *cmdline)
{
	const char *bin = NULL;

	if (STARTS_WITH(cmdline, builtin_ls))
		bin = ls_path;
	else if (STARTS_WITH(cmdline, builtin_dmesg))
		bin = dmesg_path;

	if (bin)
		run_cmd(bin);
}

int main (int argc, char **argv, char **envp)
{
	char ch[MAX_CMDLINE];


	while (1) {
		int len;

		printf("smolsh> ");
		len = read(STDIN_FILENO, &ch, ARRAY_SIZE(ch));

		/* Turn the last char, \n, into a space */
		ch[len -1] = ' ';

		debug("Got command line: \"%s\"", ch);

		try_builtin(ch);
	}

	return 0;
}
