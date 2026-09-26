// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "modules"

#include "common.h"

#include "multicall.h"

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

static int prog_insmod(int argc, char **argv, char **envp)
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

static int prog_rmmod(int argc, char **argv, char **envp)
{
	int flags = O_NONBLOCK;
	const char *name;
	char buf[64];
	char *dot;
	int c;

	while ((c = getopt(argc, argv, "f")) != -1) {
		switch (c) {
		case 'f':
			/* Force unload, I hope you know what you're doing... */
			flags |= O_TRUNC;
			break;

		default:
			usage("Usage: %s [-f] <module>\n", argv[0]);
			return 1;
		}
	}

	if (optind != argc - 1) {
		usage("Usage: %s [-f] <module>\n", argv[0]);
		return 1;
	}

	/* Module name can be the name or the file path */
	name = strrchr(argv[optind], '/');
	name = name ? name + 1 : argv[optind];

	if (strlcpy(buf, name, sizeof(buf)) >= sizeof(buf)) {
		error("Module name too long\n");
		return 1;
	}

	dot = strrchr(buf, '.');
	if (dot && !strcmp(dot, ".ko"))
		*dot = 0;

	/* Replace dots for underscores */
	for (dot = buf; *dot; dot++)
		if (*dot == '-')
			*dot = '_';

	if (delete_module(buf, flags)) {
		error("Failed to unload %s: %d\n", buf, errno);
		return 1;
	}

	return 0;
}

static const struct multicall_prog progs[] = {
	{ "insmod", prog_insmod },
	{ "rmmod", prog_rmmod },
};

int main (int argc, char **argv, char **envp)
{
	MULTICALL_DISPATCH(argv[0], progs);

	return 1;
}
