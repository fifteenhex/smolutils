// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_PATHS_H
#define _SMOLUTILS_PATHS_H

static inline bool path_join(char *buf, size_t len, const char *dir,
			     const char *name)
{
	return (size_t) snprintf(buf, len, "%s/%s", dir, name) < len;
}

static inline const char *path_basename(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

#endif /* _SMOLUTILS_PATHS_H */
