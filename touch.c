// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "multicall.h"
#include "nolibc_extensions/unistd.h"

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
	int c;

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

/* Cross filesystem move using sendfile() */
static int move_across(const char *src, const char *dst)
{
	int __cleanup_fd src_fd = -1;
	int __cleanup_fd dst_fd = -1;
	struct stat st;
	off_t left;

	src_fd = open(src, O_RDONLY);
	if (src_fd < 0) {
		error("Failed to open %s: %d\n", src, errno);
		return 1;
	}

	if (fstat(src_fd, &st)) {
		error("Failed to stat %s: %d\n", src, errno);
		return 1;
	}

	if (!S_ISREG(st.st_mode)) {
		error("%s isn't a regular file\n", src);
		return 1;
	}

	dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 07777);
	if (dst_fd < 0) {
		error("Failed to create %s: %d\n", dst, errno);
		return 1;
	}

	for (left = st.st_size; left > 0; ) {
		int done = sendfile(dst_fd, src_fd, NULL, left);

		if (done <= 0) {
			error("sendfile(%s) failed: %d\n", dst, errno);
			return 1;
		}

		left -= done;
	}

	if (unlink(src)) {
		error("unlink(%s) failed: %d\n", src, errno);
		return 1;
	}

	return 0;
}

static int prog_mv(int argc, char **argv, char **envp)
{
	const char *src, *dst;

	if (argc != 3) {
		usage("usage: mv <source> <target>\n");
		return 1;
	}

	src = argv[1];
	dst = argv[2];

	if (!rename(src, dst))
		return 0;

	if (errno != EXDEV) {
		error("rename(%s) failed: %d\n", dst, errno);
		return 1;
	}

	return move_across(src, dst);
}

/* no recursive support for now */
static int prog_rm(int argc, char **argv, char **envp)
{
	const char *path;
	int ret;

	if (argc != 2)
		return 1;

	path = argv[1];

	ret = unlink(path);
	if (ret) {
		error("unlink() failed: %d\n", errno);
		return 1;
	}

	return 0;
}

static int prog_rmdir(int argc, char **argv, char **envp)
{
	const char *path;
	int ret;

	if (argc != 2)
		return 1;

	path = argv[1];

	ret = rmdir(path);
	if (ret) {
		error("rmdir() failed: %d\n", errno);
		return 1;
	}

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

static const struct multicall_prog progs[] = {
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
