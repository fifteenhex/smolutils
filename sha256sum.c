// SPDX-License-Identifier: GPL-3.0-or-later

#include "common.h"

#define LONESHA256_STATIC
#include "thirdparty/lonesha256.h"

static int fd;

static int read_block(unsigned char* in)
{
	int len = read(fd, in, LSHA256BLKSIZE);
	if (len < 0)
		return 1;

	return 0;
}

int main (int argc, char **argv, char **envp)
{
	unsigned char out[32] = {0};
	char *path;
	int len;
	off_t filesz;

	if (argc != 2)
		return 1;

	path = argv[1];

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return 1;

	filesz = file_size(fd);
	if (filesz <= 0)
		return 1;

	lonesha256_stream(out, read_block, filesz);

	for (int i = 0; i < sizeof(out); i++)
		printf("%02x", (unsigned) out[i]);
	printf("  %s\n", path);

	return 0;
}
