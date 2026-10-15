// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_DUNGEON_H
#define _SMOLUTILS_DUNGEON_H

/* State directory for managed, backgrounded processes */
#define DUNGEON_DIR	SMOL_RUN_DIR "/dungeon"

/* These might need tweaking in the future, these will be passed by the kernel cmdline */
#define DUNGEON_ARGS	16
#define DUNGEON_CMD_MAX	256

/* create pre-process directory */
static inline int dungeon_make_dir(char *dir, size_t len, const char *path)
{
	if (mk_chmod_dir(DUNGEON_DIR, 0755))
		return -1;

	if (!path_join(dir, len, DUNGEON_DIR, path_basename(path))) {
		error("Name too long: %s\n", path);
		return -1;
	}

	if (mk_chmod_dir(dir, 0755))
		return -1;

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

static inline pid_t dungeon_read_keptby(const char *dir)
{
	unsigned long pid;

	if (!read_number_in(dir, "keptby", &pid))
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

/* Save argv one line per entry */
static inline void dungeon_write_cmd(const char *dir, char * const argv[])
{
	char buf[DUNGEON_CMD_MAX];
	char path[160];
	size_t len = 0;
	int i;

	for (i = 0; argv[i]; i++) {
		int n = snprintf(buf + len, sizeof(buf) - len, "%s\n", argv[i]);

		if (n < 0 || len + n >= sizeof(buf)) {
			error("Command too long for %s\n", dir);
			return;
		}

		len += n;
	}

	path_join(path, sizeof(path), dir, "cmd");
	write_file(path, buf, len);
}

/* Load argv from one line per entry */
static inline int dungeon_read_cmd(const char *dir, char *buf, size_t len,
				   char **argv, int max)
{
	char path[160];
	char *p = buf;
	int num = 0;

	path_join(path, sizeof(path), dir, "cmd");

	if (!read_file(path, buf, len))
		return -1;

	while (*p) {
		char *nl = strchr(p, '\n');

		if (num == max)
			return -1;

		argv[num++] = p;

		if (!nl)
			break;

		*nl = '\0';
		p = nl + 1;
	}

	argv[num] = NULL;

	return num;
}

/* spawn a process inside a dungeon */
static inline pid_t dungeon_spawn(const char *dir, char * const argv[],
				  char * const envp[], bool detach)
{
	int fds[3];
	pid_t pid;

	if (dungeon_open_stdio(dir, fds))
		return -1;

	if (detach)
		pid = spawn_detached(argv[0], argv, envp, fds);
	else
		pid = spawn_redirect(argv[0], argv, envp, fds);

	dungeon_close_stdio(fds);

	if (pid < 0) {
		error("Failed to run %s: %d\n", argv[0], errno);
		return -1;
	}

	dungeon_write_cmd(dir, argv);
	write_number_in(dir, "pid", pid);

	/* What pid is looking after this process */
	write_number_in(dir, "keptby", detach ? 0 : getpid());

	return pid;
}

#endif /* _SMOLUTILS_DUNGEON_H */
