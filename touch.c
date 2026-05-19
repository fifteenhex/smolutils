// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

int main (int argc, char **argv, char **envp)
{
	const char *path;
	int __cleanup_fd fd = -1;

	if (argc != 2)
		return 1;

	path = argv[1];

	/* File doesn't exist, try to create it */
	if (access(path, F_OK)) {
		fd = creat(path, 0644);
		if (fd < 0) {
			error("Failed to create file\n");
			return 1;
		}
	}
	/* File exists, update timestamp(s) */
	else {
		// TODO
	}

	return 0;
}
