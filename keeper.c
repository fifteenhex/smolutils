// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "keeper"

#include "common.h"
#include "dungeon.h"

#define USAGE "usage: keeper <program> [args ...]\n"

static int start(char **argv, char **envp)
{
	char dir[128];

	if (dungeon_make_dir(dir, sizeof(dir), argv[0]))
		return 1;

	return dungeon_spawn(dir, argv, envp, true) < 0 ? 1 : 0;
}

int main(int argc, char **argv, char **envp)
{
	if (argc < 2) {
		usage(USAGE);
		return 1;
	}

	return start(&argv[1], envp);
}
