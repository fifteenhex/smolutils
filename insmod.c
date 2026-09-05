// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "insmod"

#include "common.h"

#include "nolibc_extensions/modules.h"

#include <linux/module.h>

/* Detect file a module is compressed by extension */
static bool is_compressed(const char *path)
{
	const char *dot = strrchr(path, '.');

	if (!dot)
		return false;

	return !strcmp(dot, ".xz") ||
	       !strcmp(dot, ".gz") ||
	       !strcmp(dot, ".zst");
}

int main(int argc, char **argv, char **envp)
{
	int __cleanup_fd fd = -1;
	char params[128] = "";
	int flags = 0;
	int i;

	if (argc < 2) {
		usage("Usage: %s <module> [parameter=value ...]\n", argv[0]);
		return 1;
	}

	/* Build a single string wil all of the parameters */
	for (i = 2; i < argc; i++) {
		if (i > 2)
			strlcat(params, " ", sizeof(params));

		if (strlcat(params, argv[i], sizeof(params)) >= sizeof(params)) {
			error("Too many parameters\n");
			return 1;
		}
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		error("Failed to open %s: %d\n", argv[1], errno);
		return 1;
	}

	if (is_compressed(argv[1]))
		flags |= MODULE_INIT_COMPRESSED_FILE;

	if (finit_module(fd, params, flags)) {
		error("Failed to load %s: %d\n", argv[1], errno);
		return 1;
	}

	return 0;
}
