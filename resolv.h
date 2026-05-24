#ifndef _SMOLUTILS_RESOLV_H
#define _SMOLUTILS_RESOLV_H

#include "memfd.h"

#define RESOLV_MAX_RESULTS 8

struct resolv_buf {
	struct in_addr addr_v4[RESOLV_MAX_RESULTS];
	unsigned int num_addr_v4;
};

int resolv_mapbuf(int memfd, struct resolv_buf **buf)
{
	size_t mapsz = sizeof(struct resolv_buf);
	void *mapped;

	mapped = mmap(NULL, mapsz, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);
	if (mapped == MAP_FAILED) {
		error("Failed to mmap() memfd: %d\n", errno);
		return -1;
        }

	*buf = (struct resolv_buf *) mapped;

	return 0;
}

int resolv_doit(const char *hostname, struct resolv_buf *result)
{
	size_t mapsz = sizeof(*result);
	char memfd_str[16] = { 0 };
	char * const newargv[] = {
		"resolv",
		"-m", /* memfd mode */
		memfd_str, /* memfd as a string */
		(char *) hostname,
		NULL
	};
	int __cleanup_fd memfd = -1;
	struct resolv_buf *mapped;
	int ret;

	ret = memfd_create_and_size("resolv_buf", &memfd);
	if (ret) {
		verbose("failed to create memfd to capture resolv result: %d:%d\n", ret, errno);
		return ret;
	}

#if 0 /* Until nolibc ftruncate appears .. */
	ret = ftruncate(memfd, mapsz);
	if (ret) {
		verbose("failed to expand memfd to size of result\n");
		return ret;
	}
#else
	memset(result, 0, mapsz);
	ret = write(memfd, result, mapsz);
	if (ret != mapsz) {
		verbose("failed to write tmp into memfd");
	}
	lseek(memfd, 0, SEEK_SET);
#endif

	sprintf(memfd_str, "%d", memfd);

	ret = spawn_and_wait_args("/bin/resolv", newargv);
	if (ret) {
		printf("failed to spawn resolv\n");
		return -1;
	}

	ret = resolv_mapbuf(memfd, &mapped);
	if (ret)
		return -1;

	memcpy(result, mapped, mapsz);

	munmap(mapped, mapsz);

	verbose("got %d results from resolv\n", result->num_addr_v4);

	return 0;
}

#endif  /* _SMOLUTILS_RESOLV_H */
