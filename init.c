// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define SHELL_PATH "/bin/sh"

static void parse_cmdline(int argc, char **argv)
{
	int i;

	debug("cmdline args:\n");

	/* First arg will be the program name, skip that */
	for (i = 1; i < argc; i++)
		printf("%s\n", argv[i]);
}

static void parse_environment(char **envp)
{
	char *var;

	debug("environment variables\n");

	while (true) {
		var = *envp++;

		if (!var)
			break;

		debug("%s\n", var);
	}
}

static int do_mount(const char *source, const char *target, const char *type)
{
	int ret;

	ret = mount(source, target, type, 0, NULL);
	if (ret)
		printf("mount(%s) failed: %d\n", target, ret);

	return ret;
}

struct mountpoint {
	const char *source;
	const char *target;
	const char *type;
};

static const struct mountpoint fstab[] = {
	{"sysfs", "/sys", "sysfs"},
	{"proc", "/proc", "proc"},
	{"tmp", "/tmp", "tmpfs"},
};

/* This avoids having to use a mount command, fstab etc */
static int mount_filesystems(void)
{
	int ret;

	debug("mounting filesystems...\n");

	for (int i = 0; i < ARRAY_SIZE(fstab); i++) {
		const struct mountpoint *mp = &fstab[i];

		ret = do_mount(mp->source, mp->target, mp->type);
		if (ret)
			goto err;
	}

	return 0;

err:
	return ret;
}

int main (int argc, char **argv, char **envp)
{
	printf("smolutils init (%s, %s)\n", __DATE__, __TIME__);

	parse_cmdline(argc, argv);

	parse_environment(envp);

	mount_filesystems();

	while (true) {
		pid_t pid = vfork();

		/* We are init */
		if (pid) {
			wait(NULL);
		}
		/* We are the new process */
		else {
			static char *newargv[] = { "sh", NULL };
			static char *newenviron[] = { NULL };

			execve(SHELL_PATH, newargv, newenviron);
			printf("execve failed\n");
			break;
		}
	}

	return 0;
}
