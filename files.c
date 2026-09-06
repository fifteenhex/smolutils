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

static int cat_fd(int fd)
{
	char buf[4096];
	int len;

	while (true) {
		len = read(fd, buf, sizeof(buf));
		if (len < 0)
			return -1;
		if (len == 0)
			return 0;

		/* FIXME: For now write byte by byte because of nolibc's fwrite() */
		fwrite(buf, 1, len, stdout);
	}
}

static int prog_cat(int argc, char **argv, char **envp)
{
	int ret = 0;
	int i;

	/* No arguments means read stdin */
	if (argc < 2)
		return cat_fd(STDIN_FILENO) ? 1 : 0;

	for (i = 1; i < argc; i++) {
		int __cleanup_fd fd = -1;

		fd = open(argv[i], O_RDONLY);
		if (fd < 0) {
			error("Failed to open %s: %d\n", argv[i], errno);
			ret = 1;
			continue;
		}

		if (cat_fd(fd)) {
			error("Failed to read %s: %d\n", argv[i], errno);
			ret = 1;
		}
	}

	return ret;
}

static int copy_a_file(const char *src, const char *dst)
{
	int __cleanup_fd src_fd = -1;
	int __cleanup_fd dst_fd = -1;
	off_t sz;
	int ret;

	debug("copying %s to %s\n", src, dst);

	src_fd = open(src, O_RDONLY);
	if (src_fd < 0) {
		error("Failed to open %s: %d\n", src, errno);
		return -1;
	}

	sz = file_size(src_fd);
	if (sz < 0) {
		error("Failed to size %s: %d\n", src, errno);
		return -1;
	}

	dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (dst_fd < 0) {
		error("Failed to create %s: %d\n", dst, errno);
		return -1;
	}

	debug("Calling sendfile() to copy %lld bytes\n", (long long) sz);

	ret = sendfile(dst_fd, src_fd, NULL, sz);
	if (ret != sz) {
		error("Failed to copy %s: %d\n", src, errno);
		return -1;
	}

	return 0;
}

static int prog_cp(int argc, char **argv, char **envp)
{
	/* super dumb for now */
	if (argc != 3)
		return 1;

	if (copy_a_file(argv[1], argv[2]))
		return 1;

	return 0;
}

static int prog_chmod(int argc, char **argv, char **envp)
{
	unsigned long mode;
	char *end;
	int ret = 0;
	int i;

	if (argc < 3) {
		usage("usage: chmod <octal mode> <file>...\n");
		return 1;
	}

	/* Only the octal form, nobody needs u+x that badly */
	mode = strtoul(argv[1], &end, 8);
	if (end == argv[1] || *end != '\0' || mode > 07777) {
		error("Not a mode: %s\n", argv[1]);
		return 1;
	}

	for (i = 2; i < argc; i++) {
		if (chmod(argv[i], mode)) {
			error("chmod(%s) failed: %d\n", argv[i], errno);
			ret = 1;
		}
	}

	return ret;
}

static int prog_chown(int argc, char **argv, char **envp)
{
	gid_t gid = (gid_t) -1;
	uid_t uid;
	char *end;
	int ret = 0;
	int i;

	if (argc < 3) {
		usage("usage: chown <uid>[:<gid>] <file>...\n");
		return 1;
	}

	uid = strtoul(argv[1], &end, 10);
	if (end == argv[1]) {
		error("Not a uid: %s\n", argv[1]);
		return 1;
	}

	if (*end == ':') {
		char *group = end + 1;

		gid = strtoul(group, &end, 10);
		if (end == group) {
			error("Not a gid: %s\n", group);
			return 1;
		}
	}

	if (*end != '\0') {
		error("Not a uid: %s\n", argv[1]);
		return 1;
	}

	for (i = 2; i < argc; i++) {
		if (chown(argv[i], uid, gid)) {
			error("chown(%s) failed: %d\n", argv[i], errno);
			ret = 1;
		}
	}

	return ret;
}

static const struct multicall_prog progs[] = {
	{ "touch", prog_touch },
	{ "ln", prog_ln },
	{ "mv", prog_mv },
	{ "mkdir", prog_mkdir },
	{ "rm", prog_rm },
	{ "rmdir", prog_rmdir },
	{ "cat", prog_cat },
	{ "cp", prog_cp },
	{ "chmod", prog_chmod },
	{ "chown", prog_chown },
};

int main (int argc, char **argv, char **envp)
{
	MULTICALL_DISPATCH(argv[0], progs);

	return 1;
}
