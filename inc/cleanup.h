// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_CLEANUP_H
#define _SMOLUTILS_CLEANUP_H

static void cleanup_fd(const int *_fd)
{
	int fd = *_fd;

	if (fd >= 0)
		close(fd);
}

#define __cleanup_fd __attribute__((cleanup(cleanup_fd)))

static void cleanup_dir(DIR **_dir)
{
	DIR *dir = *_dir;

	if (dir)
		closedir(dir);
}

#define __cleanup_dir __attribute__((cleanup(cleanup_dir)))

#endif /* _SMOLUTILS_CLEANUP_H */
