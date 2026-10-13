// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "blkwak"

#include "common.h"

#include <linux/fs.h>

#define DEFAULT_BS	(64 * 1024)
#define MAX_BS		(1024 * 1024)
#define SECTOR		512

#define USAGE \
	"usage: blkwak [-b <bs>] <command> <dev> ...\n" \
	"       info <dev>\n" \
	"       read <dev> <file> [off] [len]      device -> file\n" \
	"       -b read/write buffer size\n"

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

static unsigned int bs = DEFAULT_BS;
static uint8_t *buf;

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

/* parse [off] [len] */
static bool parse_region(const struct blk *b, int argc, char **argv,
			 unsigned long long *off, unsigned long long *len)
{
	*off = 0;
	*len = 0;

	if (argc > 0 && !parse_size(argv[0], off)) {
		error("Not an offset: %s\n", argv[0]);
		return false;
	}

	if (argc > 1 && !parse_size(argv[1], len)) {
		error("Not a length: %s\n", argv[1]);
		return false;
	}

	if (*off > b->size) {
		error("0x%llx is past the end\n", *off);
		return false;
	}

	if (!*len)
		*len = b->size - *off;

	if (*off + *len > b->size) {
		error("0x%llx + 0x%llx is past the end\n", *off, *len);
		return false;
	}

	return true;
}

static int read_at(const struct blk *b, unsigned long long off, unsigned int n)
{
	int got;

	if (lseek(b->fd, off, SEEK_SET) != (off_t) off) {
		error("Failed to seek to 0x%llx: %d\n", off, errno);
		return -1;
	}

	got = read_full(b->fd, buf, n);
	if (got != (int) n) {
		error("Short read at 0x%llx: wanted %u got %d, errno %d\n",
		      off, n, got, errno);
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

static int cmd_read(struct blk *b, int argc, char **argv)
{
	unsigned long long done = 0;
	int __cleanup_fd out = -1;
	unsigned long long off;
	unsigned long long len;

	if (argc < 1 || argc > 3 ||
	    !parse_region(b, argc - 1, argv + 1, &off, &len)) {
		usage(USAGE);
		return 1;
	}

	out = open(argv[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (out < 0) {
		error("Failed to open %s: %d\n", argv[0], errno);
		return 1;
	}

	while (done < len) {
		unsigned int n = len - done > bs ? bs : len - done;

		if (read_at(b, off + done, n))
			return 1;

		if (write_full(out, buf, n)) {
			error("Failed to write %s: %d\n", argv[0], errno);
			return 1;
		}

		done += n;
	}

	info("read %llu bytes\n", done);

	return 0;
}

struct command {
	const char *name;
	int (*run)(struct blk *b, int argc, char **argv);
	bool writes;
};

static const struct command commands[] = {
	{ "info", cmd_info, false },
	{ "read", cmd_read, false },
};

int main (int argc, char **argv, char **envp)
{
	struct blk __cleanup_blk b = { .fd = -1 };
	const struct command *cmd = NULL;
	unsigned long long v;
	int c;

	while ((c = getopt(argc, argv, "b:")) != -1) {
		switch (c) {
		case 'b':
			if (!parse_size(optarg, &v) || !v || v > MAX_BS) {
				error("Not a buffer size: %s\n", optarg);
				return 1;
			}
			bs = v;
			break;

		default:
			usage(USAGE);
			return 1;
		}
	}

	if (argc - optind < 2) {
		usage(USAGE);
		return 1;
	}

	foreach(cmdp, commands)
		if (!strcmp(cmdp->name, argv[optind]))
			cmd = cmdp;

	if (!cmd) {
		error("Unknown command: %s\n", argv[optind]);
		return 1;
	}

	buf = malloc(bs);
	if (!buf) {
		error("No room for a %u byte buffer\n", bs);
		return 1;
	}

	if (open_blk(&b, argv[optind + 1], cmd->writes ? O_RDWR : O_RDONLY))
		return 1;

	return cmd->run(&b, argc - optind - 2, argv + optind + 2);
}
