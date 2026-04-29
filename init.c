// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define GETTY_PATH "/sbin/getty"
#define GETTY_NAME "getty"
#define SHELL_PATH "/bin/sh"

static const char cmdline_opt_prefix[] = "smolinit.";
static const char cmdline_opt_getty[] = "getty=";

static const char *gettys[16] = { 0 };
static unsigned num_gettys = 0;

static void parse_cmdline(int argc, char **argv)
{
	int i;

	debug("cmdline args:\n");

	/* First arg will be the program name, skip that */
	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];

		debug("%s\n", arg);

		if (strncmp(arg, cmdline_opt_prefix, STRLEN(cmdline_opt_prefix)) == 0) {
			const char *opt = arg + STRLEN(cmdline_opt_prefix);

			if (strncmp(opt, cmdline_opt_getty, STRLEN(cmdline_opt_getty)) == 0) {
				const char *getty = opt + STRLEN(cmdline_opt_getty);

				debug("Will start getty on %s\n", getty);
				/*
				 * I guess its safe to just point the argv memory to avoid
				 * wasting memory copying strings.
				 */
				gettys[num_gettys++] = getty;
			}
		}
	}
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
			char * const newargv[] = {
				GETTY_NAME,
				gettys[0],
				SHELL_PATH,
				NULL
			};
			char *newenviron[] = { NULL };

			execve(GETTY_PATH, newargv, newenviron);
			printf("execve failed\n");
			break;
		}
	}

	return 0;
}
