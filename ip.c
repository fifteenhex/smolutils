// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "net.h"
#include "sysfs.h"

#include <linux/sockios.h>

#define NET_CLASS "/sys/class/net"

enum show {
	SHOW_ADDR,
	SHOW_LINK,
};

struct context {
	int sock;
	enum show what;
};

static bool interface_ioctl(int sock, const char *name, unsigned long req,
			    struct ifreq *ifr)
{
	memset(ifr, 0, sizeof(*ifr));
	strncpy(ifr->ifr_name, name, IFNAMSIZ - 1);

	return ioctl(sock, req, ifr) == 0;
}

static void print_flags(short flags)
{
	static const struct {
		short flag;
		const char *name;
	} known[] = {
		{ IFF_UP, "UP" },
		{ IFF_RUNNING, "RUNNING" },
	};
	bool first = true;
	unsigned int i;

	printf("<");

	for (i = 0; i < ARRAY_SIZE(known); i++) {
		if (!(flags & known[i].flag))
			continue;

		printf("%s%s", first ? "" : ",", known[i].name);
		first = false;
	}

	if (first)
		printf("DOWN");

	printf(">");
}

static void print_hwaddr(const struct ifreq *ifr)
{
	const uint8_t *mac = (const uint8_t *) ifr->ifr_hwaddr.sa_data;

	printf("    link %02x:%02x:%02x:%02x:%02x:%02x\n",
	       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static unsigned int mask_to_bits(uint32_t mask)
{
	unsigned int bits = 0;

	mask = ntohl(mask);

	while (mask & 0x80000000u) {
		bits++;
		mask <<= 1;
	}

	return bits;
}

static void print_address(struct context *cntx, const char *name)
{
	char addr_str[INET_ADDRSTRLEN];
	struct sockaddr_in *sin;
	struct ifreq ifr;
	uint32_t addr;
	unsigned int bits = 32;

	if (!interface_ioctl(cntx->sock, name, SIOCGIFADDR, &ifr))
		return;

	sin = (struct sockaddr_in *) &ifr.ifr_addr;
	addr = sin->sin_addr.s_addr;

	if (interface_ioctl(cntx->sock, name, SIOCGIFNETMASK, &ifr)) {
		sin = (struct sockaddr_in *) &ifr.ifr_netmask;
		bits = mask_to_bits(sin->sin_addr.s_addr);
	}

	if (!inet_ntop(AF_INET, &addr, addr_str, sizeof(addr_str)))
		return;

	printf("    inet %s/%u\n", addr_str, bits);
}

static int cb(const char *name, int dir, void *priv)
{
	struct context *cntx = priv;
	unsigned long mtu = 0;
	struct ifreq ifr;
	char path[256];

	if (snprintf(path, sizeof(path), "%s/%s", NET_CLASS, name)
	    >= (int) sizeof(path))
		return 0;

	printf("%s: ", name);

	if (interface_ioctl(cntx->sock, name, SIOCGIFFLAGS, &ifr))
		print_flags(ifr.ifr_flags);
	else
		printf("<>");

	if (sysfs_read_number(path, "mtu", &mtu))
		printf(" mtu %lu", mtu);

	printf("\n");

	if (interface_ioctl(cntx->sock, name, SIOCGIFHWADDR, &ifr))
		print_hwaddr(&ifr);

	if (cntx->what == SHOW_ADDR)
		print_address(cntx, name);

	return 0;
}

int main (int argc, char **argv, char **envp)
{
	struct context cntx = { .what = SHOW_ADDR };
	int __cleanup_fd sock = -1;
	int i;

	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];

		if (strcmp(arg, "show") == 0)
			continue;

		if (strcmp(arg, "a") == 0 || strcmp(arg, "addr") == 0 ||
		    strcmp(arg, "address") == 0) {
			cntx.what = SHOW_ADDR;
			continue;
		}

		if (strcmp(arg, "l") == 0 || strcmp(arg, "link") == 0) {
			cntx.what = SHOW_LINK;
			continue;
		}

		usage("usage: ip [addr|link] [show]\n");
		return 1;
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		error("Failed to make a socket: %d\n", errno);
		return 1;
	}

	cntx.sock = sock;

	if (iterate_dir(NET_CLASS, cb, &cntx) < 0) {
		error("Failed to read %s: %d\n", NET_CLASS, errno);
		return 1;
	}

	return 0;
}
