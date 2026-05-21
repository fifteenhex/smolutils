// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#define VERBOSE
#include "common.h"
#include "net.h"

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

struct config {
	uint32_t address;
	uint32_t subnet_mask;
	uint32_t router;
	uint32_t serverid;
};

struct context {
	const char* interface;
	uint32_t xid;
	uint8_t mac[6];
	int sock;

	struct config config;
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

static void clear_packet_create_header(struct dhcp_packet *p,
				       uint32_t xid, const uint8_t *mac)
{
	memset(p, 0, sizeof(*p));

	p->op = 1,
	p->htype = 1,
	p->hlen = 6,
	p->xid = htonl(xid);
	p->flags = htons(0x8000);
	memcpy(p->chaddr, mac, 6);
	p->magic = htonl(MAGIC_COOKIE);
}

static int build_discover(struct dhcp_packet *p,
			  uint32_t xid,
			  const uint8_t *mac)
{
	int opt_count = 0;

	clear_packet_create_header(p, xid, mac);

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

static int send_discover(struct context *cntx, struct dhcp_packet *p)
{
	struct sockaddr_in dst = {
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT),
		.sin_addr.s_addr = INADDR_BROADCAST,
	};
	int len;
	int ret;

	len = build_discover(p, cntx->xid, cntx->mac);

	ret = sendto(cntx->sock, p, len, 0, (struct sockaddr *)&dst, sizeof(dst));

	if (ret != len) {
		error("Failed to send discover: %d\n", errno);
		return -1;
	}

	return 0;
}

static int build_request(struct dhcp_packet *p,
			 uint32_t xid,
			 const uint8_t *mac,
			 uint32_t req_ip,
			 uint32_t server_id)
{
	int opt_count = 0;

	clear_packet_create_header(p, xid, mac);

	p->options[opt_count++] = OPT_MSG_TYPE;
	p->options[opt_count++] = 1;
	p->options[opt_count++] = DHCPREQUEST;

	p->options[opt_count++] = OPT_REQ_IP;
	p->options[opt_count++] = 4;
	memcpy(&p->options[opt_count], &req_ip, 4);
	opt_count += 4;

	p->options[opt_count++] = OPT_SERVER_ID;
	p->options[opt_count++] = 4;
	memcpy(&p->options[opt_count], &server_id, 4);
	opt_count += 4;

	p->options[opt_count++] = OPT_END;

	return offsetof(struct dhcp_packet, options) + opt_count;
}

static int send_request(struct context *cntx, struct dhcp_packet *p)
{
	struct sockaddr_in dst = {
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT),
		.sin_addr.s_addr = INADDR_BROADCAST,
	};
	int len;
	int ret;

	len = build_request(p, cntx->xid, cntx->mac, 0, 0);

	ret = sendto(cntx->sock, p, len, 0, (struct sockaddr *)&dst, sizeof(dst));

	if (ret != len) {
		error("Failed to send discover: %d\n", errno);
		return -1;
	}

	return 0;
}

static int wait_for_packet(struct context *cntx, struct dhcp_packet *p)
{
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT),
		.sin_addr.s_addr = INADDR_BROADCAST,
	};
	socklen_t addr_len = sizeof(addr);
	ssize_t recvd;

	recvd  = recvfrom(cntx->sock, p, sizeof(*p), 0, (struct sockaddr *)&addr, &addr_len);
	if (recvd < 0) {
		error("Timed out waiting for packet\n");
		return 1;
	}

	debug("got packet\n");

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
	int sock;
	int ret;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		error("Failed to create socket\n");
		return -1;
	}

	ret = smolutils_net_setsockbroadcast(sock);
	if (ret)
		return -1;

	ret = setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
			 (void *) cntx->interface, strlen(cntx->interface) + 1);
	if (ret < 0) {
		error("Failed to bind\n");
		return -1;
	}

	ret = smolutils_net_setsockrxtimeout(sock, 3);
	if (ret)
		return -1;

	ret = bind(sock, (struct sockaddr *)&client_addr, sizeof client_addr);
	if (ret < 0) {
		error("Failed to bind socket\n");
		return -1;
	}

	cntx->sock = sock;

	return 0;
}


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

static int interface_set_up(const char *iface)
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

