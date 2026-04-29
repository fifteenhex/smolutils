// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_COMMON_H
#define _SMOLUTILS_COMMON_H

#define ARRAY_SIZE(_a) (sizeof(_a) / sizeof(_a[0]))

#define STRLEN(_s) (ARRAY_SIZE(_s) - 1)

/* Printing stuff */

#ifdef CONFIG_DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

/* File stuff */

off_t file_size(int fd) {
	struct stat st;

	if (fstat(fd, &st) == -1)
		return -1;

	return st.st_size;
}

#endif /* _SMOLUTILS_COMMON_H */
