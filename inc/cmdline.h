// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_CMDLINE_H
#define _SMOLUTILS_CMDLINE_H

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

#endif /* _SMOLUTILS_CMDLINE_H */
