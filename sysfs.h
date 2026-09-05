// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_SYSFS_H
#define _SMOLUTILS_SYSFS_H

#define SYSFS_VALUE_MAX 128

static bool sysfs_read(const char *dir, const char *name,
		       char *out, size_t len)
{
	int __cleanup_fd fd = -1;
	char path[256];
	int got;

	if (snprintf(path, sizeof(path), "%s/%s", dir, name) >= (int) sizeof(path))
		return false;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return false;

	got = read(fd, out, len - 1);
	if (got <= 0)
		return false;

	out[got] = '\0';

	while (got && (out[got - 1] == '\n' || out[got - 1] == '\r'))
		out[--got] = '\0';

	return true;
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
