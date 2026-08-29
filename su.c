// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "users.h"

#define SHELL_PATH "/bin/smolsh"

int main(int argc, char **argv, char **envp)
{
	char *newargv[] = { "sh", NULL };

	if (argc > 2 || (argc == 2 && strcmp(argv[1], "root"))) {
		usage("Usage: %s [root]\n", argv[0]);
		return 1;
	}

	if (users_changeuser(0, 0)) {
		error("Failed to become root\n");
		return 1;
	}

	execve(SHELL_PATH, newargv, envp);

	error("Failed to run %s\n", SHELL_PATH);

	return 1;
}
