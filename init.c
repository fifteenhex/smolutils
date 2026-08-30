// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "nolibc_extensions/signal.h"

#define STARTUP_PATH "/sbin/startup"
#define GETTY_PATH "/sbin/getty"
#define GETTY_NAME "getty"
#define INSMOD_PATH "/sbin/insmod"
#define INSMOD_NAME "insmod"
#define SHELL_PATH "/bin/smolsh"

static const char cmdline_opt_prefix[] = "smolinit.";
static const char cmdline_opt_getty[] = "getty=";
static const char cmdline_opt_hostname[] = "hostname=";
static const char cmdline_opt_dhcpif[] = "dhcpif=";
static const char cmdline_opt_insmod[] = "insmod=";

struct getty {
	const char *tty_path;
	pid_t getty_pid;
};

static struct getty gettys[16];
static unsigned num_gettys = 0;

static const char *hostname = NULL;
static const char *dhcpif = NULL;

static const char *modules[8];
static unsigned num_modules = 0;

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

				if (num_gettys >= ARRAY_SIZE(gettys)) {
					error("Too many gettys\n");
					continue;
				}

				debug("Will start getty on TTY %s\n", tty_path);
				/*
				 * I guess its safe to just point the argv memory to avoid
				 * wasting memory copying strings.
				 */
				gettys[num_gettys++].tty_path = tty_path;
			}

			else if (!hostname && STARTS_WITH(opt, cmdline_opt_hostname)) {
				const char *name = opt + STRLEN(cmdline_opt_hostname);

				debug("Hostname will be %s\n", name);
				hostname = name;
			}

			else if (STARTS_WITH(opt, cmdline_opt_insmod)) {
				const char *path = opt + STRLEN(cmdline_opt_insmod);

				if (num_modules >= ARRAY_SIZE(modules)) {
					error("Too many modules\n");
					continue;
				}

				debug("Will load %s\n", path);
				modules[num_modules++] = path;
			}

			else if (!dhcpif && STARTS_WITH(opt, cmdline_opt_dhcpif)) {
				const char *intf = opt + STRLEN(cmdline_opt_dhcpif);

				debug("Will configure %s via DHCP\n", intf);
				dhcpif = intf;
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

/* Load modules, order is important as there is no dependency checking */
static void load_modules(void)
{
	unsigned i;

	if (!is_enabled(CONFIG_MODULES))
		return;

	for (i = 0; i < num_modules; i++) {
		char * const newargv[] = {
			INSMOD_NAME,
			(char *) modules[i],
			NULL
		};

		if (spawn_and_wait_args(INSMOD_PATH, newargv))
			error("Failed to load %s\n", modules[i]);
	}
}

static inline int run_startup(void)
{
	char *startup_args[6] = {
		"startup",
	};
	int startup_argc = 1;
	int ret;

	if (!hostname)
		hostname="smol";
	startup_args[startup_argc++] = "-h";
	startup_args[startup_argc++] = hostname;

	if (dhcpif) {
		startup_args[startup_argc++] = "-n";
		startup_args[startup_argc++] = dhcpif;
	}

	ret = spawn_and_wait_args(STARTUP_PATH, startup_args);
	if (ret)
		error("startup failed\n");

	return 0;
}


int main (int argc, char **argv, char **envp)
{
	int ret, i;

	printf("smolutils init (%s, %s)\n", __DATE__, __TIME__);

	parse_cmdline(argc, argv);

	parse_environment(envp);

	load_modules();

	ret = run_startup();
	if (ret)
		return 1;

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
