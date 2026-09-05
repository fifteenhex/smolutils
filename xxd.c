// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

/* get an fd from our parent, for memfd based chaining */
static int inherited_fd(const char *arg)
{
	char *endptr;
	long fd;

	fd = strtol(arg, &endptr, 10);
	if (endptr == arg || *endptr != '\0' || fd < 0) {
		error("Not an fd: %s\n", arg);
		return -1;
	}

	return fd;
}

int main (int argc, char **argv, char **envp)
{
	int __cleanup_fd fd = -1;
	unsigned long offset = 0;
	const char *path;
	char *endptr;
	off_t sz;
	int i, j;
	int c;

	while ((c = getopt(argc, argv, "m:o:")) != -1) {
		switch (c) {
		case 'm':
			fd = inherited_fd(optarg);
			if (fd < 0)
				return 1;
			break;

		case 'o':
			offset = strtoul(optarg, &endptr, 0);
			if (endptr == optarg || *endptr != '\0') {
				error("Not an address: %s\n", optarg);
				return 1;
			}
			break;

		default:
			usage("usage: xxd [-m <fd>] [-o <address>] <file>\n");
			return 1;
		}
	}

	if (fd < 0) {
		if (optind != argc - 1) {
			usage("usage: xxd [-m <fd>] [-o <address>] <file>\n");
			return 1;
		}

		path = argv[optind];

		fd = open(path, O_RDONLY);
		if (fd < 0) {
			error("Failed to open: %s\n", path);
			return 1;
		}
	}

	sz = file_size(fd);

	for (i = 0; i < sz; i += 0x10) {
		uint8_t buf[0x10] = { 0 };
		int ret;

		/* Read a row's worth of data */
		ret = read(fd, buf, 0x10);
		if (ret < 0)
			return 1;

		printf("%08lx: ", offset + i);

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
