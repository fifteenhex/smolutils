// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "keeper"

#include "common.h"
#include "dungeon.h"

#define USAGE								\
	"usage: keeper [--] <program> [args ...]  start a program\n"	\
	"       keeper -l                         list the dungeon\n"

static int show_one(const char *name, int dirfd, void *priv)
{
	char *cmd_argv[DUNGEON_ARGS + 1];
	char cmd[DUNGEON_CMD_MAX];
	char dir[160];
	pid_t pid;
	int i;

	if (!path_join(dir, sizeof(dir), DUNGEON_DIR, name))
		return 0;

	pid = dungeon_read_pid(dir);

	printf("%-16s ", name);

	if (dungeon_alive(pid))
		printf("%-8s %-7d ", "running", (int) pid);
	else
		printf("%-8s %-7s ", "dead", "-");

	if (dungeon_read_cmd(dir, cmd, sizeof(cmd), cmd_argv, DUNGEON_ARGS) > 0)
		for (i = 0; cmd_argv[i]; i++)
			printf("%s%s", i ? " " : "", cmd_argv[i]);

	printf("\n");

	return 0;
}

static int show_all(void)
{
	printf("%-16s %-8s %-7s %s\n", "NAME", "STATE", "PID", "CMD");

	if (iterate_dir(DUNGEON_DIR, show_one, NULL) < 0 && errno != ENOENT) {
		error("Failed to read %s: %d\n", DUNGEON_DIR, errno);
		return 1;
	}

	return 0;
}

static int start(char **argv, char **envp)
{
	char dir[128];

	if (dungeon_make_dir(dir, sizeof(dir), argv[0]))
		return 1;

	return dungeon_spawn(dir, argv, envp, true) < 0 ? 1 : 0;
}

int main(int argc, char **argv, char **envp)
{
	int c;

	while ((c = getopt(argc, argv, "l")) != -1) {
		switch (c) {
		case 'l':
			return show_all();

		default:
			usage(USAGE);
			return 1;
		}
	}

	if (optind >= argc) {
		usage(USAGE);
		return 1;
	}

	return start(&argv[optind], envp);
}
