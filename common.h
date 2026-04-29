// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_COMMON_H
#define _SMOLUTILS_COMMON_H

#define ARRAY_SIZE(_a) (sizeof(_a) / sizeof(_a[0]))

#define STRLEN(_s) (ARRAY_SIZE(_s) - 1)

/* Printing stuff */

#ifdef CONFIG_DEBUG
#define debug(...) printf(__VA_ARGS__)
/*
 * VERBOSE is for really noisy messages that you
 * probably don't want to enable globally, define
 * this in the file you are debugging.
 */
#ifdef VERBOSE
#define verbose(...) printf(__VA_ARGS__)
#else
#define verbose(...)
#endif
#else
#define debug(...)
#define verbose(...)
#endif

#define error(...) printf(__VA_ARGS__)

/* File stuff */

off_t file_size(int fd) {
	struct stat st;

	if (fstat(fd, &st) == -1)
		return -1;

	return st.st_size;
}

/* String matching */

/* Does a string start with this char array? */
#define STARTS_WITH(_string, _chararray) \
	(strncmp(_string, _chararray, STRLEN(_chararray)) == 0)

#endif /* _SMOLUTILS_COMMON_H */
