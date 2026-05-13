// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "users.h"

int main(int argc, char **argv, char **envp)
{
	const char *shell_path;
	const char *tty_path;
	pid_t pid;

	if (argc != 3)
		return 1;

	tty_path = argv[1];
	shell_path = argv[2];

	debug("Starting getty on %s with shell %s\n",
		tty_path, shell_path);

	pid = vfork();

	if (pid == -1)
		return 1;

	if (!pid) {
		/* We are the new process */
		char * const newargv[] = {
			"sh",
			NULL
		};
		char *newenviron[] = { NULL };

		users_changeuser(SMOLUTILS_USERS_NORMAL_MIN,
				 SMOLUTILS_USERS_NORMAL_MIN);

		execve(shell_path, newargv, newenviron);
		error("execve failed: %d\n", errno);

		_exit(1);
	}

	/* We are still the getty, wait for the shell to exit */
	wait(NULL);

	debug("%s exited\n", tty_path);

	return 0;
}
