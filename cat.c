// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

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

int main (int argc, char **argv, char **envp)
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
