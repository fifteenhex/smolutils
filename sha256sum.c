// SPDX-License-Identifier: GPL-3.0-or-later

#define LONESHA256_STATIC
#include "thirdparty/lonesha256.h"

int main (int argc, char **argv, char **envp)
{
	unsigned char data[4096];
	unsigned char out[32] = {0};
	char *path;
	int fd;
	int len;

	if (argc != 2)
		return 1;

	path = argv[1];

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return 1;

	len = read(fd, data, sizeof(data));
	if (len < 0)
		return 1;

	lonesha256(out, data, len);

	for (int i = 0; i < sizeof(out); i++)
		printf("%02x", (unsigned) out[i]);
	printf("  %s\n", path);

	return 0;
}
