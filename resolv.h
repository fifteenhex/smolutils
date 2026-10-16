// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_RESOLV_H
#define _SMOLUTILS_RESOLV_H

#include "inc/memfd.h"
#include "inc/mmap.h"

#define RESOLV_MAX_RESULTS 8

struct resolv_buf {
	struct in_addr addr_v4[RESOLV_MAX_RESULTS];
	unsigned int num_addr_v4;
};

static inline bool resolv_mapbuf(struct mapping *m, int memfd)
{
	if (!mapping_map(m, memfd, 0, sizeof(struct resolv_buf),
			 PROT_READ | PROT_WRITE, MAP_SHARED)) {
		error("Failed to mmap() memfd: %d\n", errno);
		return false;
	}

	return true;
}

static inline int resolv_doit(const char *hostname, struct resolv_buf *result)
{
	size_t mapsz = sizeof(*result);
	char memfd_str[FD_ARG_LEN];
	char * const newargv[] = {
		"resolv",
		"-m", /* memfd mode */
		memfd_str, /* memfd as a string */
		(char *) hostname,
		NULL
	};
	struct mapping __cleanup_mapping map = MAPPING_NONE;
	int __cleanup_fd memfd = -1;
	int ret;

	/* An empty result for resolv to fill in */
	memset(result, 0, mapsz);
	memfd = memfd_from_buffer("resolv_buf", result, mapsz);
	if (memfd < 0) {
		verbose("failed to create memfd to capture resolv result: %d\n",
			errno);
		return -1;
	}

	fd_to_arg(memfd_str, memfd);

	ret = spawn_and_wait_args("/bin/resolv", newargv);
	if (ret) {
		error("Failed to spawn resolv\n");
		return -1;
	}

	if (!resolv_mapbuf(&map, memfd))
		return -1;

	memcpy(result, map.at, mapsz);

	verbose("got %d results from resolv\n", result->num_addr_v4);

	return 0;
}

static inline int resolv_host(const char *host, struct in_addr *result)
{
	struct resolv_buf addresses;
	int ret;

	/* Just see if we can convert a v4 dot thingy address first */
	if (inet_aton(host, result))
		return 0;

	/* Do an actual lookup using resolv */
	ret = resolv_doit(host, &addresses);
	if (ret)
		return ret;

	if (!addresses.num_addr_v4)
		return -1;

	memcpy(result, &addresses.addr_v4[0], sizeof(*result));

	return 0;
}

#endif  /* _SMOLUTILS_RESOLV_H */
