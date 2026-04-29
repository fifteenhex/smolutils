// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define MAX_CMDLINE 256

static void run_cmd(void)
{
	const char *bin = "/bin/ls";

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


int main (int argc, char **argv, char **envp)
{
	char ch[MAX_CMDLINE];


	while (1) {
		printf("smolsh> ");
		read(STDIN_FILENO, &ch, ARRAY_SIZE(ch));
		printf("%s", ch);

		run_cmd();
	}

	return 0;
}
