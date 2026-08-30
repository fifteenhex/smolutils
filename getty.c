// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "users.h"

#include "nolibc_extensions/signal.h"

/* Don't get killed by SIGINT */
static void handle_sigint(int sig)
{
}

static void setup_signals(void)
{
	struct sigaction act = {
		.sa_flags   = SA_RESTART,
		.sa_handler = handle_sigint,
	};

	if (sigaction(SIGINT, &act, NULL))
		verbose("Failed to setup signals: %d\n", errno);
}

int main(int argc, char **argv, char **envp)
{
	const char *shell_path;
	const char *tty_path;
	int tty_fd;

	if (argc != 3)
		return 1;

	tty_path = argv[1];
	shell_path = argv[2];

	debug("Starting getty on %s with shell %s\n",
		tty_path, shell_path);

	tty_fd = open(tty_path, O_RDWR);
	if (tty_fd < 0) {
		error("Failed to open TTY\n");
		return 1;
	}

	setup_signals();

	/* Start a new session and make the tty the controlling tty so ctrl-c works */
	if (setsid() < 0)
		verbose("setsid() failed: %d\n", errno);

	if (ioctl(tty_fd, TIOCSCTTY, 0) < 0)
		verbose("Failed to take %s as the controlling tty: %d\n",
		      tty_path, errno);

	/* Wire up stdin, stdout, stderr */
	dup2(tty_fd, STDIN_FILENO);
	dup2(tty_fd, STDOUT_FILENO);
	dup2(tty_fd, STDERR_FILENO);
	close(tty_fd);

	/* Create a per-session seat directory owned by the target user */
	{
		char seat_path[64];

		snprintf(seat_path, sizeof(seat_path), "/run/seat-%d-%d",
			 (int)getpid(), SMOLUTILS_USERS_NORMAL_MIN);

		if (mkdir(seat_path, 0755) < 0 && errno != EEXIST) {
			error("Failed to create seat directory\n");
			return 1;
		}

		if (chown(seat_path, SMOLUTILS_USERS_NORMAL_MIN,
			  SMOLUTILS_USERS_NORMAL_MIN) < 0) {
			error("Failed to chown seat directory\n");
			return 1;
		}
	}

#if 1
	/* Change the user, this is what login would do... */
	if (users_changeuser(SMOLUTILS_USERS_NORMAL_MIN,
			     SMOLUTILS_USERS_NORMAL_MIN)) {
		error("Failed to switch user\n");
		return 1;
	}
#endif

	if (spawn_and_wait("sh", shell_path)) {
		error("Failed to spawn shell\n");
		return 1;
	}

	debug("%s exited\n", tty_path);

	return 0;
}
