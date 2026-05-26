// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "multicall.h"

static int prog_touch(int argc, char **argv, char **envp)
{
	const char *path;
	int __cleanup_fd fd = -1;

	if (argc != 2)
		return 1;

	path = argv[1];

	/* File doesn't exist, try to create it */
	if (access(path, F_OK)) {
		fd = creat(path, 0644);
		if (fd < 0) {
			error("Failed to create file\n");
			return 1;
		}
	}
	/* File exists, update timestamp(s) */
	else {
#if 0 // utime/utimes is missing?
		if (utime(path, NULL)) {
			error("Failed to update timestamps\n");
			return 1;
		}
#endif
	}

	return 0;
}

static int prog_ln(int argc, char **argv, char **envp)
{
	const char *target, *linkpath;
	bool symbolic = false;
	int ret;
	char c;

	while ((c = getopt(argc, argv, "s")) != -1) {
		switch (c) {
                case 's':
			symbolic = true;
                        break;
                }
        }

	target = (optind < argc) ? argv[optind++] : NULL;
	if (!target)
		return 1;

	linkpath = (optind < argc) ? argv[optind++] : NULL;
	if (!linkpath)
		return 1;

	ret = symbolic ? symlink(target, linkpath) : link(target, linkpath);
	if (ret) {
		error("ln() failed: %d\n", errno);
		return 1;
	}

	return 0;
}

static int prog_mv(int argc, char **argv, char **envp)
{
	return 0;
}

static int prog_rm(int argc, char **argv, char **envp)
{
	return 0;
}

static int prog_rmdir(int argc, char **argv, char **envp)
{
	return 0;
}

static int prog_mkdir(int argc, char **argv, char **envp)
{
	const char *path;
	int ret;

	if (argc != 2)
		return 1;

	path = argv[1];

	ret = mkdir(path, 0755);
	if (ret) {
		error("mkdir() failed: %d\n", errno);
		return 1;
	}

	return 0;
}

static const struct mutlicall_prog progs[] = {
	{ "touch", prog_touch },
	{ "ln", prog_ln },
	{ "mv", prog_mv },
	{ "mkdir", prog_mkdir },
	{ "rm", prog_rm },
	{ "rmdir", prog_rmdir },
};

int main (int argc, char **argv, char **envp)
{
	MULTICALL_DISPATCH(argv[0], progs);

	return 1;
}
