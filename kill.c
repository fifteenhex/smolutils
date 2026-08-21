// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

int main (int argc, char **argv, char **envp)
{
	int sig = SIGTERM;
	char *end;
	long pid;
	int i = 1;

	if (argc > 1 && argv[1][0] == '-') {
		sig = strtol(argv[1] + 1, &end, 10);
		if (*end != '\0')
			return 1;
		i++;
	}

	if ((argc - i) != 1)
		return 1;

	pid = strtol(argv[i], &end, 10);
	if (*end != '\0' || pid < 1)
		return 1;

	if (kill(pid, sig)) {
		error("kill() failed: %d\n", errno);
		return 1;
	}

	return 0;
}
