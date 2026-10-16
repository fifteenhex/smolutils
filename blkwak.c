// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "blkwak"

#include "common.h"

#include <linux/fs.h>

#define DEFAULT_BS	(64 * 1024)
#define MAX_BS		(1024 * 1024)
#define SECTOR		512

#define USAGE \
	"usage: blkwak [-b <bs>] [-v] <command> <dev> ...\n" \
	"       info <dev>\n" \
	"       read <dev> <file> [off] [len]      device -> file\n" \
	"       write <dev> <file> [off]           file -> device\n" \
	"       cmp <dev> <file> [off]             compare with a file\n" \
	"       rescan <dev>                       re-read the partition table\n" \
	"       -b read/write buffer size\n" \
	"       -v verify write\n"

struct blk {
	struct stat st;
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
	b->fd = open_with_msgs(path, flags, 0);
	if (b->fd < 0)
		return -1;

	if (fstat(b->fd, &b->st)) {
		error("Failed to stat %s: %d\n", path, errno);
		return -1;
	}

	if (S_ISBLK(b->st.st_mode)) {
		if (ioctl(b->fd, BLKGETSIZE64, &b->size)) {
			error("Failed to size %s: %d\n", path, errno);
			return -1;
		}
	} else if (S_ISREG(b->st.st_mode)) {
		b->size = b->st.st_size;
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

	if (!off_len_fits(*off, *len, b->size)) {
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

static int write_at(const struct blk *b, unsigned long long off,
		    const uint8_t *data, unsigned int n)
{
	if (lseek(b->fd, off, SEEK_SET) != (off_t) off) {
		error("Failed to seek to 0x%llx: %d\n", off, errno);
		return -1;
	}

	if (write_full(b->fd, data, n)) {
		error("Write at 0x%llx failed: %d\n", off, errno);
		return -1;
	}

	return 0;
}

struct img {
	struct stat st;
	int fd;
};

static void cleanup_img(struct img *i)
{
	if (i->fd >= 0)
		close(i->fd);
}

#define __cleanup_img __attribute__((cleanup(cleanup_img)))

static int open_img(struct img *i, const char *path, int flags, mode_t mode)
{
	i->fd = open_with_msgs(path, flags, mode);
	if (i->fd < 0)
		return -1;

	if (fstat(i->fd, &i->st)) {
		error("Failed to stat %s: %d\n", path, errno);
		return -1;
	}

	return 0;
}

static bool is_nonsense(struct blk *b, struct img *img)
{
	/* Don't bother playing games with the block and image being the same thing */
	if (__same_file(&b->st, &img->st)) {
		error("Don't pass the same thing for block device and image\n");
		return true;
	}

	return false;
}

static int cmd_read(struct blk *b, int argc, char **argv)
{
	struct img __cleanup_img out = { .fd = -1 };
	unsigned long long done = 0;
	unsigned long long off;
	unsigned long long len;

	if (argc < 1 || argc > 3 ||
	    !parse_region(b, argc - 1, argv + 1, &off, &len)) {
		usage(USAGE);
		return 1;
	}

	if (open_img(&out, argv[0], O_WRONLY | O_CREAT, 0644))
		return 1;

	if (is_nonsense(b, &out))
		return 1;

	/* If the output is a normal file truncate it first, maybe we need a notrunc mode later? */
	if (__is_regfile(&out.st) && ftruncate(out.fd, 0)) {
		error("Failed to truncate %s: %d\n", argv[0], errno);
		return 1;
	}

	while (done < len) {
		unsigned int n = len - done > bs ? bs : len - done;

		if (read_at(b, off + done, n))
			return 1;

		if (write_full(out.fd, buf, n)) {
			error("Failed to write %s: %d\n", argv[0], errno);
			return 1;
		}

		done += n;
	}

	info("read %llu bytes\n", done);

	return 0;
}

static bool verify;

static int compare_image(const struct blk *b, const struct img *in,
			 unsigned long long off)
{
	unsigned long long first_bad = ~0ULL;
	unsigned long long done = 0;
	unsigned long long bad = 0;
	uint8_t *data;
	int ret = 1;
	int got;

	data = malloc(bs);
	if (!data) {
		error("No room for a second buffer\n");
		return 1;
	}

	while ((got = read_full(in->fd, data, bs)) > 0) {
		unsigned int i;

		if (!off_len_fits(off + done, got, b->size)) {
			error("0x%llx + %u is past the end of the device\n",
			      off + done, got);
			goto out;
		}

		if (read_at(b, off + done, got))
			goto out;

		for (i = 0; i < (unsigned int) got; i++)
			if (buf[i] != data[i]) {
				if (first_bad == ~0ULL)
					first_bad = off + done + i;
				bad++;
			}

		done += got;
	}

	if (got < 0) {
		error("Failed to read the image: %d\n", errno);
		goto out;
	}

	if (bad)
		info("%llu of %llu bytes differ, first at 0x%llx\n", bad, done,
		     first_bad);
	else
		info("%llu bytes match\n", done);

	ret = bad ? 1 : 0;
out:
	free(data);

	return ret;
}

static int cmd_write(struct blk *b, int argc, char **argv)
{
	struct img __cleanup_img in = { .fd = -1 };
	unsigned long long done = 0;
	unsigned long long off = 0;
	int got;

	if (argc < 1 || argc > 2 || (argc == 2 && !parse_size(argv[1], &off))) {
		usage(USAGE);
		return 1;
	}

	if (open_img(&in, argv[0], O_RDONLY, 0))
		return 1;

	if (is_nonsense(b, &in))
		return 1;

	while ((got = read_full(in.fd, buf, bs)) > 0) {
		if (!off_len_fits(off + done, got, b->size)) {
			error("0x%llx + %u is past the end of the device\n",
			      off + done, got);
			return 1;
		}

		if (write_at(b, off + done, buf, got))
			return 1;
		done += got;
	}

	if (got < 0) {
		error("Failed to read %s: %d\n", argv[0], errno);
		return 1;
	}

	if (fsync(b->fd)) {
		error("fsync failed: %d\n", errno);
		return 1;
	}

	info("wrote %llu bytes\n", done);

	if (!verify)
		return 0;

	/* drop the caches */
	if (ioctl(b->fd, BLKFLSBUF, 0) && errno != ENOTTY) {
		return 1;
	}

	if (lseek(in.fd, 0, SEEK_SET)) {
		error("Failed to rewind %s: %d\n", argv[0], errno);
		return 1;
	}

	return compare_image(b, &in, off);
}

static int cmd_cmp(struct blk *b, int argc, char **argv)
{
	struct img __cleanup_img in = { .fd = -1 };
	unsigned long long off = 0;

	if (argc < 1 || argc > 2 || (argc == 2 && !parse_size(argv[1], &off))) {
		usage(USAGE);
		return 1;
	}

	if (open_img(&in, argv[0], O_RDONLY, 0))
		return 1;

	if (is_nonsense(b, &in))
		return 1;

	return compare_image(b, &in, off);
}

static int cmd_rescan(struct blk *b, int argc, char **argv)
{
	if (argc) {
		usage(USAGE);
		return 1;
	}

	if (ioctl(b->fd, BLKRRPART, 0)) {
		if (errno == EBUSY)
			error("Device is busy\n");
		else
			error("Rescan failed: %d\n", errno);
		return 1;
	}

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
	{ "write", cmd_write, true },
	{ "cmp", cmd_cmp, false },
	{ "rescan", cmd_rescan, false },
};

int main (int argc, char **argv, char **envp)
{
	struct blk __cleanup_blk b = { .fd = -1 };
	const struct command *cmd = NULL;
	unsigned long long v;
	int c;

	while ((c = getopt(argc, argv, "b:v")) != -1) {
		switch (c) {
		case 'b':
			if (!parse_size(optarg, &v) || !v || v > MAX_BS) {
				error("Not a buffer size: %s\n", optarg);
				return 1;
			}
			bs = v;
			break;

		case 'v':
			verify = true;
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
