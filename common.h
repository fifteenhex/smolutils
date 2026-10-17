// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_COMMON_H
#define _SMOLUTILS_COMMON_H

#include "inc/cleanup.h"
#include "inc/fs.h"
#include "inc/io.h"
#include "inc/numbers.h"
#include "inc/paths.h"
#include "inc/printing.h"
#include "inc/strings.h"
#include "inc/util.h"

/* Shared state directories, tmpfs abuse */
#define SMOL_RUN_DIR		"/run/smol"
#define SMOL_RUN_PRIVATE_DIR	SMOL_RUN_DIR "/private"
#define SMOL_RUN_PUBLIC_DIR	SMOL_RUN_DIR "/public"

/* A tty we trust with important stuff */
#define SMOL_SECURETTY_PATH SMOL_RUN_PRIVATE_DIR "/securetty"

static inline int signal_catch(int sig, void (*handler)(int), int flags)
{
	struct sigaction act = {
		.sa_handler = handler,
		.sa_flags = flags,
	};

	return sigaction(sig, &act, NULL);
}

static void __signal_dummy_handler(int sig)
{
}

/* Setup a signal just to interrupt something that is blocking to get control back */
static inline int signal_interrupt(int sig)
{
	return signal_catch(sig, __signal_dummy_handler, 0);
}

static inline bool __read_number_in(const char *dir, const char *name,
				    int base, unsigned long *out)
{
	char tmp[128];
	char *end;

	if (!read_file_in(dir, name, tmp, sizeof(tmp)))
		return false;

	*out = strtoul(tmp, &end, base);

	return end != tmp;
}

static inline bool read_number_in(const char *dir, const char *name,
				  unsigned long *out)
{
	return __read_number_in(dir, name, 0, out);
}

static inline bool read_hex_number_in(const char *dir, const char *name,
				      unsigned long *out)
{
	return __read_number_in(dir, name, 16, out);
}

static inline bool write_number_in(const char *dir, const char *name,
				   unsigned long value)
{
	char path[256];
	char buf[32];
	int len;

	if (!path_join(path, sizeof(path), dir, name))
		return false;

	len = snprintf(buf, sizeof(buf), "%lu\n", value);

	return !write_file(path, buf, len);
}

/* Process stuff */

/*
 * Mininal wrapper around vfork() + execve() to avoid
 * shitting up the parent's stack.
 *
 * fds is what the child should use for stdin, stdout and stderr,
 * -1 == inherit. NULL means inherit everything.
 *
 * returns a pid or -1 to the caller.
 */
static __attribute__((noinline)) int __spawn(const char *path,
					     char * const argv[],
					     char * const envp[],
					     const int *fds, bool detach)
{
	volatile pid_t pid;

	pid = vfork();

	if (pid == -1)
		return -1;

	if (!pid) {
		int i;

		if (detach)
			setsid();

		for (i = 0; fds && i < 3; i++) {
			if (fds[i] >= 0 && fds[i] != i)
				dup2(fds[i], i);
		}

		execve(path, argv, envp);
		_exit(1);
	}

	return pid;
}

static int spawn_redirect(const char *path, char * const argv[],
			  char * const envp[], const int *fds)
{
	return __spawn(path, argv, envp, fds, false);
}

static int spawn_detached(const char *path, char * const argv[],
			  char * const envp[], const int *fds)
{
	return __spawn(path, argv, envp, fds, true);
}

static int spawn(const char *path, char * const argv[], char * const envp[])
{
	return spawn_redirect(path, argv, envp, NULL);
}

static int spawn_and_wait_redirect(const char *path,
				   char * const argv[],
				   char * const envp[],
				   bool *killed,
				   const int *fds)
{
	int waitpid_stat;
	pid_t pid;

	pid = spawn_redirect(path, argv, envp, fds);

	if (pid < 0)
		return -1;

	if (waitpid(pid, &waitpid_stat, 0) < 0)
		return -1;

	if (WIFSIGNALED(waitpid_stat)) {
		if (killed)
			*killed = true;
		return WTERMSIG(waitpid_stat);
	}

	if (WIFEXITED(waitpid_stat)) {
		return WEXITSTATUS(waitpid_stat);
	}

	return 0;
}

static int spawn_and_wait_full(const char *path,
			       char * const argv[],
			       char * const envp[],
			       bool *killed)
{
	return spawn_and_wait_redirect(path, argv, envp, killed, NULL);
}

static int spawn_and_wait_args(const char *path, char * const argv[])
{
	return spawn_and_wait_full(path, argv, environ, NULL);
}

static int spawn_and_wait(char *name, const char *path)
{
	char * const newargv[] = {
			name,
			NULL
	};

	return spawn_and_wait_args(path, newargv);
}

#endif /* _SMOLUTILS_COMMON_H */
