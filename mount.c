// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

static int multicall_mount(int argc, char **argv, char **envp)
{
	int ret;
	char *source = NULL;
	char *target = NULL;
	char *type = NULL;
	char c;

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
		error("mount(%s) failed: %d\n", target, ret);
		return 1;
	}

	return 0;
}

static int multicall_umount(int argc, char **argv, char **envp)
{
	char *target;

	if (argc != 2)
		return 1;

	target = argv[1];

	umount2(target, 0);

	return 0;
}

int main (int argc, char **argv, char **envp)
{
	if (strcmp(argv[0], "mount") == 0)
		return multicall_mount(argc, argv, envp);
	else if (strcmp(argv[0], "umount") == 0)
		return multicall_umount(argc, argv, envp);

	return 1;
}
