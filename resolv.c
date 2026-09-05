// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "net.h"

#include "resolv.h"
#include "dhcpc.h"

static uint16_t dns_id;

static void pick_dns_id(void)
{
	if (getrandom(&dns_id, sizeof(dns_id), 0) != sizeof(dns_id))
		dns_id = (uint16_t) getpid();
}
#define DNS_SERVER	"8.8.8.8"
#define DNS_PORT	53
#define DNS_TIMEOUT	5
#define BUF_SZ		512

struct dns_hdr {
	uint16_t id;
	uint16_t flags;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
} __attribute__((packed));

/* Avoid doing unaligned accesses on 68000 */
static uint16_t dns_get16(const uint8_t *buf)
{
	uint16_t v;

	memcpy(&v, buf, sizeof(v));

	return ntohs(v);
}

static void dns_put16(uint8_t *buf, uint16_t v)
{
	v = htons(v);

	memcpy(buf, &v, sizeof(v));
}

static int encode_name(const char *name, uint8_t *out, int out_len)
{
	int off = 0;

	while (*name) {
		const char *dot = strchr(name, '.');
		int len = dot ? dot - name : (int)strlen(name);

		/* Skip a stray dot */
		if (len == 0) {
			name++;
			continue;
		}

		/* A label is 63 bytes at most */
		if (len > 63)
			return -1;

		/* Length byte, the label, and the terminator */
		if ((off + 1 + len + 1) > out_len)
			return -1;

		out[off++] = len;
		memcpy(out + off, name, len);
		off += len;
		name += len + (dot ? 1 : 0);
	}

	out[off++] = 0;

	return off;
}

static int build_query(const char *host, uint8_t *buf, int buf_len)
{
	struct dns_hdr *hdr = (struct dns_hdr *)buf;
	int off = sizeof(*hdr);
	int ret;

	memset(hdr, 0, sizeof(*hdr));
	hdr->id = htons(dns_id);
	/* RD set */
	hdr->flags = htons(0x0100);
	hdr->qdcount = htons(1);

	/* Leave room for the QTYPE and QCLASS */
	ret = encode_name(host, buf + off, buf_len - off - 4);
	if (ret < 0)
		return -1;
	off += ret;

	/* QTYPE  A */
	dns_put16(buf + off, 1);
	off += 2;
	/* QCLASS IN */
	dns_put16(buf + off, 1);
	off += 2;

	return off;
}

static int skip_name(const uint8_t *buf, int buf_len, int off)
{
	while (off < buf_len) {
		uint8_t len = buf[off];

		if ((len & 0xc0) == 0xc0) { /* pointer */
			return off + 2;
		}

		if (len == 0)
			return off + 1;

		off += 1 + len;
	}
	return -1;
}

static int parse_response_cb_stdout(uint32_t v4addr, void *priv)
{
	char ip[INET_ADDRSTRLEN];

	inet_ntop(AF_INET, &v4addr, ip, sizeof(ip));
	printf("%s\n", ip);

	return 0;
}

static int parse_response_cb_memfd(uint32_t v4addr, void *priv)
{
	struct resolv_buf *resolv_buf = (struct resolv_buf *) priv;
	unsigned int num_results = resolv_buf->num_addr_v4;

	if (num_results >= RESOLV_MAX_RESULTS)
		return 0;

	resolv_buf->addr_v4[num_results].s_addr = v4addr;
	resolv_buf->num_addr_v4++;

	return 0;
}