static void print_address(uint32_t addr)
{
	verbose(IPPRINT,
		(addr >> 24) & 0xff,
		(addr >> 16) & 0xff,
		(addr >> 8) & 0xff,
		 addr & 0xff);
}

static int interface_set_address(const char *iface, uint32_t addr, uint32_t mask)
{
	int __cleanup_fd sock = -1;
	struct ifreq ifr = { 0 };
	struct sockaddr_in *sin;
	int ret;

	verbose("Configuring %s\n", iface);
	verbose("Address: ");
	print_address(addr);
	verbose(" subnet mask: ");
	print_address(mask);
	verbose("\n");

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return -1;

	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = htonl(addr);

	ret = ioctl(sock, SIOCSIFADDR, &ifr);
	if (ret < 0) {
		verbose("Failed to set address\n");
		return -1;
	}

	/*
	 * This is really the same thing as above because of the union
	 * but go through the motions.
	 */
	sin = (struct sockaddr_in *)&ifr.ifr_netmask;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = htonl(mask);

        ret = ioctl(sock, SIOCSIFNETMASK, &ifr);
	if (ret < 0) {
		verbose("Failed to set subnet mask\n");
		return -1;
	}

	return 0;
}

struct rtentry {
	unsigned long	rt_pad1;
	struct sockaddr	rt_dst;
	struct sockaddr	rt_gateway;
	struct sockaddr	rt_genmask;
	unsigned short	rt_flags;
	short		rt_pad2;
	unsigned long	rt_pad3;
	void		*rt_pad4;
	short		rt_metric;
	char		*rt_dev;
	unsigned long	rt_mtu;
	unsigned long	rt_window;
	unsigned short	rt_irtt;
};

#define	RTF_UP		0x0001
#define	RTF_GATEWAY	0x0002

static int interface_set_default_route(const char *iface, uint32_t gateway)
{
	int __cleanup_fd sock = -1;
	struct rtentry rt = { 0 };
	struct sockaddr_in *sin;
	int ret;

	verbose("Default route via ");
	print_address(gateway);
	verbose("\n");

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return -1;

	sin = (struct sockaddr_in *)&rt.rt_dst;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = INADDR_ANY;

	sin = (struct sockaddr_in *)&rt.rt_genmask;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = INADDR_ANY;

	sin = (struct sockaddr_in *)&rt.rt_gateway;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = htonl(gateway);

	rt.rt_flags = RTF_UP | RTF_GATEWAY;
	rt.rt_dev = (char *)iface;

	ret = ioctl(sock, SIOCADDRT, &rt);
	if (ret < 0) {
	verbose("Failed to set default route\n");
		return -1;
	}

	return 0;
}

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

static bool check_packet(struct context *cntx, struct dhcp_packet *p, unsigned int op)
{
	if (p->xid != htonl(cntx->xid)) {
		verbose("XID doesn't match\n");
		return false;
	}

	if (p->op != op) {
		verbose("op is wrong\n");
		return false;
	}


	return true;
}

static int find_opt(struct dhcp_packet *p, uint8_t code, uint8_t **opt, unsigned int *len)
{
	uint8_t *_opt = p->options;
	uint8_t *opt_end = p->options + sizeof(p->options);

	while (_opt < opt_end) {
		uint8_t _code = *_opt++;
		uint8_t _len;

		switch(_code) {
		case OPT_PAD:
			break;
		case OPT_END:
			goto out;
		default:
			_len = *_opt++;

			if (_code == code) {
				*opt = _opt;
				*len = _len;
				return 0;
			}
			else
				_opt += _len;
			break;
		}
    }

out:
    return -ENOENT;
}

static int find_opt_u32(struct dhcp_packet *p, uint8_t code, uint32_t *opt)
{
	unsigned int len;
	uint8_t *_opt;
	uint32_t tmp;
	int ret;

	ret = find_opt(p, code, &_opt, &len);
	if (ret)
		return ret;

	if (len != 4)
		return -EINVAL;

	memcpy(&tmp, _opt, sizeof(tmp));
	*opt = ntohl(tmp);

	return 0;
}

