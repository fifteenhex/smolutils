// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "multicall.h"

static int prog_mount(int argc, char **argv, char **envp)
{
	int ret;
	char *source = NULL;
	char *target = NULL;
	char *type = NULL;
	int c;

        while ((c = getopt(argc, argv, "t:")) != -1) {
                switch (c) {
                case 't':
			type = optarg;
                        break;
                }
        }

	if (!type)
		return 1;

	source = (optind < argc) ? argv[optind++] : NULL;
	if (!source)
		return 1;

	target = (optind < argc) ? argv[optind++] : NULL;
	if (!target)
		return 1;

	ret = mount(source, target, type, 0, NULL);
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
