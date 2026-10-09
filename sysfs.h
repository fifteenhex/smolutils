// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_SYSFS_H
#define _SMOLUTILS_SYSFS_H

#define SYSFS_VALUE_MAX 128

static bool sysfs_read(const char *dir, const char *name,
		       char *out, size_t len)
{
	char path[256];

	if (!path_join(path, sizeof(path), dir, name))
		return false;

	return read_file(path, out, len);
}

static bool sysfs_read_number(const char *dir, const char *name,
			      unsigned long *out)
{
	char tmp[SYSFS_VALUE_MAX];
	char *endptr;

	if (!sysfs_read(dir, name, tmp, sizeof(tmp)))
		return false;

	*out = strtoul(tmp, &endptr, 0);

	return endptr != tmp;
}

#endif /* _SMOLUTILS_SYSFS_H */
