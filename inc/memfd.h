// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_MEMFD_H
#define _SMOLUTILS_MEMFD_H

#include "cleanup.h"
#include "io.h"

static inline int memfd_create_and_size(const char *name, int *memfd, size_t sz)
{

	int ret;

	ret = memfd_create(name, 0);
	if (ret < 0)
		return ret;

	if (sz != 0)
		ftruncate(ret, sz);

	*memfd = ret;

	return 0;
}

static inline int memfd_from_buffer(const char *name, const void *buf,
				    size_t len)
{
	int __cleanup_fd memfd = -1;

	if (memfd_create_and_size(name, &memfd, len))
		return -1;

	if (write_full(memfd, buf, len))
		return -1;

	if (lseek(memfd, 0, SEEK_SET) < 0)
		return -1;

	return keep_fd(&memfd);
}

#define FD_ARG_LEN 12

static inline void fd_to_arg(char arg[FD_ARG_LEN], int fd)
{
	snprintf(arg, FD_ARG_LEN, "%d", fd);
}

#endif  /* _SMOLUTILS_MEMFD_H */
