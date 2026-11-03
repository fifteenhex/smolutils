// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_NONBLOCKINGIO_H
#define _SMOLUTILS_NONBLOCKINGIO_H

#include "ringbuf.h"

/* A non-blocking fd with a queue, so a full fd can't block the loop */
struct nonblockingio {
	int fd;
	struct ringbuf *queue;
};

/* Give it its queue, there is no fd yet */
static inline void nonblockingio_init(struct nonblockingio *b,
				      struct ringbuf *queue)
{
	b->fd = -1;
	b->queue = queue;
}

/* Serve fd from now on, made non-blocking, -1 when that fails */
static inline int nonblockingio_attach(struct nonblockingio *b, int fd)
{
	int flags = fcntl(fd, F_GETFL);

	if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK))
		return -1;

	b->fd = fd;

	return 0;
}

static inline unsigned int nonblockingio_pending(const struct nonblockingio *b)
{
	return ringbuf_pending(b->queue);
}

static inline unsigned int nonblockingio_room(const struct nonblockingio *b)
{
	return ringbuf_room(b->queue);
}

/* What to ask poll() for on the fd besides POLLIN */
static inline short nonblockingio_events(const struct nonblockingio *b)
{
	return ringbuf_pending(b->queue) ? POLLOUT : 0;
}

/* Write what the fd takes now, the next POLLOUT takes the rest */
static inline int nonblockingio_drain(struct nonblockingio *b)
{
	const char *at;
	unsigned int run = ringbuf_span(b->queue, &at);
	int done;

	if (!run)
		return 0;

	done = write(b->fd, at, run);
	if (done < 0)
		return errno == EAGAIN ? 0 : -1;

	ringbuf_nomnom(b->queue, done);

	return 0;
}

/* put a byte into the queue */
static inline void nonblockingio_put(struct nonblockingio *b, char c)
{
	ringbuf_put(b->queue, c);
}

/* Queue the data behind what is waiting and write what goes now */
static inline int nonblockingio_write(struct nonblockingio *b,
				      const void *data, unsigned int len)
{
	if (ringbuf_add(b->queue, data, len))
		return -1;

	return nonblockingio_drain(b);
}

/* Read, nothing there yet is 0 bytes, not an error */
static inline int nonblockingio_read(struct nonblockingio *b, void *data,
				     unsigned int len)
{
	int got = read(b->fd, data, len);

	if (got < 0 && errno == EAGAIN)
		return 0;

	return got;
}

/* Wait for the pending data to go, up to timeout ms, -1 if it didn't */
static inline int nonblockingio_flush(struct nonblockingio *b, int timeout)
{
	struct pollfd pfd = { .fd = b->fd, .events = POLLOUT };

	while (ringbuf_pending(b->queue)) {
		if (poll(&pfd, 1, timeout) <= 0)
			return -1;
		if (nonblockingio_drain(b))
			return -1;
	}

	return 0;
}

#endif /* _SMOLUTILS_NONBLOCKINGIO_H */
