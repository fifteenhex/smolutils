// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_NET_H
#define _SMOLUTILS_NET_H

#include <linux/sockios.h>

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
//		struct ifmap ifru_map;
//		char ifru_slave[IFNAMSIZ];
//		char ifru_newname[IFNAMSIZ];
//		void *ifru_data;
//		struct if_settings ifru_settings;
	} ifr_ifru;
};

static inline int smolutils_net_interface_set_up(const char *iface)
{
	int __cleanup_fd sock = -1;
	struct ifreq ifr = { 0 };
	int ret;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return -1;

	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

	ret = ioctl(sock, SIOCGIFFLAGS, &ifr);
	if (ret < 0)
		return -1;

	ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

	ret = ioctl(sock, SIOCSIFFLAGS, &ifr);
	if (ret < 0)
		return -1;

	return 0;
}

#define SOCK_CLOEXEC	O_CLOEXEC

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
		memcpy(&v4_addr, src, sizeof(v4_addr));
		v4_addr = ntohl(v4_addr);
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
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
	};
	int sock;

	sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (sock < 0) {
		error("Failed to create socket: %d\n", errno);
		return -1;
	}

	smolutils_net_setsockreuseaddr(sock);

	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
		error("Failed to bind to port %d: %d\n", port, errno);
		close(sock);
		return -1;
	}

	if (listen(sock, backlog)) {
		error("Failed to listen: %d\n", errno);
		close(sock);
		return -1;
	}

	return sock;
}

#endif /* _SMOLUTILS_NET_H */
