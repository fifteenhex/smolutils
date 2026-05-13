// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "nolibc_extensions/signal.h"

#define STARTUP_PATH "/sbin/startup"
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
	const char *tty_path = getty->tty_path;
	char *newenviron[] = { NULL };
	char * const newargv[] = {
		GETTY_NAME,
		tty_path,
		SHELL_PATH,
		NULL
	};
	int tty_fd;
	pid_t pid;

	pid = spawn(GETTY_PATH, newargv, newenviron);

	if (pid == -1)
		return -1;

	getty->getty_pid = pid;

	return 0;
}

static void handle_sig(int sig)
{
	printf("got sig!\n");
}

static int setup_signals(void)
{
	struct sigaction act = {
		.sa_flags   = SA_RESTART,
		.sa_handler = handle_sig,
	};
	int ret;

	ret = sigaction(SIGUSR1, &act, NULL);
	if (ret)
		printf("failed to setup signals: %d\n", errno);

	return 0;
}

int main (int argc, char **argv, char **envp)
{
	int ret, i;

	printf("smolutils init (%s, %s)\n", __DATE__, __TIME__);

	parse_cmdline(argc, argv);

	parse_environment(envp);

	mount_filesystems();

	ret = spawn_and_wait("startup", STARTUP_PATH);
	if (ret)
		error("startup failed\n");

	setup_signals();

	/* Spawn each of the configured gettys */
	for (i = 0; i < num_gettys; i++) {
		ret = spawn_getty(&gettys[i]);
		if (ret) {
			error("Failed to spawn getty\n");
			return 1; /* hmmm */
		}
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
