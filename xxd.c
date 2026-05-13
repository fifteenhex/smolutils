// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

int main (int argc, char **argv, char **envp)
{
	int __cleanup_fd fd = -1;
	const char *path;
	off_t sz;
	int i, j;

	if (argc != 2)
		return 1;

	path = argv[1];

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		error("Failed to open: %s\n", path);
		return 1;
	}

	sz = file_size(fd);

	for (i = 0; i < sz; i += 0x10) {
		uint8_t buf[0x10];
		int ret;

		/* Read a row's worth of data */
		ret = read(fd, buf, 0x10);
		if (ret < 0)
			return 1;

		printf("%08x: ", i);

		for (j = 0; j < 0x10; j += 2) {
			uint8_t lsb = buf[j];
			uint8_t msb = buf[j + 1];
			int remaining = ret - j;

			if (remaining <= 0)
				printf("     ");
			else if (remaining == 0)
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
