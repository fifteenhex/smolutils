// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "nolibc_extensions/signal.h"
#include "nolibc_extensions/unistd.h"

/* Signals for controlling halt, poweroff, reboot, matches busybox */
#define SIG_HALT	SIGUSR1
#define SIG_POWEROFF	SIGUSR2
#define SIG_REBOOT	SIGTERM

#define SHUTDOWN_GRACE	2

static volatile int shutdown_cmd;

#define STARTUP_PATH "/sbin/startup"
#define GETTY_PATH "/sbin/getty"
#define GETTY_NAME "getty"
#define INSMOD_PATH "/sbin/insmod"
#define INSMOD_NAME "insmod"
#define TELNETD_PATH "/sbin/telnetd"
#define TELNETD_NAME "telnetd"
#define SHELL_PATH "/bin/smolsh"

static const char cmdline_opt_prefix[] = "smolinit.";
static const char cmdline_opt_getty[] = "getty=";
static const char cmdline_opt_hostname[] = "hostname=";
static const char cmdline_opt_dhcpif[] = "dhcpif=";
static const char cmdline_opt_insmod[] = "insmod=";
static const char cmdline_opt_telnetd[] = "telnetd=";

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

static const char *telnetd_port = NULL;
static pid_t telnetd_pid = -1;

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

			else if (is_enabled(CONFIG_TELNETD) &&
				 (!telnetd_port && STARTS_WITH(opt, cmdline_opt_telnetd))) {
				const char *port = opt + STRLEN(cmdline_opt_telnetd);

				/* FIXME port isn't optional,  smolinit.telnetd=0 means the default port */
				telnetd_port = *port ? port : "23";

				debug("Will start telnetd on port %s\n",
				      telnetd_port);
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
	char * const newargv[] = {
		GETTY_NAME,
		tty_path,
		SHELL_PATH,
		NULL
	};
	pid_t pid;

	pid = spawn(GETTY_PATH, newargv, environ);

	if (pid == -1)
		return -1;

	getty->getty_pid = pid;

	return 0;
}

static int spawn_telnetd(void)
{
	char * const newargv[] = {
		TELNETD_NAME,
		"-p",
		telnetd_port,
		NULL
	};

	if (!is_enabled(CONFIG_TELNETD))
		return 0;

	telnetd_pid = spawn(TELNETD_PATH, newargv, environ);

	return telnetd_pid < 0 ? -1 : 0;
}

static void handle_shutdown(int sig)
{
	switch (sig) {
	case SIG_HALT:
		shutdown_cmd = LINUX_REBOOT_CMD_HALT;
		break;

	case SIG_POWEROFF:
		shutdown_cmd = LINUX_REBOOT_CMD_POWER_OFF;
		break;

	default:
		shutdown_cmd = LINUX_REBOOT_CMD_RESTART;
		break;
	}
}

static int setup_signals(void)
{
	static const int shutdown_signals[] = {
		SIG_HALT,
		SIG_POWEROFF,
		SIG_REBOOT,
	};
	struct sigaction shutdown_act = {
		.sa_handler = handle_shutdown,
	};
	unsigned i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(shutdown_signals); i++) {
		ret = sigaction(shutdown_signals[i], &shutdown_act, NULL);
		if (ret)
			verbose("failed to setup signal %d: %d\n",
				shutdown_signals[i], errno);
	}

	return 0;
}

static void do_shutdown(int cmd)
{
	printf("Shutting down\n");

	kill(-1, SIGTERM);
	sleep(SHUTDOWN_GRACE);
	kill(-1, SIGKILL);

	sync();

	reboot(cmd);

	error("reboot() failed: %d\n", errno);
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

	/* init will be multicall later, so when running as init check the pid is 1 */
	if (getpid() != 1) {
		verbose("init is already running\n");
		return 1;
	}

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

	if (telnetd_port && spawn_telnetd())
		error("Failed to spawn telnetd\n");

	/* Now sit in wait for one of the gettys to exit */
	while (true) {
		int status = 0;
		pid_t pid;
		int i;

		if (shutdown_cmd) {
			do_shutdown(shutdown_cmd);
			shutdown_cmd = 0;
		}

		pid = wait(&status);

		/* No kids left */
		if (pid < 0 && errno == ECHILD) {
			verbose("No children left\n");
			break;
		}

		verbose("pid %d came home\n", (int) pid);

		if (telnetd_port && pid == telnetd_pid) {
			/*
			 * It only returns non-zero when it couldn't start
			 * at all, and starting it again won't fix that
			 */
			if (WIFEXITED(status) && WEXITSTATUS(status))
				error("telnetd gave up, leaving it alone\n");
			else
				spawn_telnetd();

			continue;
		}

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
