// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_IO_H
#define _SMOLUTILS_IO_H

#include "cleanup.h"

/* write() with loop to make sure the full write happens */
static inline __warn_if_unused int write_full(int fd, const void *buf, int len)
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
static inline __warn_if_unused int read_full(int fd, void *buf, int len)
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

#define READ_STRUCT_FLAGS (O_RDONLY | O_NONBLOCK | O_NOFOLLOW)

static int __read_struct_fd(int fd, void *out, size_t len)
{
	struct stat st;

	if (fstat(fd, &st) || !S_ISREG(st.st_mode))
		return -1;

	if (st.st_size != (off_t) len)
		return -1;

	if (read_full(fd, out, len) != (int) len)
		return -1;

	return 0;
}

/* open() + read() + close(), the file has to be exactly len bytes - by path*/
static int __read_struct(const char *path, void *out, size_t len)
{
	int __cleanup_fd fd = -1;

	fd = open(path, READ_STRUCT_FLAGS);
	if (fd < 0)
		return -1;

	return __read_struct_fd(fd, out, len);
}

/* open() + read() + close(), the file has to be exactly len bytes - relative to dir */
static int __read_struct_at(int dir, const char *name, void *out, size_t len)
{
	int __cleanup_fd fd = -1;

	fd = openat(dir, name, READ_STRUCT_FLAGS);
	if (fd < 0)
		return -1;

	return __read_struct_fd(fd, out, len);
}

/* treat a path as an on-disk copy of a struct and read it into a struct */
#define read_struct(_path, _out) __read_struct(_path, _out, sizeof(*(_out)))
#define read_struct_at(_dir, _name, _out) \
	__read_struct_at(_dir, _name, _out, sizeof(*(_out)))

#endif /* _SMOLUTILS_IO_H */
