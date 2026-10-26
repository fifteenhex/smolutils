// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "modules"

#include "common.h"
#include "inc/strings.h"

#include "inc/multicall.h"

#include <linux/module.h>

#define INSMOD_USAGE	"Usage: %s <module> [parameter=value ...]\n"
#define RMMOD_USAGE	"Usage: %s [-f] <module>\n"

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
		usage(INSMOD_USAGE, argv[0]);
		return EX_USAGE;
	}

	/* Build a single string wil all of the parameters */
	for (i = 2; i < argc; i++) {
		if (i > 2)
			strlcat(params, " ", sizeof(params));

		if (strlcat(params, argv[i], sizeof(params)) >= sizeof(params)) {
			error("Too many parameters\n");
			return EX_USAGE;
		}
	}

	fd = open_with_msgs(argv[1], O_RDONLY, 0);
	if (fd < 0)
		return ex_errno(errno);

	if (is_compressed(argv[1]))
		flags |= MODULE_INIT_COMPRESSED_FILE;

	if (finit_module(fd, params, flags)) {
		error("Failed to load %s: %d\n", argv[1], errno);
		switch (errno) {
		/* Not a module, built for another kernel, an unresolved symbol */
		case ENOEXEC:
		case EINVAL:
		case ENOENT:
			return EX_DATAERR;
		/* Already loaded */
		case EEXIST:
			return EX_UNAVAILABLE;
		default:
			return ex_errno(errno);
		}
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
			usage(RMMOD_USAGE, argv[0]);
			return EX_USAGE;
		}
	}

	if (optind != argc - 1) {
		usage(RMMOD_USAGE, argv[0]);
		return EX_USAGE;
	}

	/* Module name can be the name or the file path */
	name = path_basename(argv[optind]);

	if (strlcpy(buf, name, sizeof(buf)) >= sizeof(buf)) {
		error("Module name too long\n");
		return EX_USAGE;
	}

	dot = strrchr(buf, '.');
	if (dot && !strcmp(dot, ".ko"))
		*dot = 0;

	/* The kernel wants underscores instead of dashes apparently */
	str_replace_char(buf, '-', '_');

	if (delete_module(buf, flags)) {
		error("Failed to unload %s: %d\n", buf, errno);
		return ex_errno(errno);
	}

	return 0;
}

static const struct multicall_prog progs[] = {
	{ "insmod", prog_insmod },
	{ "rmmod", prog_rmmod },
};

MULTICALL_MAIN(progs)
