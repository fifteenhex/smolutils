// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_DHCPC_H
#define _SMOLUTILS_DHCPC_H

#define DHCPC_LEASE_PATH	SMOL_RUN_PUBLIC_DIR "/dhcpc.lease"
#define DHCPC_DNS_PATH		SMOL_RUN_PUBLIC_DIR "/dhcpc.dns"

#define DHCPC_MAX_DNS 4

struct dhcpc_lease {
	uint32_t address;
	uint32_t subnet_mask;
	uint32_t router;
	uint32_t serverid;
	uint32_t lease_time;
};

struct dhcpc_dns {
	uint32_t server[DHCPC_MAX_DNS];
	uint32_t num;
};

static inline int dhcpc_read_state(const char *path, void *out, size_t len)
{
	int __cleanup_fd fd = -1;
	int ret;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;

	ret = read(fd, out, len);
	if (ret != (int) len)
		return -1;

	return 0;
}

#endif /* _SMOLUTILS_DHCPC_H */
