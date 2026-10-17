// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_KERNFS_H
#define _SMOLUTILS_KERNFS_H

#include "fs.h"

/* Single line files holding a number, as /proc and /sys have */
static inline bool __read_number_in(const char *dir, const char *name,
				    int base, unsigned long *out)
{
	char tmp[128];
	char *end;

	if (!read_file_in(dir, name, tmp, sizeof(tmp)))
		return false;

	*out = strtoul(tmp, &end, base);

	return end != tmp;
}

static inline bool read_number_in(const char *dir, const char *name,
				  unsigned long *out)
{
	return __read_number_in(dir, name, 0, out);
}

static inline bool read_hex_number_in(const char *dir, const char *name,
				      unsigned long *out)
{
	return __read_number_in(dir, name, 16, out);
}

static inline bool write_number_in(const char *dir, const char *name,
				   unsigned long value)
{
	char path[256];
	char buf[32];
	int len;

	if (!path_join(path, sizeof(path), dir, name))
		return false;

	len = snprintf(buf, sizeof(buf), "%lu\n", value);

	return !write_file(path, buf, len);
}

#endif /* _SMOLUTILS_KERNFS_H */
