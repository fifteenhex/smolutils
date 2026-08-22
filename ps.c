// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "users.h"

static const char *proc_path = "/proc";

static void print_user(uid_t uid)
{
	const char *user = users_map_user(uid);

	if (user)
		printf("%s", user);
	else
		printf("%u", (unsigned int) uid);
}

static void print_process(const char *pid, const char *comm_path, uid_t uid)
{
	char tmp[1024];
	int len;
	int __cleanup_fd fd;

	fd = open(comm_path, O_RDONLY);
	if (fd < 0)
		return;

	len = read(fd, tmp, sizeof(tmp) - 1);
	if (len <= 0)
		return;

	/* Drop the trailing newline */
	if (tmp[len - 1] == '\n')
		len--;
	tmp[len] = '\0';

	print_user(uid);
	printf("\t\t%s\t\t%s\n", pid, tmp);
}

static int cb(const char *name, int dir, void *priv)
{
	char tmp[1024];
	struct stat st;

	if (strcmp(name, "self") == 0)
		return 0;

	if (strcmp(name, "thread-self") == 0)
		return 0;

	sprintf(tmp, "%s/%s/comm", proc_path, name);

	/* If there isn't a comm file then this isn't a process? */
	if (access(tmp, F_OK))
		return 0;

	if (fstatat(dir, name, &st, 0))
		return 0;

	print_process(name, tmp, st.st_uid);

	return 0;
}

int main (int argc, char **argv, char **envp)
{
	printf("USER\t\tPID\t\tCMD\n");

	iterate_dir(proc_path, cb, NULL);

	return 0;
}
