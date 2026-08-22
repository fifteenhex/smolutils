// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

int main (int argc, char **argv, char **envp)
{
	char buf[4096];
	char *path;
	int len;
	int fd;

	if (argc != 2)
		return 1;

	path = argv[1];

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return 1;

	while (true) {
		len = read(fd, buf, sizeof(buf));
		if (len <= 0)
			break;
		/* FIXME: For now write byte by byte because of nolibc's fwrite() */
		fwrite(buf, 1, len, stdout);
	}

	close(fd);

	return 0;
}
