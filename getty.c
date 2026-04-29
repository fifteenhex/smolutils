// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

int main(int argc, char **argv, char **envp)
{
	const char *tty;

	if (argc != 3)
		return 1;

	tty = argv[1];

	debug("Starting getty on %s\n", tty);

	while (1) { }

	return 0;
}
