// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

int main (int argc, char **argv, char **envp)
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
