// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "users.h"

int main(int argc, char **argv, char **envp)
{
	const char *shell_path;
	const char *tty_path;
	int tty_fd;
	pid_t pid;

	if (argc != 3)
		return 1;

	tty_path = argv[1];
	shell_path = argv[2];

	debug("Starting getty on %s with shell %s\n",
		tty_path, shell_path);

	tty_fd = open(tty_path, O_RDWR);
	if (tty_fd < 0) {
		error("Failed to open TTY\n");
		return 1;
	}

	/* Wire up stdin, stdout, stderr */
	dup2(tty_fd, STDIN_FILENO);
	dup2(tty_fd, STDOUT_FILENO);
	dup2(tty_fd, STDERR_FILENO);
	close(tty_fd);

	/* Change the user, this is what login would do... */
	users_changeuser(SMOLUTILS_USERS_NORMAL_MIN,
			 SMOLUTILS_USERS_NORMAL_MIN);

	if (spawn_and_wait("sh", shell_path)) {
		error("Failed to spawn shell\n");
		return 1;
	}

	debug("%s exited\n", tty_path);

	return 0;
}
