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

	if ((argc - i) != 1) {
		usage("usage: kill [-<signal>] <pid>\n");
		return 1;
	}

	pid = strtol(argv[i], &end, 10);

	/*
         * pid_t is 32 bits but the value we just parsed might get
         * truncated resulting in badness so we do a cast to check
         * for truncation.
         */
	if (*end != '\0' || pid < 1 || (long) (pid_t) pid != pid) {
		error("Not a pid: %s\n", argv[i]);
		return 1;
	}

	if (kill(pid, sig)) {
		error("kill() failed: %d\n", errno);
		return 1;
	}

	return 0;
}
