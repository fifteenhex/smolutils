// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_IO_H
#define _SMOLUTILS_IO_H

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

#endif /* _SMOLUTILS_IO_H */
