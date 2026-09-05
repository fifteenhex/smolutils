// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "dhcpc"

#include "common.h"
#include "net.h"

#include "dhcpc.h"
#include "later.h"

#include <linux/sockios.h>

#define DHCPC_PATH	"/sbin/dhcpc"

#define RENEW_FRACTION	2

#define RETRY_SECONDS	60

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
	uint32_t lease_time;
	uint32_t dns[DHCPC_MAX_DNS];
	uint8_t num_dns;
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

	len = build_request(p, cntx->xid, cntx->mac,
			    htonl(cntx->config.address),
			    htonl(cntx->config.serverid));

	ret = sendto(cntx->sock, p, len, 0, (struct sockaddr *)&dst, sizeof(dst));

	if (ret != len) {
		error("Failed to send request: %d\n", errno);
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

	verbose("Waiting for packet ...\n");

	memset(p, 0, sizeof(*p));

	recvd  = recvfrom(cntx->sock, p, sizeof(*p), 0, (struct sockaddr *)&addr, &addr_len);
	if (recvd < 0) {
		error("Timed out waiting for packet\n");
		return 1;
	}

	if (recvd < (ssize_t) offsetof(struct dhcp_packet, options)) {
		return 1;
	}

	if (ntohl(p->magic) != MAGIC_COOKIE) {
		return 1;
	}

	verbose("Got packet\n");

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


static const char *addr_to_str(uint32_t addr, char *buf, socklen_t len)
{
	uint32_t tmp = htonl(addr);

	return inet_ntop(AF_INET, &tmp, buf, len);
}

static int interface_set_address(const char *iface, uint32_t addr, uint32_t mask)
{
	int __cleanup_fd sock = -1;
	struct ifreq ifr = { 0 };
	struct sockaddr_in *sin;
	int ret;

	verbose("Configuring %s, address " IPPRINT " subnet mask " IPPRINT "\n",
		iface, IPARGS(addr), IPARGS(mask));

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

	verbose("Default route via " IPPRINT "\n", IPARGS(gateway));

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
		error("failed to get mac address\n");
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

static int _find_opt(struct dhcp_packet *p, uint8_t *from, uint8_t code, uint8_t **opt, unsigned int *len)
{
	uint8_t *_opt = from ? from : p->options;
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
			if (_opt == opt_end)
				goto out;

			_len = *_opt++;

			if ((opt_end - _opt) < _len)
				goto out;

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

static int find_opt(struct dhcp_packet *p, uint8_t code, uint8_t **opt, unsigned int *len)
{
	return _find_opt(p, NULL, code, opt, len);
}

static int _find_opt_u32(struct dhcp_packet *p, uint8_t *from, uint8_t **pos, uint8_t code, uint32_t *opt)
{
	unsigned int len;
	uint8_t *_opt;
	uint32_t tmp;
	int ret;

	ret = _find_opt(p, from, code, &_opt, &len);
	if (ret)
		return ret;

	if (len != 4)
		return -EINVAL;

	if (pos)
		*pos = _opt;
	memcpy(&tmp, _opt, sizeof(tmp));
	*opt = ntohl(tmp);

	return 0;
}


static int find_opt_u32(struct dhcp_packet *p, uint8_t code, uint32_t *opt)
{
	return _find_opt_u32(p, NULL, NULL, code, opt);
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

static int write_file(const char *path, const void *what, size_t len)
{
	int __cleanup_fd fd = -1;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		error("Failed to open %s: %d\n", path, errno);
		return -1;
	}

	if (write(fd, what, len) != (int) len) {
		error("Failed to write %s: %d\n", path, errno);
		return -1;
	}

	return 0;
}

/* Write out a state file for nosey processes to look at */
static void write_state(const struct config *config)
{
	struct dhcpc_lease lease = {
		.address = config->address,
		.subnet_mask = config->subnet_mask,
		.router = config->router,
		.serverid = config->serverid,
		.lease_time = config->lease_time,
	};
	struct dhcpc_dns dns = { 0 };
	unsigned int i;

	write_file(DHCPC_LEASE_PATH, &lease, sizeof(lease));

	for (i = 0; i < config->num_dns; i++)
		dns.server[dns.num++] = config->dns[i];

	if (dns.num)
		write_file(DHCPC_DNS_PATH, &dns, sizeof(dns));
}

/*
 * Schedule to be run again later by init.
 * We don't actually do dhcp renewal, we just do a normal request to
 * save on code.
 */
static void ask_to_run_again(const char *interface, unsigned int secs)
{
	const char *argv[] = {
		DHCPC_PATH,
		"-i",
		interface,
		NULL
	};
	char name[32];

	if (snprintf(name, sizeof(name), "dhcpc.%s", interface)
	    >= (int) sizeof(name)) {
		error("Interface name is too long: %s\n", interface);
		return;
	}

	verbose("Asking to run again in %u seconds\n", secs);

	later_ask(name, secs, argv);
}

int do_discover(struct context *cntx, struct dhcp_packet *p)
{
	uint32_t addr, subnet, router, serverid;
	uint32_t lease;
	char subnet_str[INET_ADDRSTRLEN];
	char router_str[INET_ADDRSTRLEN];
	char addr_str[INET_ADDRSTRLEN];
	char dns_str[INET_ADDRSTRLEN];
	unsigned int dns_len;
	uint8_t msgtype;
	uint8_t *dns;
	int ret;
	int i;

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
	if (ret) {
		verbose("Failed to find server id\n");
		return ret;
	}

	if (find_opt_u32(p, OPT_ROUTER, &router))
		router = 0;

	/* Currently unused, will be used when we handle renewal */
	if (find_opt_u32(p, OPT_LEASE, &lease))
		lease = 0;

	addr = ntohl(p->yiaddr);

	addr_to_str(addr, addr_str, sizeof(addr_str));
	addr_to_str(subnet, subnet_str, sizeof(subnet_str));
	addr_to_str(router, router_str, sizeof(router_str));

	verbose("Got offer for %s (%s), router %s\n",
		addr_str, subnet_str, router_str);

	cntx->config.serverid = serverid;
	cntx->config.address = addr;
	cntx->config.subnet_mask = subnet;
	cntx->config.router = router;
	cntx->config.lease_time = lease;

	/* All of the DNS servers are in one option, 4 bytes each */
	cntx->config.num_dns = 0;
	if (!find_opt(p, OPT_DNS, &dns, &dns_len)) {
		for (i = 0; (i + 4) <= dns_len; i += 4) {
			uint32_t tmp;

			if (cntx->config.num_dns == ARRAY_SIZE(cntx->config.dns))
				break;

			memcpy(&tmp, dns + i, sizeof(tmp));
			cntx->config.dns[cntx->config.num_dns++] = ntohl(tmp);

			inet_ntop(AF_INET, &tmp, dns_str, sizeof(dns_str));
			verbose("DNS server: %s\n", dns_str);
		}
	}

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
	unsigned int renew;
	struct dhcp_packet p;
	int ret, tries;
	int c;

	while ((c = getopt(argc, argv, "i:")) != -1) {
		switch (c) {
		case 'i':
			cntx.interface = optarg;
			break;
		}
	}

	cntx.xid = (uint32_t) time(NULL) ^ (uint32_t) getpid();

	verbose("Bringing %s up\n", cntx.interface);
	ret = smolutils_net_interface_set_up(cntx.interface);
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
		ask_to_run_again(cntx.interface, RETRY_SECONDS);
		return 1;
	}

	interface_set_address(cntx.interface, cntx.config.address, cntx.config.subnet_mask);

	if (cntx.config.router)
		interface_set_default_route(cntx.interface, cntx.config.router);

	write_state(&cntx.config);

	renew = cntx.config.lease_time / RENEW_FRACTION;
	ask_to_run_again(cntx.interface, renew ? renew : RETRY_SECONDS);

	return 0;
}
