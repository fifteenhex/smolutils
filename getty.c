// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "getty"

#include "common.h"
#include "auth.h"
#include "seat.h"
#include "users.h"

#include "nolibc_extensions/signal.h"
#include "nolibc_extensions/unistd.h"

/* Don't get killed by SIGINT */
static void handle_sigint(int sig)
{
}

#define SEAT_NODE_MAX	(SEAT_PATH_MAX + 64)

#define SEAT_ENVIRON_MAX	32

struct seat {
	const char *path;
	uid_t uid;
	char dir[SEAT_PATH_MAX];
	char var[SEAT_PATH_MAX + STRLEN(SEAT_ENV "=")];
	char *env[SEAT_ENVIRON_MAX];
};

static int seat_add(const char *name, int dir, void *priv)
{
	const struct seat *seat = priv;
	char path[SEAT_NODE_MAX];
	struct stat st;

	if (fstatat(dir, name, &st, 0))
		return 0;

	if (!S_ISCHR(st.st_mode))
		return 0;

	if (snprintf(path, sizeof(path), "%s/%s", seat->path, name)
	    >= (int) sizeof(path))
		return 0;

	if (mknod(path, S_IFCHR | 0600, st.st_rdev)) {
		verbose("Failed to put %s on the seat: %d\n", name, errno);
		return 0;
	}

	if (chmod(path, 0600) || chown(path, seat->uid, seat->uid))
		verbose("Failed to hand %s over: %d\n", name, errno);

	return 0;
}

static int seat_drop(const char *name, int dir, void *priv)
{
	if (unlinkat(dir, name, 0))
		verbose("Failed to take %s away: %d\n", name, errno);

	return 0;
}

static void seat_make(struct seat *seat, const char *tty, uid_t uid)
{
	static const char * const sources[] = SEAT_SOURCES;
	struct stat st;
	unsigned int i;

	if (!is_enabled(CONFIG_SEAT))
		return;

	seat->path = NULL;
	seat->uid = uid;

	if (seat_path(seat->dir, sizeof(seat->dir), tty)
	    >= (int) sizeof(seat->dir)) {
		debug("No room for a seat path\n");
		return;
	}

	if (mkdir(seat->dir, 0700) && errno != EEXIST) {
		debug("Failed to make the seat %s: %d\n", seat->dir, errno);
		return;
	}

	if (lstat(seat->dir, &st) || !S_ISDIR(st.st_mode)) {
		debug("The seat %s isn't a directory\n", seat->dir);
		return;
	}

	if (chmod(seat->dir, 0700) || chown(seat->dir, uid, uid)) {
		debug("Failed to hand the seat over: %d\n", errno);
		return;
	}

	seat->path = seat->dir;

	for (i = 0; i < ARRAY_SIZE(sources); i++)
		if (iterate_dir(sources[i], seat_add, seat) < 0)
			verbose("No %s to take devices from\n", sources[i]);
}

static void seat_take_away(const struct seat *seat)
{
	if (!is_enabled(CONFIG_SEAT))
		return;

	if (!seat->path)
		return;

	iterate_dir(seat->path, seat_drop, NULL);
}

/* nolibc has no setenv, so build the child's environment by hand */
static char * const *seat_environ(struct seat *seat)
{
	size_t n = 0;
	char **e;

	if (!is_enabled(CONFIG_SEAT))
		return environ;

	if (!seat->path)
		return environ;

	if (snprintf(seat->var, sizeof(seat->var), SEAT_ENV "=%s", seat->path)
	    >= (int) sizeof(seat->var))
		return environ;

	for (e = environ; *e; e++) {
		/* One slot for the seat, one for the NULL that ends it */
		if (n + 2 >= ARRAY_SIZE(seat->env)) {
			error("Too much environment to add the seat to\n");
			return environ;
		}

		seat->env[n++] = *e;
	}

	seat->env[n++] = seat->var;
	seat->env[n] = NULL;

	return seat->env;
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
	char * const newargv[] = { "sh", NULL };
	const char *shell_path;
	const char *tty_path;
	char * const *env;
	struct seat seat;
	int ret = 0;
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

	/* We don't just let anyone poke around in here I've have you know.. */
	if (auth_check(tty_path)) {
		error("%s wasn't allowed in\n", tty_path);
		return 1;
	}

	/* While we are still root, a device node isn't the user's to make */
	seat_make(&seat, seat_name(tty_path), SMOLUTILS_USERS_NORMAL_MIN);

	env = seat_environ(&seat);

#if 1
	/* Change the user, this is what login would do... */
	if (users_changeuser(SMOLUTILS_USERS_NORMAL_MIN,
			     SMOLUTILS_USERS_NORMAL_MIN)) {
		error("Failed to switch user\n");
		return 1;
	}
#endif

	/* -1 == spawn failed, anything else is the return code from the shell */
	if (spawn_and_wait_full(shell_path, newargv, env, NULL) < 0) {
		error("Failed to spawn shell\n");
		ret = 1;
	}

	debug("%s exited\n", tty_path);

	/* The login is over, so the hardware isn't theirs */
	seat_take_away(&seat);

	return ret;
}
