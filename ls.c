// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

static int cb_short(const char *name, void *priv)
{
	printf("%s\t", name);

	return 0;
}

static int cb_long(const char *name, void *priv)
{
	printf("%s\n", name);

	return 0;
}

int main(int argc, char **argv, char **envp)
{
	bool long_format = false;
	char *path;
	char c;

	while ((c = getopt(argc, argv, "l")) != -1) {
		switch (c) {
		case 'l':
			long_format = true;
			break;
		}
	}

	path = (optind < argc) ? argv[optind] : ".";

	iterate_dir(path, long_format ? cb_long : cb_short, NULL);

	return 0;
}
