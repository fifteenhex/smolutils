// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

static const char *proc_path = "/proc";

static void print_process(const char *pid, const char *comm_path)
{
	char tmp[1024];
	int len;
	int fd;

	fd = open(comm_path, O_RDONLY);

	len = read(fd, tmp, sizeof(tmp));

	tmp[len - 1] = '\0';

	printf("%s\t\t%s\n", pid, tmp);

	close(fd);
}

static int cb(const char *name, int dir, void *priv)
{
	char tmp[1024];

	if (strcmp(name, "self") == 0)
		return 0;

	if (strcmp(name, "thread-self") == 0)
		return 0;

	sprintf(tmp, "%s/%s/comm", proc_path, name);

	/* If there isn't a comm file then this isn't a process? */
	if (access(tmp, F_OK))
		return 0;

	print_process(name, tmp);
}

int main (int argc, char **argv, char **envp)
{
	printf("PID\t\tCMD\n");

	iterate_dir(proc_path, cb, NULL);

	return 0;
}
