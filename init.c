// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define GETTY_PATH "/sbin/getty"
#define GETTY_NAME "getty"
#define SHELL_PATH "/bin/smolsh"

static const char cmdline_opt_prefix[] = "smolinit.";
static const char cmdline_opt_getty[] = "getty=";

struct getty {
	const char *tty_path;
	pid_t getty_pid;
};

static struct getty gettys[16];
static unsigned num_gettys = 0;

static void parse_cmdline(int argc, char **argv)
{
	int i;

	debug("cmdline args:\n");

	/* First arg will be the program name, skip that */
	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];

		debug("%s\n", arg);

		if (STARTS_WITH(arg, cmdline_opt_prefix)) {
			const char *opt = arg + STRLEN(cmdline_opt_prefix);

			if (STARTS_WITH(opt, cmdline_opt_getty)) {
				const char *tty_path = opt + STRLEN(cmdline_opt_getty);

				debug("Will start getty on TTY %s\n", tty_path);
				/*
				 * I guess its safe to just point the argv memory to avoid
				 * wasting memory copying strings.
				 */
				gettys[num_gettys++].tty_path = tty_path;
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
	if (ret) {
		error("mount(%s) failed: %d\n", target, ret);
		return ret;
	}

	debug("mounted %s(%s) on %s\n", source, type, target);

	return 0;
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

static int spawn_getty(struct getty *getty)
{
	pid_t pid;

	pid = vfork();

	/* We are init */
	if (pid) {
		getty->getty_pid = pid;
		return 0;
	}
	/* We are the new process */
	else {
		const char *tty_path = getty->tty_path;
		int tty_fd;

		tty_fd = open(tty_path, O_RDWR);
		if (tty_fd < 0)
			return -1;

		/* Wire up stdin, stdout, stderr */
		dup2(tty_fd, STDIN_FILENO);
		dup2(tty_fd, STDOUT_FILENO);
		dup2(tty_fd, STDERR_FILENO);
		close(tty_fd);

		char * const newargv[] = {
			GETTY_NAME,
			tty_path,
			SHELL_PATH,
			NULL
		};
		char *newenviron[] = { NULL };

		execve(GETTY_PATH, newargv, newenviron);
		printf("execve failed\n");

		return -1;
	}
}

int main (int argc, char **argv, char **envp)
{
	int i;

	printf("smolutils init (%s, %s)\n", __DATE__, __TIME__);

	parse_cmdline(argc, argv);

	parse_environment(envp);

	mount_filesystems();

	/* Spawn each of the configured gettys */
	for (i = 0; i < num_gettys; i++) {
		spawn_getty(&gettys[i]);
	}

	/* Now sit in wait for one of the gettys to exit */
	while (true) {
		pid_t pid;
		int i;

		pid = wait(NULL);

		verbose("pid %d came home\n", (int) pid);

		for (i = 0; i < num_gettys; i++) {
			struct getty *getty = &gettys[i];

			if (getty->getty_pid == pid) {
				spawn_getty(getty);
				break;
			}
		}
	}

	return 0;
}
