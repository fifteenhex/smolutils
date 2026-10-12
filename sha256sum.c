// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include <linux/fs.h>

#define LONESHA256_STATIC
#include "thirdparty/lonesha256.h"

static int fd;
static off_t hashed;

static int read_block(unsigned char *in)
{
	int got = read_full(fd, in, LSHA256BLKSIZE);

	if (got < 0)
		return 1;

	hashed += got;

	return 0;
}

int main (int argc, char **argv, char **envp)
{
	unsigned char out[32] = {0};
	struct stat st;
	size_t size;
	char *path;

	if (argc != 2)
		return 1;

	path = argv[1];

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		error("Failed to open %s: %d\n", path, errno);
		return 1;
	}

	if (fstat(fd, &st)) {
		error("Failed to size %s: %d\n", path, errno);
		return 1;
	}

	if (S_ISREG(st.st_mode)) {
		size = st.st_size;
	} else if (S_ISBLK(st.st_mode)) {
		unsigned long long blksize;

		if (ioctl(fd, BLKGETSIZE64, &blksize)) {
			verbose("Failed to size block device %s: %d\n", path, errno);
			return 1;
		}

		size = blksize;
	} else {
		error("%s isn't a file or a block device\n", path);
		return 1;
	}

	if (lonesha256_stream(out, read_block, size)) {
		error("Failed to read %s: %d\n", path, errno);
		return 1;
	}

	for (int i = 0; i < sizeof(out); i++)
		printf("%02x", (unsigned) out[i]);
	printf("  %s\n", path);

	return 0;
}
