// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_IO_H
#define _SMOLUTILS_IO_H

#include "cleanup.h"

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

/* open() + read() + close(), the file has to be exactly len bytes */
static int __read_struct(const char *path, void *out, size_t len)
{
	int __cleanup_fd fd = -1;
	struct stat st;

	fd = open(path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
	if (fd < 0)
		return -1;

	if (fstat(fd, &st) || !S_ISREG(st.st_mode))
		return -1;

	if (st.st_size != (off_t) len)
		return -1;

	if (read_full(fd, out, len) != (int) len)
		return -1;

	return 0;
}

/* treat a path as an on-disk copy of a struct and read it into a struct */
#define read_struct(_path, _out) __read_struct(_path, _out, sizeof(*(_out)))

#endif /* _SMOLUTILS_IO_H */
