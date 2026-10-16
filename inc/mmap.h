// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_MMAP_H
#define _SMOLUTILS_MMAP_H

#include "util.h"

struct mapping {
	/* MAP_FAILED when there is no mapping to clean up */
	void *mapped;
	size_t sz;

	/*
	 * The mapping will be aligned to page size,
	 * this points to the requested start.
	 */
	void *at;
};

#define MAPPING_NONE { .mapped = MAP_FAILED }

static void cleanup_mapping(struct mapping *m)
{
	if (m->mapped != MAP_FAILED)
		munmap(m->mapped, m->sz);
}

#define __cleanup_mapping __attribute__((cleanup(cleanup_mapping)))

/* Do mmap() but also handle the alignment stuff etc */
static inline bool mapping_map(struct mapping *m, int fd, off_t offset,
			       size_t len, int prot, int flags)
{
	unsigned long page = getpagesize();
	off_t base = round_down(offset, page);
	size_t into = offset - base;

	m->sz = round_up(into + len, page);

	m->mapped = mmap(NULL, m->sz, prot, flags, fd, base);
	if (m->mapped == MAP_FAILED)
		return false;

	m->at = (char *) m->mapped + into;

	return true;
}

/* Get a pointer into the map */
static inline void *map_at(const struct mapping *m, size_t off)
{
	return (char *) m->at + off;
}

#endif /* _SMOLUTILS_MMAP_H */
