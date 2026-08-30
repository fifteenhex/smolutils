// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_PTS_H
#define _SMOLUTILS_PTS_H

#include <asm/ioctls.h>

static inline int smolutils_pts_open(char *slave, size_t len)
{
	int unlock = 0;
	int master;
	int idx;

	master = open("/dev/ptmx", O_RDWR | O_CLOEXEC);
	if (master < 0) {
		error("Failed to open /dev/ptmx: %d\n", errno);
		return -1;
	}

	if (ioctl(master, TIOCSPTLCK, &unlock) ||
	    ioctl(master, TIOCGPTN, &idx)) {
		error("Failed to get a pty: %d\n", errno);
		close(master);
		return -1;
	}

	snprintf(slave, len, "/dev/pts/%d", idx);

	return master;
}

#endif /* _SMOLUTILS_PTS_H */
