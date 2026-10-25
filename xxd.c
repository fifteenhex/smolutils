// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define USAGE \
	"usage: xxd [-m <fd>] [-o <address>] [-s <skip>] [-l <length>] <file>\n"

int main (int argc, char **argv, char **envp)
{
	int __cleanup_fd fd = -1;
	unsigned long offset = 0;
	unsigned long length = 0;
	unsigned long skip = 0;
	const char *path;
	bool bounded;
	off_t sz;
	off_t i;
	int j;
	int c;

	while ((c = getopt(argc, argv, "m:o:s:l:")) != -1) {
		switch (c) {
		case 'm':
			if (!parse_fd_with_msgs(optarg, &fd))
				return 1;
			break;

		case 'o':
			if (!parse_ul(optarg, 0, &offset)) {
				error("Not an address: %s\n", optarg);
				return 1;
			}
			break;

		case 's':
			if (!parse_ul(optarg, 0, &skip)) {
				error("Not an offset: %s\n", optarg);
				return 1;
			}
			break;

		case 'l':
			if (!parse_ul(optarg, 0, &length)) {
				error("Not a length: %s\n", optarg);
				return 1;
			}
			break;

		default:
			usage(USAGE);
			return EX_USAGE;
		}
	}

	if (fd < 0) {
		if (optind != argc - 1) {
			usage(USAGE);
			return EX_USAGE;
		}

		path = argv[optind];

		fd = open_with_msgs(path, O_RDONLY, 0);
		if (fd < 0)
			return 1;
	}

	if (skip && lseek(fd, (off_t) skip, SEEK_SET) < 0) {
		error("Failed to seek to %lu: %d\n", skip, errno);
		return 1;
	}

	sz = file_size(fd);
	if (sz < 0) {
		error("Failed to size the input: %d\n", errno);
		return 1;
	}

	/* Devices, pipes and /proc files have no size: read to -l or the end */
	bounded = sz || length;
	if (!sz)
		sz = (off_t) length;
	else if (length && (off_t) length < sz)
		sz = (off_t) length;

	for (i = 0; !bounded || i < sz; i += 0x10) {
		uint8_t buf[0x10] = { 0 };
		int want = 0x10;
		int ret;

		/* clamp want so it doesn't read past the size or requested length */
		if (bounded && sz - i < want)
			want = (int) (sz - i);

		/* Read a row's worth of data */
		ret = read_full(fd, buf, want);
		if (ret < 0) {
			error("Failed to read: %d\n", errno);
			return 1;
		}

		/* Ran out early: a short device, or -l past the end */
		if (!ret)
			break;

		printf("%08lx: ", (unsigned long) (offset + skip + i));

		for (j = 0; j < 0x10; j += 2) {
			uint8_t lsb = buf[j];
			uint8_t msb = buf[j + 1];
			int remaining = ret - j;

			if (remaining <= 0)
				printf("     ");
			else if (remaining == 1)
				printf("%02x   ", lsb);
			else
				printf("%02x%02x ", lsb, msb);
		}

		for (j = 0; j < 0x10; j++) {
			char ch = buf[j];
			int remaining = ret - j;

			if (remaining > 0)
				printf("%c", isprint(ch) ? ch : '.');
			else
				printf(" ");
		}

		printf("\n");
	}

	return 0;
}
