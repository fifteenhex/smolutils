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

static inline void dungeon_close_stdio(int *fds)
{
	int i;

	for (i = 0; i < 3; i++)
		if (fds[i] >= 0)
			close(fds[i]);
}

/*
 * create stdin/stdout/stderr for a process,
 * this might need to be reconsidered later to avoid a process
 * filling the tmpfs.
 */
static inline int dungeon_open_stdio(const char *dir, int *fds)
{
	char path[160];

	fds[0] = open("/dev/null", O_RDONLY);

	path_join(path, sizeof(path), dir, "out");
	fds[1] = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	path_join(path, sizeof(path), dir, "err");
	fds[2] = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (fds[0] < 0 || fds[1] < 0 || fds[2] < 0) {
		error("Failed to open stdio in %s: %d\n", dir, errno);
		dungeon_close_stdio(fds);
		return -1;
	}

	return 0;
}

static inline pid_t dungeon_read_pid(const char *dir)
{
	unsigned long pid;

	if (!read_number_in(dir, "pid", &pid))
		return -ENOENT;

	return pid;
}

/* ping a pid, EPERM means the pid is valid but its not ours */
static inline bool dungeon_alive(pid_t pid)
{
	if (pid <= 0)
		return false;

	return !kill(pid, 0) || errno == EPERM;
}

#endif /* _SMOLUTILS_DUNGEON_H */
