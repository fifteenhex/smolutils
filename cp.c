// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

static int copy_a_file(const char *src, const char *dst)
{
	int __cleanup_fd src_fd = -1;
	int __cleanup_fd dst_fd = -1;
	off_t sz;
	int ret;

	debug("copying %s to %s\n", src, dst);

	src_fd = open(src, O_RDONLY);
	if (src_fd < 0) {
		debug("failed to open src file\n");
		return -1;
	}

	sz = file_size(src_fd);
	if (sz < 0) {
		debug("failed to get source size\n");
		return -1;
	}

	dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (dst_fd < 0) {
		debug("failed to open/create dst file\n");
		return -1;
	}

	debug("Calling sendfile() to copy %lld bytes\n", sz);

	ret = sendfile(dst_fd, src_fd, NULL, sz);

	return 0;
}

int main (int argc, char **argv, char **envp)
{
	/* super dumb for now */
	if (argc != 3)
		return 1;

	copy_a_file(argv[1], argv[2]);

	return 0;
}
