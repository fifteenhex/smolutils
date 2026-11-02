// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_RINGBUF_H
#define _SMOLUTILS_RINGBUF_H

#include "util.h"

struct ringbuf {
	unsigned int size;
	unsigned int start;
	unsigned int len;
	char buf[] __attribute__((counted_by(size)));
};

/* Use this to declare a ringbuf inside your structs */
#define RINGBUF(_name, _size)						\
	union {								\
		_Static_assert((_size) > 0 &&				\
			       !((_size) & ((_size) - 1)),		\
			       "ringbuf size isn't a power of two");	\
		struct ringbuf ring;					\
		char bytes[offsetof(struct ringbuf, buf) + (_size)];	\
	} _name

static inline __attribute__((nonnull, returns_nonnull))
struct ringbuf *__ringbuf_init(struct ringbuf *r, unsigned int size)
{
	r->size = size;
	r->start = 0;
	r->len = 0;

	return r;
}

#define ringbuf_init(_s)						\
	__ringbuf_init(&(_s)->ring,					\
		       sizeof((_s)->bytes) - offsetof(struct ringbuf, buf))

static inline __attribute__((nonnull))
unsigned int ringbuf_pending(const struct ringbuf *r)
{
	return r->len;
}

static inline __attribute__((nonnull))
unsigned int ringbuf_room(const struct ringbuf *r)
{
	return r->size - r->len;
}

/*
 * Returns how many contiguous bytes there are and deposits a pointer into
 * at so you can access them. nomnom the amount consumed once you're done.
 */
static inline __attribute__((nonnull))
unsigned int ringbuf_span(const struct ringbuf *r, const char **at)
{
	unsigned int run = r->size - r->start;

	*at = r->buf + r->start;

	return run > r->len ? r->len : run;
}

static inline __attribute__((nonnull))
void ringbuf_nomnom(struct ringbuf *r, unsigned int len)
{
	r->start = (r->start + len) & (r->size - 1);
	r->len -= len;
}

static inline __warn_if_unused
__attribute__((nonnull, access(read_only, 2, 3)))
int ringbuf_add(struct ringbuf *r, const void *data, unsigned int len)
{
	unsigned int end = (r->start + r->len) & (r->size - 1);
	unsigned int run = r->size - end;

	if (len > ringbuf_room(r)) {
		errno = ENOBUFS;
		return -1;
	}

	if (run > len)
		run = len;

	memcpy(r->buf + end, data, run);
	memcpy(r->buf, (const char *) data + run, len - run);
	r->len += len;

	return 0;
}

#endif /* _SMOLUTILS_RINGBUF_H */
