// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_NET_H
#define _SMOLUTILS_NET_H

/* This is some libc stuff that might get moved.. */

#define INET_ADDRSTRLEN		16
#define INET6_ADDRSTRLEN	48

int inet_aton(const char *cp, struct in_addr *inp) {
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

int inet_pton(int af, const char *src, void *dst)
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

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
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

static int smolutils_net_setsockbroadcast(int sock)
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

static int smolutils_net_setsockrxtimeout(int sock, int timeout)
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

#endif /* _SMOLUTILS_NET_H */
