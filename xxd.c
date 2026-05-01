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
	sz = file_size(fd);

	for (i = 0; i < sz; i += 0x10) {
		uint8_t buf[0x10];

		read(fd, buf, 0x10);

		printf("%08x: ", i);
		for (j = 0; j < 0x10; j += 2) {
			printf("%02x%02x ", buf[j], buf[j + 1]);
		}

		for (j = 0; j < 0x10; j++) {
			char ch = buf[j];

			printf("%c", isprint(ch) ? ch : '.');
		}

		printf("\n");
	}

	return 0;
}
