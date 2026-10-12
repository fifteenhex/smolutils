// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_DUNGEON_H
#define _SMOLUTILS_DUNGEON_H

/* State directory for managed, backgrounded processes */
#define DUNGEON_DIR	SMOL_RUN_DIR "/dungeon"

/* create pre-process directory */
static inline int dungeon_make_dir(char *dir, size_t len, const char *path)
{
	if (mkdir(DUNGEON_DIR, 0755) && errno != EEXIST) {
		error("Failed to mkdir(%s): %d\n", DUNGEON_DIR, errno);
		return -1;
	}

	if (!path_join(dir, len, DUNGEON_DIR, path_basename(path))) {
		error("Name too long: %s\n", path);
		return -1;
	}

	if (mkdir(dir, 0755) && errno != EEXIST) {
		error("Failed to mkdir(%s): %d\n", dir, errno);
		return -1;
	}

	return 0;
}

#endif /* _SMOLUTILS_DUNGEON_H */
