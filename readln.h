// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_READLN_H
#define _SMOLUTILS_READLN_H

#define READLN_BUFSZ 256
#define READLN_INTERRUPTED	-1
#define READLN_OVERFLOW		-2
#define READLN_DONE		-3

enum readln_action {
	READLN_KEEP,
	READLN_DROP,
	/* Discard remainder of line */
	READLN_EOL,
	/* Treat like a new line */
	READLN_SPLIT,
	/* Keep as a hint for the next character */
	READLN_NEXT,
};

struct readln_filter {
	const char *chars;
	enum readln_action (*handle)(char *ch, void *state);
	void *state;
};

static char readln_buf[READLN_BUFSZ];
static size_t readln_pending = 0;

/* Shift the tail portion of the buffer to the head */
static inline void _readln_consume(size_t howmuch)
{
	readln_pending -= howmuch;

	memmove(readln_buf, readln_buf + howmuch, readln_pending);
}

/* Copy a line from in to out while running it through filter */
static inline size_t _readln_copy(const struct readln_filter *filter,
				  const char *in, char *out, size_t len,
				  size_t *eaten)
{
	bool asked = false;
	size_t used = 0;
	size_t i;

	*eaten = len;

	if (!filter) {
		memcpy(out, in, len);

		return len;
	}

	for (i = 0; i < len; i++) {
		char ch = in[i];

		if (asked || (ch && strchr(filter->chars, ch))) {
			enum readln_action act;

			act = filter->handle(&ch, filter->state);
			asked = act == READLN_NEXT;

			if (act == READLN_EOL)
				break;

			if (act == READLN_SPLIT) {
				*eaten = i + 1;
				break;
			}

			if (act == READLN_DROP)
				continue;
		}

		out[used++] = ch;
	}

	return used;
}

/*
 * read a line from stdin, buffer any left overs.
 * This uses a static buffer!
 *
 * >0 length of line, 0 an empty one, <0 the end of the input or an error.
 */
static inline int readln(char *out, size_t max,
			 const struct readln_filter *filter)
{
	while (true) {
		size_t consume_len;
		size_t copy_len;
		size_t eaten;
		char *newline;
		int ret;

		newline = memchr(readln_buf, '\n', readln_pending);

		/* Buffer contains a newline? */
		if (newline)
		{
			consume_len = newline - readln_buf;

			if (consume_len > max) {
				_readln_consume(consume_len + 1);
				return READLN_OVERFLOW;
			}

			copy_len = _readln_copy(filter, readln_buf, out,
						consume_len, &eaten);

			/* Replace the newline for a null */
			out[copy_len] = '\0';

			if (eaten == consume_len)
				eaten++;

			_readln_consume(eaten);

			return copy_len;
		}

		/* Buffer is full ? */
		if (readln_pending == sizeof(readln_buf))
			return READLN_OVERFLOW;

		/* Read new data into the buffer */
		ret = read(STDIN_FILENO, readln_buf + readln_pending,
			   sizeof(readln_buf) - readln_pending);

		/* Did we get interrupted? */
		if (ret < 0)
			return (errno == EINTR) ? READLN_INTERRUPTED : READLN_DONE;

		/* End of the input, anything left is the last line */
		if (!ret)
			return READLN_DONE;

		/* Move the buffer tail and go around again */
		readln_pending += ret;
	}
}

struct readln_shell_state {
	bool quoted;
	bool escaped;
};

/* readln filter for smolsh */
static inline enum readln_action _readln_shell_char(char *ch, void *state)
{
	struct readln_shell_state *s = state;

	/* Escaping was trigger, short circuit */
	if (s->escaped) {
		s->escaped = false;

		return READLN_KEEP;
	}

	switch (*ch) {
	case '"':
		s->quoted = !s->quoted;
		break;

	/* Nothing is special after this one, so ask about it */
	case '\\':
		if (!s->quoted) {
			s->escaped = true;

			return READLN_NEXT;
		}
		break;

	/* Comment */
	case '#':
		if (!s->quoted)
			return READLN_EOL;
		break;

	/* Strip tabs */
	case '\t':
		if (!s->quoted)
			*ch = ' ';
		break;

	/* Strip carriage returns */
	case '\r':
		return READLN_DROP;

	/* Multiple commands on one line, process one at a time */
	case ';':
		if (!s->quoted)
			return READLN_SPLIT;
		break;
	}

	return READLN_KEEP;
}

/* Pre-processor for shell lines */
static inline int readln_shell(char *out, size_t max)
{
	struct readln_shell_state state = { 0 };
	const struct readln_filter filter = {
		.chars = "\"\\#\t\r;",
		.handle = _readln_shell_char,
		.state = &state,
	};

	return readln(out, max, &filter);
}

#endif /* _SMOLUTILS_READLN_H */
