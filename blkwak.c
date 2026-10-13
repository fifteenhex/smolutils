// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "blkwak"

#include "common.h"

#include <linux/fs.h>

#define SECTOR		512

#define USAGE \
	"usage: blkwak <command> <dev> ...\n" \
	"       info <dev>\n"

struct blk {
	unsigned long long size;
	int fd;
};

static void cleanup_blk(struct blk *b)
{
	if (b->fd >= 0)
		close(b->fd);
}

#define __cleanup_blk __attribute__((cleanup(cleanup_blk)))

static int open_blk(struct blk *b, const char *path, int flags)
{
	struct stat st;

	b->fd = open(path, flags);
	if (b->fd < 0) {
		error("Failed to open %s: %d\n", path, errno);
		return -1;
	}

	if (fstat(b->fd, &st)) {
		error("Failed to stat %s: %d\n", path, errno);
		return -1;
	}

	if (S_ISBLK(st.st_mode)) {
		if (ioctl(b->fd, BLKGETSIZE64, &b->size)) {
			error("Failed to size %s: %d\n", path, errno);
			return -1;
		}
	} else if (S_ISREG(st.st_mode)) {
		b->size = st.st_size;
	} else {
		error("%s isn't a block device or a file\n", path);
		return -1;
	}

	return 0;
}

static int cmd_info(struct blk *b, int argc, char **argv)
{
	int sector = 0;
	int ro = 0;

	if (argc) {
		usage(USAGE);
		return 1;
	}

	info("size %llu bytes, %llu sectors\n", b->size, b->size / SECTOR);

	if (!ioctl(b->fd, BLKSSZGET, &sector))
		info("sector size %d\n", sector);
	if (!ioctl(b->fd, BLKROGET, &ro))
		info("%s\n", ro ? "read-only" : "writable");

	return 0;
}

struct command {
	const char *name;
	int (*run)(struct blk *b, int argc, char **argv);
	bool writes;
};

static const struct command commands[] = {
	{ "info", cmd_info, false },
};

int main (int argc, char **argv, char **envp)
{
	struct blk __cleanup_blk b = { .fd = -1 };
	const struct command *cmd = NULL;

	if (argc < 3) {
		usage(USAGE);
		return 1;
	}

	foreach(cmdp, commands)
		if (!strcmp(cmdp->name, argv[1]))
			cmd = cmdp;

	if (!cmd) {
		error("Unknown command: %s\n", argv[1]);
		return 1;
	}

	if (open_blk(&b, argv[2], cmd->writes ? O_RDWR : O_RDONLY))
		return 1;

	return cmd->run(&b, argc - 3, argv + 3);
}
