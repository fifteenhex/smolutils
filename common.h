// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_COMMON_H
#define _SMOLUTILS_COMMON_H

#define ARRAY_SIZE(_a) (sizeof(_a) / sizeof(_a[0]))

#define STRLEN(_s) (ARRAY_SIZE(_s) - 1)

/* Printing stuff */

#ifdef CONFIG_DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

/*
 * VERBOSE is for really noisy messages that you
 * probably don't want to enable globally, define
 * this in the file you are debugging.
 */
#ifdef VERBOSE
#define verbose(...) debug(__VA_ARGS__)
#else
#define verbose(...)
#endif

#define error(...) printf(__VA_ARGS__)
#define info(...) printf(__VA_ARGS__)

/*
 * Usage messages, CONFIG_USAGE=n removes them if you are really tight on space
 */
#if is_enabled(CONFIG_USAGE)
#define usage(...) error(__VA_ARGS__)
#else
#define usage(...)
#endif

/* File stuff */

static off_t file_size(int fd) {
	struct stat st;

	if (fstat(fd, &st) == -1)
		return -1;

	return st.st_size;
}

static void cleanup_fd(int *_fd)
{
        int fd = *_fd;

        if (fd >= 0)
                close(fd);
}

#define __cleanup_fd __attribute__((cleanup(cleanup_fd)))

static void cleanup_dir(DIR **_dir)
{
	DIR *dir = *_dir;

	if (dir)
		closedir(dir);
}

#define __cleanup_dir __attribute__((cleanup(cleanup_dir)))

/*
 * Iterate over a directory calling cb() for each element,
 * if cb() returns <0 thats an error and the loop will abort,
 * 0 means to continue looping, and >0 means to exit the loop.
 */
static int iterate_dir(const char *path,
		       int (*cb)(const char *name, int dir, void *priv), void *priv)
{
	DIR __cleanup_dir *dir = NULL;
	struct dirent e, *result;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return -1;

	dir = fdopendir(fd);
	if (!dir)
		return -1;

        while ((readdir_r(dir, &e, &result) == 0) && result) {
                const char *name = e.d_name;
		int ret;

                if (strcmp(name, ".") == 0)
                        continue;

                if (strcmp(name, "..") == 0)
                        continue;

		ret = cb(name, fd, priv);

		/* done */
		if (ret > 0)
			return 1;

		/* error */
		if (ret < 0)
			return ret;
        }

	return 0;
}

/* String matching */

/* Does a string start with this char array? */
#define STARTS_WITH(_string, _chararray) \
	(strncmp(_string, _chararray, STRLEN(_chararray)) == 0)

/* Process stuff */


/*
 * Mininal wrapper around vfork() + execve() to avoid
 * shitting up the parent's stack.
 *
 * stdout_fd is what the child should use as its stdout, or -1 to
 * just inherit ours.
 *
 * returns a pid or -1 to the caller.
 */
static __attribute__((noinline)) int spawn_redirect(const char *path,
						    char * const argv[],
						    char * const envp[],
						    int stdout_fd)
{
	volatile pid_t pid;

	pid = vfork();

	if (pid == -1)
		return -1;

	if (!pid) {
		if (stdout_fd >= 0 && stdout_fd != STDOUT_FILENO)
			dup2(stdout_fd, STDOUT_FILENO);
		execve(path, argv, envp);
		_exit(1);
	}

	return pid;
}

static int spawn(const char *path, char * const argv[], char * const envp[])
{
	return spawn_redirect(path, argv, envp, -1);
}

static int spawn_and_wait_redirect(const char *path,
				   char * const argv[],
				   char * const envp[],
				   bool *killed,
				   int stdout_fd)
{
	int waitpid_stat;
	pid_t pid;

	pid = spawn_redirect(path, argv, envp, stdout_fd);

	if (pid < 0)
		return -1;

	waitpid(pid, &waitpid_stat, 0);

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
	return spawn_and_wait_redirect(path, argv, envp, killed, -1);
}

static int spawn_and_wait_args(const char *path, char * const argv[])
{
	char *newenviron[] = { NULL };

	return spawn_and_wait_full(path, argv, newenviron, NULL);
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
