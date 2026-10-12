// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "keeper"

#include "common.h"
#include "dungeon.h"

#define USAGE								\
	"usage: keeper [--] <program> [args ...]  start a program\n"	\
	"       keeper -l                         list the dungeon\n"	\
	"       keeper -r <name>                  restart a program\n"

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

static int stop(pid_t pid)
{
	int i;

	if (kill(pid, SIGTERM))
		return -1;

	/* Wait for it to exit cleanly */
	for (i = 0; i < 20; i++) {
		if (!dungeon_alive(pid))
			return 0;

		usleep(100000);
	}

	/* bye bye */
	return kill(pid, SIGKILL);
}

static int restart(const char *name, char **envp)
{
	char *cmd_argv[DUNGEON_ARGS + 1];
	char cmd[DUNGEON_CMD_MAX];
	char dir[160];
	pid_t pid, keeper;

	if (!path_join(dir, sizeof(dir), DUNGEON_DIR, name)) {
		error("Name too long: %s\n", name);
		return 1;
	}

	if (dungeon_read_cmd(dir, cmd, sizeof(cmd), cmd_argv, DUNGEON_ARGS) < 1) {
		error("Dunno know what %s was: %s/cmd\n", name, dir);
		return 1;
	}

	/* Can't work out if someone else is managing this */
	keeper = dungeon_read_keptby(dir);
	if (keeper < 0) {
		error("Don't know who keeps %s, leaving it alone\n", name);
		return 1;
	}

	/* Someone else is managing this */
	if (keeper) {
		error("%s is kept by %d, leaving it alone\n", name, (int) keeper);
		return 1;
	}

	pid = dungeon_read_pid(dir);

	if (dungeon_alive(pid) && stop(pid)) {
		error("Failed to stop %d: %d\n", (int) pid, errno);
		return 1;
	}

	return start(cmd_argv, envp);
}

int main(int argc, char **argv, char **envp)
{
	int c;

	while ((c = getopt(argc, argv, "lr:")) != -1) {
		switch (c) {
		case 'l':
			return show_all();

		case 'r':
			return restart(optarg, envp);

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