static void parse_response(const uint8_t *buf, int len,
			   int (*cb)(uint32_t v4addr, void *priv), void *priv)
{
	const struct dns_hdr *hdr = (const struct dns_hdr *)buf;
	int ancount;
	int off = sizeof(*hdr);

	if (len < (int) sizeof(*hdr))
		return;

	/* Not an answer to anything we asked */
	if (ntohs(hdr->id) != dns_id)
		return;

	ancount = ntohs(hdr->ancount);

	/* skip question section */
	off = skip_name(buf, len, off);
	if (off < 0 || (off + 4) > len)
		return;
	/* QTYPE + QCLASS */
	off += 4;

	for (int i = 0; i < ancount && off < len; i++) {
		uint16_t type, rdlen;

		off = skip_name(buf, len, off);
		if (off < 0 || off + 10 > len)
			return;

		type = dns_get16(buf + off);
		off += 2;
		/* class */
		off += 2;
		/* ttl   */
		off += 4;
		rdlen = dns_get16(buf + off);
		off += 2;

		if ((off + rdlen) > len)
			return;

		if (type == 1 && rdlen == 4) {
			uint32_t v4addr;

			memcpy(&v4addr, buf + off, sizeof(v4addr));
			cb(v4addr, priv);
		}

		off += rdlen;
	}
}

static int setup_memfd(const char *memfd_str, struct resolv_buf **resolv_buf)
{
	size_t mapsz = sizeof(struct resolv_buf);
	unsigned long tmp;
	char *endptr;
	int memfd;

	tmp = strtoul(memfd_str, &endptr, 10);
	if (endptr == memfd_str || *endptr != '\0')
		return -1;

	memfd = tmp;

	if (resolv_mapbuf(memfd, resolv_buf))
		return -1;

	memset(*resolv_buf, 0, mapsz);

	return 0;
}

/* Try to get dns servers from dhcpc's state, fallback otherwise */
static unsigned int dns_servers(uint32_t *servers, unsigned int max)
{
	struct dhcpc_dns dns;
	struct in_addr fallback;
	unsigned int num = 0;
	unsigned int i;

	if (!dhcpc_read_state(DHCPC_DNS_PATH, &dns, sizeof(dns)))
		for (i = 0; i < dns.num && i < DHCPC_MAX_DNS && num < max; i++)
			servers[num++] = htonl(dns.server[i]);

	if (num)
		return num;

	verbose("No servers from DHCP, falling back to " DNS_SERVER "\n");

	inet_pton(AF_INET, DNS_SERVER, &fallback);
	servers[num++] = fallback.s_addr;

	return num;
}

int main (int argc, char **argv, char **envp)
{
	struct sockaddr_in srv = {
		.sin_family = AF_INET,
		.sin_port   = htons(DNS_PORT),
	};
	uint32_t servers[DHCPC_MAX_DNS];
	int __cleanup_fd sock = -1;
	unsigned int num_servers;
	const char *hostname;
	uint8_t buf[BUF_SZ];
	unsigned int i;
	int qlen;
	int ret;
	int len;
	int c;

	bool memfd_mode = false;
	struct resolv_buf *resolv_buf;

	/* Fix this.. */
	if (argc < 2)
		return 1;

        while ((c = getopt(argc, argv, "m:")) != -1) {
                switch (c) {
                case 'm':
			ret = setup_memfd(optarg, &resolv_buf);
			if (ret)
				return 1;
			memfd_mode = true;
                        break;
                }
        }

        hostname = (optind < argc) ? argv[optind] : ".";

	num_servers = dns_servers(servers, ARRAY_SIZE(servers));

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return 1;

	ret = smolutils_net_setsockrxtimeout(sock, DNS_TIMEOUT);
	if (ret)
		return 1;

	pick_dns_id();

	qlen = build_query(hostname, buf, sizeof(buf));
	if (qlen < 0) {
		error("Bad hostname\n");
		return 1;
	}

	/* Ask each one in turn, the first to answer wins */
	for (i = 0, len = 0; i < num_servers && len < 1; i++) {
		srv.sin_addr.s_addr = servers[i];

		if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)))
			continue;

		ret = sendto(sock, buf, qlen, 0, NULL, 0);
		if (ret < 0)
			continue;

		len = recv(sock, buf, sizeof(buf), 0);
	}

	if (len < 1)
		return 1;

	if (memfd_mode)
		parse_response(buf, len, parse_response_cb_memfd, resolv_buf);
	else
		parse_response(buf, len, parse_response_cb_stdout, NULL);

	return 0;
}
