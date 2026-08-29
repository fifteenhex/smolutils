// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_MEMFD_H
#define _SMOLUTILS_MEMFD_H

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
#endif  /* _SMOLUTILS_MEMFD_H */
