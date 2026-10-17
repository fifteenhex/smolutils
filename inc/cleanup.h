// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_CLEANUP_H
#define _SMOLUTILS_CLEANUP_H

#include "util.h"

static void cleanup_fd(const int *_fd)
{
	int fd = *_fd;

	if (fd >= 0)
		close(fd);
}

#define __cleanup_fd __attribute__((cleanup(cleanup_fd)))

/*
 * Take a reference to an fd that you don't want cleaned up,
 * fd cannot be the variable you place the return value into!
 */
static inline __warn_if_unused int keep_fd(int *fd)
{
	int kept = *fd;

	*fd = -1;

	return kept;
}

static void cleanup_dir(DIR **_dir)
{
	DIR *dir = *_dir;

	if (dir)
		closedir(dir);
}

#define __cleanup_dir __attribute__((cleanup(cleanup_dir)))

static void cleanup_free(void *_p)
{
	/*
	 * This is confusing but _p is really a ** but because of C
	 * it needs to be a * in the function signature as otherwise
	 * using this with uint8_t *xyz doesn't work.
	 */

	void **p = _p;

	free(*p);
}

#define __cleanup_free __attribute__((cleanup(cleanup_free)))

#endif /* _SMOLUTILS_CLEANUP_H */
