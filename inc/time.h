// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_TIME_H
#define _SMOLUTILS_TIME_H

static inline struct timespec deadline_after(unsigned int secs)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	t.tv_sec += secs;

	return t;
}

static inline bool deadline_left(const struct timespec *deadline, struct timeval *left)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	left->tv_sec = deadline->tv_sec - now.tv_sec;
	left->tv_usec = (deadline->tv_nsec - now.tv_nsec) / 1000;
	if (left->tv_usec < 0) {
		left->tv_sec--;
		left->tv_usec += 1000000;
	}

	return left->tv_sec > 0 || (left->tv_sec == 0 && left->tv_usec > 0);
}

#endif /* _SMOLUTILS_TIME_H */