static int find_opt_u8(struct dhcp_packet *p, uint8_t code, uint8_t *opt)
{
	unsigned int len;
	uint8_t *_opt;
	int ret;

	ret = find_opt(p, code, &_opt, &len);
	if (ret)
		return ret;

	if (len != 1)
		return -EINVAL;

	*opt = _opt[0];

	return 0;
}

int do_discover(struct context *cntx, struct dhcp_packet *p)
{
	uint32_t addr, subnet, router, serverid;
	uint8_t msgtype;
	int ret;

	verbose("Sending discover\n");
	ret = send_discover(cntx, p);
	if (ret) {
		verbose("Failed to send discover\n");
		return ret;
	}

	verbose("Waiting for offer\n");
	ret = wait_for_packet(cntx, p);
	if (ret) {
		verbose("Didn't get response\n");
		return ret;
	}

	if (!check_packet(cntx, p, 2)) {
		verbose("incorrect packet?\n");
		return -EINVAL;
	}

	ret = find_opt_u8(p, OPT_MSG_TYPE, &msgtype);
	if (ret) {
		verbose("Failed to get msgtype\n");
		return ret;
	}

	if (msgtype != DHCPOFFER)
		return -EINVAL;

	ret = find_opt_u32(p, OPT_SUBNET, &subnet);
	if (ret) {
		verbose("Failed to find subnet\n");
		return ret;
	}

	ret = find_opt_u32(p, OPT_SERVER_ID, &serverid);
	ret = find_opt_u32(p, OPT_ROUTER, &router);

	addr = ntohl(p->yiaddr);

	verbose("Got offer for " IPPRINT "(" IPPRINT "), router " IPPRINT "\n",
		(addr >> 24) & 0xff,
		(addr >> 16) & 0xff,
		(addr >> 8) & 0xff,
		 addr & 0xff,
		(subnet >> 24) & 0xff,
		(subnet >> 16) & 0xff,
		(subnet >> 8) & 0xff,
		 subnet & 0xff,
		(router >> 24) & 0xff,
		(router >> 16) & 0xff,
		(router >> 8) & 0xff,
		 router & 0xff
	);

	cntx->config.serverid = serverid;
	cntx->config.address = addr;
	cntx->config.subnet_mask = subnet;
	cntx->config.router = router;

	return 0;
}

int do_request(struct context *cntx, struct dhcp_packet *p)
{
	uint8_t msgtype;
	int ret;

	verbose("Sending request\n");
	ret = send_request(cntx, p);
	if (ret) {
		verbose("Failed to send request\n");
		return ret;
	}

	verbose("Waiting for ack\n");
	ret = wait_for_packet(cntx, p);
	if (ret) {
		verbose("Didn't get a response to our ack\n");
		return ret;
	}

	if (!check_packet(cntx, p, 2)) {
		verbose("incorrect packet?\n");
		return -EINVAL;
	}

	ret = find_opt_u8(p, OPT_MSG_TYPE, &msgtype);
	if (ret) {
		verbose("Failed to get msgtype\n");
		return ret;
	}

	if (msgtype != DHCPACK) {
		return -EINVAL;
	}

	return 0;
}

int main(int argc, char **argv, char **envp)
{
	struct context cntx = {
		.interface = DEFAULT_INTERFACE,
	};
	bool have_address = false;
	struct dhcp_packet p;
	int ret, sock, tries;

	verbose("Bringing %s up\n", cntx.interface);
	ret = interface_set_up(cntx.interface);
	if (ret)
		return 1;

	verbose("Getting MAC address for %s\n", cntx.interface);
	ret = get_mac(cntx.interface, cntx.mac);
	if (ret)
		return 1;

	verbose("Creating broadcast socket\n");
	ret = setup_socket(&cntx);
	if (ret)
		return 1;

	for (tries = 0; tries < 10; tries++) {
			/*
			 * Note: the socket timeouts control how long
			 * this loop sleeps between tries.
			 */

			ret = do_discover(&cntx, &p);
			if (ret)
				continue;

			ret = do_request(&cntx, &p);
			if (ret)
				continue;

			have_address = true;
			break;
	}

	if (!have_address) {
		error("Giving up..\n");
		return 1;
	}

	interface_set_address(cntx.interface, cntx.config.address, cntx.config.subnet_mask);
	interface_set_default_route(cntx.interface, cntx.config.router);

	return 0;
}
