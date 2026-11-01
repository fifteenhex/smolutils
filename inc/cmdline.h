// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_CMDLINE_H
#define _SMOLUTILS_CMDLINE_H

#include "strings.h"

#define CMDLINE_PREFIX "smolinit."

static inline const char *cmdline_option(const char *arg, const char *name)
{
	size_t len = strlen(name);

	if (!STARTS_WITH(arg, CMDLINE_PREFIX))
		return NULL;

	arg += STRLEN(CMDLINE_PREFIX);

	if (strncmp(arg, name, len))
		return NULL;

	return arg + len;
}

/* The next argument of an option's value */
static inline char *cmdline_next_arg(char **pos)
{
	/* White space or comma, white space might be removed later to make things consistent */
	return __next_field(pos, " \t,");
}

#endif /* _SMOLUTILS_CMDLINE_H */
