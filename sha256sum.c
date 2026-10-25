// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define TOOL_NAME	"sha256sum"
#define TOOL_USAGE	"<file>"
#include "inc/tool.h"

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

static int prog_sha256sum(int argc, char **argv, char **envp)
{
	unsigned char out[32] = {0};
	struct stat st;
	size_t size;
	char *path;

	path = argv[1];

	fd = open_with_msgs(path, O_RDONLY, 0);
	if (fd < 0)
		return EX_NOINPUT;

	if (fstat(fd, &st)) {
		error("Failed to size %s: %d\n", path, errno);
		return EX_OSERR;
	}

	if (S_ISREG(st.st_mode)) {
		size = st.st_size;
	} else if (S_ISBLK(st.st_mode)) {
		unsigned long long blksize;

		if (ioctl(fd, BLKGETSIZE64, &blksize)) {
			verbose("Failed to size block device %s: %d\n", path, errno);
			return EX_OSERR;
		}

		size = blksize;
	} else {
		error("%s isn't a file or a block device\n", path);
		return EX_DATAERR;
	}

	if (lonesha256_stream(out, read_block, size)) {
		error("Failed to read %s: %d\n", path, errno);
		return EX_IOERR;
	}

	/* Make sure we hashed as much data as we expected */
	if (hashed < (off_t) size)
		return EX_IOERR;

	for (int i = 0; i < sizeof(out); i++)
		printf("%02x", (unsigned) out[i]);
	printf("  %s\n", path);

	return 0;
}

TOOL_SIMPLE_MAIN(1, 1, prog_sha256sum)
