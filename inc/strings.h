// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_STRINGS_H
#define _SMOLUTILS_STRINGS_H

#include "util.h"

#define STRLEN(_s) (ARRAY_SIZE(_s) - 1)

/* Does a string start with this char array? */
#define STARTS_WITH(_string, _chararray) \
	(strncmp(_string, _chararray, STRLEN(_chararray)) == 0)

/* get the next whitespace separated field from a string and terminate it */
static inline char *next_field(char **pos)
{
	char *start = *pos;
	char *end;

	while (*start == ' ' || *start == '\t')
		start++;

	if (!*start)
		return NULL;

	for (end = start; *end && *end != ' ' && *end != '\t'; end++)
		;

	*pos = *end ? end + 1 : end;
	*end = '\0';

	return start;
}

/* replace every from with to, returns the number of chars changed */
static inline unsigned int str_replace_char(char *s, char from, char to)
{
	unsigned int replaced = 0;

	for (; *s; s++) {
		if (*s != from)
			continue;

		*s = to;
		replaced++;
	}

	return replaced;
}

/* Did snprintf() work or not? */
static inline bool __str_fits(int n, size_t len)
{
	return n >= 0 && (size_t) n < len;
}

/* Use snprintf() to create a string in an array with bounds checking! */
#define str_format(_array, ...)						\
	__str_fits(snprintf(_array, sizeof(_array) + __must_be_array(_array),	\
			    __VA_ARGS__), sizeof(_array))

#endif /* _SMOLUTILS_STRINGS_H */
