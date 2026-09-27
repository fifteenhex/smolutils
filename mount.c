// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "multicall.h"

#include <linux/mount.h>

#define MOUNT_USAGE "usage: mount -t <type> [-o <options>] <source> <target>\n"

struct mount_option {
	const char *name;
	unsigned long flag;
	bool clear;
};

static const struct mount_option mount_options[] = {
	{ "ro", MS_RDONLY },
	{ "rw", MS_RDONLY, true },
};

static int parse_options(const char *opts, unsigned long *flags)
{
	while (*opts) {
		const char *comma = strchr(opts, ',');
		int len = comma ? (int) (comma - opts) : (int) strlen(opts);
		unsigned int i;

		for (i = 0; i < ARRAY_SIZE(mount_options); i++) {
			const struct mount_option *o = &mount_options[i];

			if ((int) strlen(o->name) != len ||
			    strncmp(opts, o->name, len))
				continue;

			if (o->clear)
				*flags &= ~o->flag;
			else
				*flags |= o->flag;

			break;
		}

		if (i == ARRAY_SIZE(mount_options)) {
			error("Unknown option: %.*s\n", len, opts);
			return -1;
		}

		opts = comma ? comma + 1 : opts + len;
	}

	return 0;
}

static int prog_mount(int argc, char **argv, char **envp)
{
	unsigned long flags = 0;
	char *source = NULL;
	char *target = NULL;
	char *type = NULL;
	int ret;
	int c;

        while ((c = getopt(argc, argv, "t:o:")) != -1) {
                switch (c) {
                case 't':
			type = optarg;
                        break;

		case 'o':
			if (parse_options(optarg, &flags))
				return 1;
			break;

		default:
			usage(MOUNT_USAGE);
			return 1;
                }
        }

	source = (optind < argc) ? argv[optind++] : NULL;
	target = (optind < argc) ? argv[optind++] : NULL;

	if (!type || !source || !target) {
		usage(MOUNT_USAGE);
		return 1;
	}

	ret = mount(source, target, type, flags, NULL);
	if (ret) {
		error("mount(%s) failed: %d\n", target, errno);
		return 1;
	}

	return 0;
}

static int prog_umount(int argc, char **argv, char **envp)
{
	char *target;

	if (argc != 2)
		return 1;

	target = argv[1];

	if (umount2(target, 0)) {
		error("umount(%s) failed: %d\n", target, errno);
		return 1;
	}

	return 0;
}

static const struct multicall_prog progs[] = {
	{ "mount", prog_mount },
	{ "umount", prog_umount },
};

int main (int argc, char **argv, char **envp)
{
	MULTICALL_DISPATCH(argv[0], progs);

	return 1;
}
