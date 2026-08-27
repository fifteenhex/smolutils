// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

int main (int argc, char **argv, char **envp)
{
	unsigned long mode;
	char *end;
	int ret = 0;
	int i;

	if (argc < 3) {
		usage("usage: chmod <octal mode> <file>...\n");
		return 1;
	}

	/* Only the octal form, nobody needs u+x that badly */
	mode = strtoul(argv[1], &end, 8);
	if (end == argv[1] || *end != '\0' || mode > 07777) {
		error("Not a mode: %s\n", argv[1]);
		return 1;
	}

	for (i = 2; i < argc; i++) {
		if (chmod(argv[i], mode)) {
			error("chmod(%s) failed: %d\n", argv[i], errno);
			ret = 1;
		}
	}

	return ret;
}
