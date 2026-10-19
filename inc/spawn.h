// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_SPAWN_H
#define _SMOLUTILS_SPAWN_H

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

	/* There isn't much we can do if we don't have an MMU, trust me, I've tested this... */
	/* NOLINTNEXTLINE(bugprone-unsafe-functions,clang-analyzer-security.insecureAPI.vfork) */
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

	/*
	 * waitpid() might get interrupted but the child might still be alive
	 * so loop until its really dead for sure.
	 */
	while (waitpid(pid, &waitpid_stat, 0) < 0) {
		if (errno != EINTR)
			return -1;
	}

	/* nolibc has a bug that means WIFEXITED() must be checked first */
	if (WIFEXITED(waitpid_stat))
		return WEXITSTATUS(waitpid_stat);

	if (WIFSIGNALED(waitpid_stat)) {
		if (killed)
			*killed = true;
		return WTERMSIG(waitpid_stat);
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

#endif /* _SMOLUTILS_SPAWN_H */
