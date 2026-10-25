// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "inc/auth.h"
#include "inc/users.h"

#define SHELL_PATH "/bin/smolsh"

int main(int argc, char **argv, char **envp)
{
	char *newargv[] = { "sh", NULL };

	if (argc > 2 || (argc == 2 && strcmp(argv[1], "root"))) {
		usage("Usage: %s [root]\n", argv[0]);
		return EX_USAGE;
	}

	if (users_changeuser(0, 0)) {
		error("Failed to become root\n");
		return EX_OSERR;
	}

	if (auth_check("su")) {
		error("You shall not pass!\n");
		return EX_NOPERM;
	}

	execve(SHELL_PATH, newargv, envp);

	error("Failed to run %s\n", SHELL_PATH);

	return EX_OSERR;
}
