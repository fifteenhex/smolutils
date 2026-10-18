// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_NET_H
#define _SMOLUTILS_NET_H

#include "asserts.h"
#include "cleanup.h"
#include "printing.h"

#include <linux/sockios.h>

#include "unaligned.h"

/* This is some libc stuff that might get moved.. */

/* The uapi headers don't provide these, so here it is by hand */
struct ifreq {
	#define IFHWADDRLEN 6
	#define	IFNAMSIZ 16
	#define IFF_UP (1 << 0)
	#define IFF_RUNNING (1 << 6)
	#define ifr_name ifr_ifrn.ifrn_name
	#define ifr_hwaddr ifr_ifru.ifru_hwaddr
	#define	ifr_addr ifr_ifru.ifru_addr
	#define	ifr_netmask ifr_ifru.ifru_netmask
	#define	ifr_flags ifr_ifru.ifru_flags
union
	{
		char ifrn_name[IFNAMSIZ];
	} ifr_ifrn;

	union {
		struct sockaddr ifru_addr;
//		struct sockaddr ifru_dstaddr;
//		struct sockaddr ifru_broadaddr;
		struct sockaddr ifru_netmask;
		struct sockaddr ifru_hwaddr;
		short ifru_flags;
//		int ifru_ivalue;
//		int ifru_mtu;
		/* Not used, needed to get the struct size right */
		struct {
			unsigned long mem_start;
			unsigned long mem_end;
			unsigned short base_addr;
			unsigned char irq;
			unsigned char dma;
			unsigned char port;
		} ifru_map;
//		char ifru_slave[IFNAMSIZ];
//		char ifru_newname[IFNAMSIZ];
//		void *ifru_data;
//		struct if_settings ifru_settings;
	} ifr_ifru;
};

static_assert_size(struct ifreq, sizeof(long) == 8 ? 40 : 32);

static inline void ifreq_init(struct ifreq *ifr, const char *name)
{
	memset(ifr, 0, sizeof(*ifr));
	strncpy(ifr->ifr_name, name, IFNAMSIZ - 1);
}

static inline int smolutils_net_inet_socket(void)
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock < 0)
		error("Failed to create socket: %d\n", errno);

	return sock;
}

static inline int ifreq_get(int sock, const char *name, unsigned long cmd, struct ifreq *ifr)
{
	ifreq_init(ifr, name);

	return ioctl(sock, cmd, ifr);
}

static inline int smolutils_net_interface_set_up(const char *iface)
{
	int __cleanup_fd sock = -1;
	struct ifreq ifr;

	sock = smolutils_net_inet_socket();
	if (sock < 0)
		return -1;

	if (ifreq_get(sock, iface, SIOCGIFFLAGS, &ifr) < 0)
		return -1;

	ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

	if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0)
		return -1;

	return 0;
}

#define SOCK_CLOEXEC	O_CLOEXEC

static inline void sockaddr_in_init(struct sockaddr_in *sin, uint32_t addr_be,
				    uint16_t port)
{
	memset(sin, 0, sizeof(*sin));
	sin->sin_family = AF_INET;
	sin->sin_port = htons(port);
	sin->sin_addr.s_addr = addr_be;
}

#define INET_ADDRSTRLEN		16
#define INET6_ADDRSTRLEN	48

static inline int inet_aton(const char *cp, struct in_addr *inp) {
	const char *start = cp;
	uint32_t tmp = 0;
	char *end;
	int i;

	for (i = 0; i < 4; i++) {
		unsigned long val = strtoul(start, &end, 10);

		if (end == start || val > 255)
			return 0;

		tmp = (tmp << 8) | val;

		if (i < 3) {
			const char *dot = strchr(start, '.');

			if (!dot || dot != end)
				return 0;
			start = dot + 1;
		}
	}

	if (*end != '\0')
		return 0;

	if (inp)
		inp->s_addr = htonl(tmp);

	return 1;
}

static inline int inet_pton(int af, const char *src, void *dst)
{
	switch (af) {
	case AF_INET:
		return inet_aton(src, dst);
	default:
		break;
	}
	return 0;
}

#define IPPRINT "%d.%d.%d.%d"
#define IPARGS(_a)			\
	(int) (((_a) >> 24) & 0xff),	\
	(int) (((_a) >> 16) & 0xff),	\
	(int) (((_a) >> 8) & 0xff),	\
	(int) ((_a) & 0xff)

static inline const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
	uint32_t v4_addr;

	switch(af) {
	case AF_INET:
		v4_addr = get_unaligned_be32(src);
		sprintf(dst, IPPRINT, (v4_addr >> 24) & 0xff,
		(v4_addr >> 16) & 0xff,
		(v4_addr >> 8) & 0xff,
		 v4_addr & 0xff);
		return dst;
	default:
		break;
	}

	return NULL;
}

static inline int smolutils_net_setsockbroadcast(int sock)
{
	int sock_opt = 1;
	int ret;

	ret = setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
			 &sock_opt, sizeof(sock_opt));
	if (ret < 0) {
		error("Failed to set broadcast\n");
		return -1;
	}

	return 0;
}

static inline int smolutils_net_setsockrxtimeout(int sock, int timeout)
{
	struct timeval tv = { .tv_sec = timeout };
	int ret;

	ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (ret) {
		verbose("Failed to set socket timeout\n");
		return -1;
	}

	return 0;
}

static inline int smolutils_net_socket_timeout(int type, int protocol,
					       int timeout)
{
	int __cleanup_fd sock = -1;

	sock = socket(AF_INET, type, protocol);
	if (sock < 0) {
		error("Failed to create socket: %d\n", errno);
		return -1;
	}

	if (smolutils_net_setsockrxtimeout(sock, timeout))
		return -1;

	return keep_fd(&sock);
}

static inline int smolutils_net_udp_socket(int timeout)
{
	return smolutils_net_socket_timeout(SOCK_DGRAM, 0, timeout);
}

static inline int smolutils_net_setsockreuseaddr(int sock)
{
	int sock_opt = 1;
	int ret;

	ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			 &sock_opt, sizeof(sock_opt));
	if (ret < 0) {
		error("Failed to set reuseaddr\n");
		return -1;
	}

	return 0;
}

static inline int smolutils_net_listen_tcp(int port, int backlog)
{
	int __cleanup_fd sock = -1;
	struct sockaddr_in addr;

	sockaddr_in_init(&addr, INADDR_ANY, port);

	sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (sock < 0) {
		error("Failed to create socket: %d\n", errno);
		return -1;
	}

	smolutils_net_setsockreuseaddr(sock);

	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
		error("Failed to bind to port %d: %d\n", port, errno);
		return -1;
	}

	if (listen(sock, backlog)) {
		error("Failed to listen: %d\n", errno);
		return -1;
	}

	return keep_fd(&sock);
}

#endif /* _SMOLUTILS_NET_H */
