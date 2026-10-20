// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_FS_H
#define _SMOLUTILS_FS_H

#include "cleanup.h"
#include "io.h"
#include "paths.h"
#include "printing.h"
#include "util.h"

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
	if (!dir) {
		close(fd);
		return -1;
	}

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

/* open() that prints the error so you don't need to copy/paste this string */
static inline int open_with_msgs(const char *path, int flags, mode_t mode)
{
	int fd = open(path, flags, mode);

	if (fd < 0)
		error("Failed to open %s: %d\n", path, errno);

	return fd;
}

/* Create a file (if needed) and jam a buffer into it */
static int write_file(const char *path, const void *what, size_t len)
{
	int __cleanup_fd fd = -1;

	fd = open_with_msgs(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return -1;

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

static inline bool read_file_in(const char *dir, const char *name,
				char *out, size_t len)
{
	char path[256];

	if (!path_join(path, sizeof(path), dir, name))
		return false;

	return read_file(path, out, len);
}

#endif /* _SMOLUTILS_FS_H */
