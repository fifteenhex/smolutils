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

#endif /* _SMOLUTILS_DHCPC_H */
