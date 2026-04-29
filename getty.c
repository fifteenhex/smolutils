// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

int main(int argc, char **argv, char **envp)
{
	const char *shell_path;
	const char *tty_path;

	if (argc != 3)
		return 1;

	tty_path = argv[1];
	shell_path = argv[2];

	debug("Starting getty on %s with shell %s\n",
		tty_path, shell_path);

        while (true) {
		pid_t pid = vfork();

                /* We are getty */
		if (pid) {
			wait(NULL);
		}
		/* We are the new process */
		else {
                        char * const newargv[] = {
                                "sh",
                                NULL
                        };
                        char *newenviron[] = { NULL };

                        execve(shell_path, newargv, newenviron);
                        printf("execve failed\n");
                        break;
                }
        }

	return 0;
}
