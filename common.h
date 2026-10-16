// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_COMMON_H
#define _SMOLUTILS_COMMON_H

#include "inc/cleanup.h"

#define __smol_always_inline inline __attribute__((always_inline))

#define ARRAY_SIZE(_a) (sizeof(_a) / sizeof(_a[0]))

#define STRLEN(_s) (ARRAY_SIZE(_s) - 1)

/* for each item in an array */
#define foreach(_p, _array)						\
	for (__typeof__(&(_array)[0]) _p = (_array);			\
	     _p < (_array) + ARRAY_SIZE(_array);			\
	     _p++)

/* Shared state directories, tmpfs abuse */
#define SMOL_RUN_DIR		"/run/smol"
#define SMOL_RUN_PRIVATE_DIR	SMOL_RUN_DIR "/private"
#define SMOL_RUN_PUBLIC_DIR	SMOL_RUN_DIR "/public"

/* A tty we trust with important stuff */
#define SMOL_SECURETTY_PATH SMOL_RUN_PRIVATE_DIR "/securetty"

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

#if is_enabled(CONFIG_DEBUG)
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

/* Overflow safe(TM) bounds etc checking helpers */

/* Does (_off + _len) fit into _total, taking care that (_off + _len) didn't overflow */
#define off_len_fits(_off, _len, _total)				\
({									\
	__typeof__((_total) + 0) __olf_end;				\
									\
	!__builtin_add_overflow((_off), (_len), &__olf_end) &&		\
	__olf_end <= (_total);						\
})

/* File stuff */

static off_t file_size(int fd) {
	struct stat st;

	if (fstat(fd, &st) == -1)
		return -1;

	return st.st_size;
}

static bool is_chardev(int fd)
{
	struct stat st;

	if (fstat(fd, &st) == -1)
		return false;

	return S_ISCHR(st.st_mode);
}

static __smol_always_inline bool __is_regfile(struct stat *st)
{
	return S_ISREG(st->st_mode);
}

static inline bool is_regfile(int fd)
{
	struct stat st;

	if (fstat(fd, &st) == -1)
		return false;

	return __is_regfile(&st);
}

/* Do two fd's point to the same file? */
static bool __same_file(struct stat *sa, struct stat *sb)
{
	if ((S_ISBLK(sa->st_mode) && S_ISBLK(sb->st_mode)) ||
	    (S_ISCHR(sa->st_mode) && S_ISCHR(sb->st_mode)))
		return sa->st_rdev == sb->st_rdev;

	return sa->st_dev == sb->st_dev && sa->st_ino == sb->st_ino;
}

static bool same_file(int a, int b)
{
	struct stat sa, sb;

	if (fstat(a, &sa) || fstat(b, &sb))
		return false;

	return __same_file(&sa, &sb);
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

	fd = open(path, O_RDONLY | O_DIRECTORY);
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

/*
 * Mainly for reading /proc, /sys single line files.
 * Reads, NULL terminates and strips newline.
 */
static inline bool read_file(const char *path, char *out, size_t len)
{
	int __cleanup_fd fd = -1;
	int got;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return false;

	got = read_full(fd, out, len - 1);
	if (got <= 0)
		return false;

	out[got] = '\0';

	while (got && (out[got - 1] == '\n' || out[got - 1] == '\r'))
		out[--got] = '\0';

	return true;
}

/* Create a file (if needed) and jam a buffer into it */
static int write_file(const char *path, const void *what, size_t len)
{
	int __cleanup_fd fd = -1;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		error("Failed to open %s: %d\n", path, errno);
		return -1;
	}

	if (write(fd, what, len) != (int) len) {
		error("Failed to write %s: %d\n", path, errno);
		return -1;
	}

	return 0;
}

/*
 * Create a directory if it does not already exist
 * and always chmod it with the desired mode.
 */
static int mk_chmod_dir(const char *path, mode_t mode)
{
	if (mkdir(path, mode) && errno != EEXIST) {
		error("mkdir(%s) failed: %d\n", path, errno);
		return -1;
	}

	if (chmod(path, mode)) {
		error("chmod(%s) failed: %d\n", path, errno);
		return -1;
	}

	return 0;
}

/* Read a file line by line, calling cb() for each */
static inline int read_lines(const char *path, char *buf, size_t len,
			     void (*cb)(char *line, void *priv), void *priv)
{
	int __cleanup_fd fd = -1;
	size_t held = 0;
	int dropped = 0;
	int got;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;

	while ((got = read(fd, buf + held, len - held - 1)) > 0) {
		char *p = buf;
		char *nl;

		held += got;
		buf[held] = '\0';

		while ((nl = strchr(p, '\n'))) {
			*nl = '\0';
			cb(p, priv);
			p = nl + 1;
		}

		held = strlen(p);

		/* Drop the line if its too big */
		if (held == len - 1) {
			dropped++;
			held = 0;
			continue;
		}

		memmove(buf, p, held + 1);
	}

	/* Tail bit */
	if (held)
		cb(buf, priv);

	return dropped;
}

/* get the next whitespace separated field from a string and terminate it */
static inline char *next_field(char **pos)
{
	char *start = *pos;
	char *end;

	while (*start == ' ' || *start == '\t')
		start++;

	if (!*start)
		return NULL;

	for (end = start; *end && *end != ' ' && *end != '\t'; end++)
		;

	*pos = *end ? end + 1 : end;
	*end = '\0';

	return start;
}

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

static inline bool parse_ul(const char *s, int base, unsigned long *out)
{
	char *end;

	*out = strtoul(s, &end, base);

	return end != s && *end == '\0';
}

static inline bool parse_ull(const char *s, int base, unsigned long long *out)
{
	char *end;

	*out = strtoull(s, &end, base);

	return end != s && *end == '\0';
}

static inline bool parse_size(const char *s, unsigned long long *out)
{
	char *end;

	*out = strtoull(s, &end, 0);
	if (end == s)
		return false;

	switch (*end) {
	case 'G':
		*out <<= 10;
		/* fallthrough */
	case 'M':
		*out <<= 10;
		/* fallthrough */
	case 'K':
		*out <<= 10;
		end++;
		break;
	}

	return *end == '\0';
}

static inline bool path_join(char *buf, size_t len, const char *dir,
			     const char *name)
{
	return (size_t) snprintf(buf, len, "%s/%s", dir, name) < len;
}

static inline const char *path_basename(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

static inline bool read_file_in(const char *dir, const char *name,
				char *out, size_t len)
{
	char path[256];

	if (!path_join(path, sizeof(path), dir, name))
		return false;

	return read_file(path, out, len);
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
