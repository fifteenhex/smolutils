// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_READLN_H
#define _SMOLUTILS_READLN_H

#define READLN_BUFSZ 256
#define READLN_INTERRUPTED	-1
#define READLN_OVERFLOW		-2
#define READLN_DONE		-3

static char readln_buf[READLN_BUFSZ];
static size_t readln_pending = 0;

/* Shift the tail portion of the buffer to the head */
static inline void _readln_consume(size_t howmuch)
{
	readln_pending -= howmuch;

	memmove(readln_buf, readln_buf + howmuch, readln_pending);
}

/*
 * read a line from stdin, buffer any left overs.
 * This uses a static buffer!
 *
 * >0 length of line, 0 no line yet, <0 an error.
 */
static inline int readln(char *out, size_t max)
{
	while (true) {
		size_t len;
		char *nl;
		int ret;

		nl = memchr(readln_buf, '\n', readln_pending);

		/* Buffer contains a newline? */
		if (nl)
		{
			len = nl - readln_buf;
			memcpy(out, readln_buf, len);

			/* Replace the newline for a null */
			out[len] = '\0';

			/* Consume the string and the newline */
			len++;
			_readln_consume(len);

			return len;
		}

		/* Buffer is full ? */
		if (readln_pending == sizeof(readln_buf))
			return READLN_OVERFLOW;

		/* Read new data into the buffer */
		ret = read(STDIN_FILENO, readln_buf + readln_pending,
			   sizeof(readln_buf) - readln_pending);

		/* Did we get interrupted? */
		if (ret < 0)
			return (errno == EINTR) ? READLN_INTERRUPTED : 0;

		/* End of the input, anything left is the last line */
		if (!ret)
			return READLN_DONE;

		/* Move the buffer tail and go around again */
		readln_pending += ret;
	}
}

#endif /* _SMOLUTILS_READLN_H */
