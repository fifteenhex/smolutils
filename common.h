// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_COMMON_H
#define _SMOLUTILS_COMMON_H

#define ARRAY_SIZE(_a) (sizeof(_a) / sizeof(_a[0]))

#define STRLEN(_s) (ARRAY_SIZE(_s) - 1)

/* Shared state directories, tmpfs abuse */
#define SMOL_RUN_DIR		"/run/smol"
#define SMOL_RUN_PRIVATE_DIR	SMOL_RUN_DIR "/private"
#define SMOL_RUN_PUBLIC_DIR	SMOL_RUN_DIR "/public"

/* Printing stuff */

/*
 * #define TAG "something" before including this
 * to get tagged log lines
 */
#ifdef TAG
#define TAG_PREFIX	TAG ": "
#else
#define TAG_PREFIX	""
#endif

#ifdef CONFIG_DEBUG
#define debug(fmt, ...) printf(TAG_PREFIX fmt, ##__VA_ARGS__)
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

#define error(fmt, ...) fprintf(stderr, TAG_PREFIX fmt, ##__VA_ARGS__)

#define info(...) printf(__VA_ARGS__)

/*
 * Usage messages, CONFIG_USAGE=n removes them if you are really tight on space.
 */
#if is_enabled(CONFIG_USAGE)
#define usage(...) fprintf(stderr, __VA_ARGS__)
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

/* write() with loop to make sure the full write happens */
static inline int write_full(int fd, const void *buf, int len)
{
	int done = 0;

	while (done < len) {
		int ret = write(fd, (const char *) buf + done, len - done);

		if (ret <= 0)
			return -1;

		done += ret;
	}

	return 0;
}

/* read() with loop to make sure the full read happens */
static inline int read_full(int fd, void *buf, int len)
{
	int got = 0;

	while (got < len) {
		int ret = read(fd, (char *) buf + got, len - got);

		if (ret < 0)
			return -1;
		if (ret == 0)
			break;

		got += ret;
	}

	return got;
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
 * fds is what the child should use for stdin, stdout and stderr,
 * -1 == inherit. NULL means inherit everything.
 *
 * returns a pid or -1 to the caller.
 */
static __attribute__((noinline)) int spawn_redirect(const char *path,
						    char * const argv[],
						    char * const envp[],
						    const int *fds)
{
	volatile pid_t pid;

	pid = vfork();

	if (pid == -1)
		return -1;

	if (!pid) {
		int i;

		for (i = 0; fds && i < 3; i++) {
			if (fds[i] >= 0 && fds[i] != i)
				dup2(fds[i], i);
		}

		execve(path, argv, envp);
		_exit(1);
	}

	return pid;
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
