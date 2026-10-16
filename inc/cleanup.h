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

#endif /* _SMOLUTILS_CLEANUP_H */
