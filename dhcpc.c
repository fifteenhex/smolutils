// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#define VERBOSE
#include "common.h"

#include <linux/sockios.h>

#define DEFAULT_INTERFACE "eth0"
#define SERVER_PORT 67
#define CLIENT_PORT 68

#define MAGIC_COOKIE    0x63825363U

#define DHCPDISCOVER    1
#define DHCPOFFER       2
#define DHCPREQUEST     3
#define DHCPACK         5

#define OPT_PAD         0
#define OPT_SUBNET      1
#define OPT_ROUTER      3
#define OPT_DNS         6
#define OPT_REQ_IP      50
#define OPT_LEASE       51
#define OPT_MSG_TYPE    53
#define OPT_SERVER_ID   54
#define OPT_PARAM_REQ   55
#define OPT_END         255

struct context {
	const char* interface;
	uint32_t xid;
	uint8_t mac[6];
	int sock;
};

struct dhcp_packet {
	uint8_t  op;
	uint8_t  htype;
	uint8_t  hlen;
	uint8_t  hops;
	uint32_t xid;
	uint16_t secs;
	uint16_t flags;
	uint32_t ciaddr;
	uint32_t yiaddr;
	uint32_t siaddr;
	uint32_t giaddr;
	uint8_t  chaddr[16];
	uint8_t  sname[64];
	uint8_t  file[128];
	uint32_t magic;
	uint8_t  options[308];
} __attribute__((packed));

static int build_discover(struct dhcp_packet *p,
			  uint32_t xid,
			  const uint8_t *mac)
{
	int opt_count = 0;

	memset(p, 0, sizeof(*p));

	p->op = 1,
	p->htype = 1,
	p->hlen = 6,
	p->xid = htonl(xid);
	p->flags = htons(0x8000);
	memcpy(p->chaddr, mac, 6);
	p->magic = htonl(MAGIC_COOKIE);

	p->options[opt_count++] = OPT_MSG_TYPE;
	p->options[opt_count++] = 1;
	p->options[opt_count++] = DHCPDISCOVER;

	p->options[opt_count++] = OPT_PARAM_REQ;
	p->options[opt_count++] = 3;
	p->options[opt_count++] = OPT_SUBNET;
	p->options[opt_count++] = OPT_ROUTER;
	p->options[opt_count++] = OPT_DNS;
	p->options[opt_count++] = OPT_END;

	return offsetof(struct dhcp_packet, options) + opt_count;
}

static int send_discover(struct context *cntx)
{
	struct sockaddr_in dst = {
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT),
		.sin_addr.s_addr = INADDR_BROADCAST,
	};
	struct dhcp_packet p;
	int len;
	int ret;

	len = build_discover(&p, cntx->xid, cntx->mac);

	ret = sendto(cntx->sock, &p, len, 0, (struct sockaddr *)&dst, sizeof(dst));

	if (ret) {
		error("Failed to send discover: %d\n", errno);
		return -1;
	}

	return 0;
}

static int setup_socket(struct context *cntx)
{
	const struct sockaddr_in client_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(CLIENT_PORT),
		.sin_addr.s_addr = INADDR_ANY,
	};
	const struct sockaddr_in bcast = {
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT),
		.sin_addr.s_addr = INADDR_BROADCAST,
	};
	struct timeval timeout = { .tv_sec = 10 };
	int sock_opt = 1;
	int sock;
	int ret;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		error("Failed to create socket\n");
		return 1;
	}

	ret = setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
			 &sock_opt, sizeof(sock_opt));
	if (ret < 0) {
		error("Failed to set broadcast\n");
		return 1;
	}

	ret = setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
			 (void *) cntx->interface, strlen(cntx->interface) + 1);
	if (ret < 0) {
		error("Failed to bind\n");
		return 1;
	}

	ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
		   &timeout, sizeof(timeout));
	if (ret < 0) {
		error("Failed to set timeout\n");
		return 1;
	}

	ret = bind(sock, (struct sockaddr *)&client_addr, sizeof client_addr);
	if (ret < 0) {
		error("Failed to bind socket\n");
		return 1;
	}

	cntx->sock = sock;

	return 0;
}


struct ifreq {
	#define IFHWADDRLEN 6
	#define	IFNAMSIZ 16
	#define ifr_name ifr_ifrn.ifrn_name
	#define ifr_hwaddr ifr_ifru.ifru_hwaddr
	union
	{
		char ifrn_name[IFNAMSIZ];
	} ifr_ifrn;

	union {
//		struct sockaddr ifru_addr;
//		struct sockaddr ifru_dstaddr;
//		struct sockaddr ifru_broadaddr;
//		struct sockaddr ifru_netmask;
		struct sockaddr ifru_hwaddr;
//		short ifru_flags;
//		int ifru_ivalue;
//		int ifru_mtu;
//		struct ifmap ifru_map;
//		char ifru_slave[IFNAMSIZ];
//		char ifru_newname[IFNAMSIZ];
//		void *ifru_data;
//		struct if_settings ifru_settings;
	} ifr_ifru;
};

static int get_mac(const char *iface, uint8_t *mac)
{
	int __cleanup_fd sock = -1;
	struct ifreq ifr = { 0 };
	int ret;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	strncpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name) - 1);

	ret = ioctl(sock, SIOCGIFHWADDR, &ifr);
	if (ret) {
		error("failed to get mac address");
		return -1;
	}

	memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);

	return 0;
}

int main(int argc, char **argv, char **envp)
{
	struct context cntx = {
		.interface = DEFAULT_INTERFACE,
	};
	int ret, sock;

	ret = get_mac(cntx.interface, cntx.mac);
	if (ret)
		return 1;

	ret = setup_socket(&cntx);
	if (ret)
		return 1;

	ret = send_discover(&cntx);

	return 0;
}
