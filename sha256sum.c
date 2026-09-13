// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

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

	if (!S_ISREG(st.st_mode)) {
		error("%s isn't a regular file\n", path);
		return 1;
	}

	if (lonesha256_stream(out, read_block, st.st_size)) {
		error("Failed to read %s: %d\n", path, errno);
		return 1;
	}

	for (int i = 0; i < sizeof(out); i++)
		printf("%02x", (unsigned) out[i]);
	printf("  %s\n", path);

	return 0;
}
