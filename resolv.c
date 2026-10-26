// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "inc/asserts.h"
#include "inc/net.h"
#include "inc/resolv.h"
#include "dhcpc.h"
#include "inc/misc.h"

#define USAGE "usage: resolv [-m <memfd>] <hostname>\n"

static uint16_t dns_id;

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

static_assert_size(struct dns_hdr, 12);

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
	put_unaligned_be16(buf + off, 1);
	off += 2;
	/* QCLASS IN */
	put_unaligned_be16(buf + off, 1);
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

#define DNS_FLAG_QR	0x8000
#define DNS_FLAG_TC	0x0200
#define DNS_OPCODE(f)	(((f) >> 11) & 0xf)
#define DNS_RCODE(f)	((f) & 0xf)
#define DNS_NXDOMAIN	3

/* Validate that a packet is probably a reply to our request */
static bool is_reply_to(const uint8_t *query, int qlen, const uint8_t *reply, int len)
{
	const struct dns_hdr *hdr = (const struct dns_hdr *) reply;
	uint16_t flags = ntohs(hdr->flags);

	if (len < qlen || hdr->id != ((const struct dns_hdr *) query)->id)
		return false;

	if (!(flags & DNS_FLAG_QR) || DNS_OPCODE(flags) || (flags & DNS_FLAG_TC))
		return false;

	if (ntohs(hdr->qdcount) != 1)
		return false;

	return !memcmp(query + sizeof(*hdr), reply + sizeof(*hdr), qlen - sizeof(*hdr));
}

static int parse_response(const uint8_t *buf, int len,
			  int (*cb)(uint32_t v4addr, void *priv), void *priv)
{
	const struct dns_hdr *hdr = (const struct dns_hdr *)buf;
	int ancount;
	int off = sizeof(*hdr);
	int found = 0;

	if (len < (int) sizeof(*hdr))
		return -1;

	/* Not an answer to anything we asked */
	if (ntohs(hdr->id) != dns_id)
		return -1;

	ancount = ntohs(hdr->ancount);

	/* skip question section */
	off = skip_name(buf, len, off);
	if (off < 0 || (off + 4) > len)
		return -1;
	/* QTYPE + QCLASS */
	off += 4;

	for (int i = 0; i < ancount && off < len; i++) {
		uint16_t type, rdlen;

		off = skip_name(buf, len, off);
		if (off < 0 || off + 10 > len)
			return -1;

		type = get_unaligned_be16(buf + off);
		off += 2;
		/* class */
		off += 2;
		/* ttl   */
		off += 4;
		rdlen = get_unaligned_be16(buf + off);
		off += 2;

		if ((off + rdlen) > len)
			return -1;

		if (type == 1 && rdlen == 4) {
			uint32_t v4addr;

			memcpy(&v4addr, buf + off, sizeof(v4addr));
			cb(v4addr, priv);
			found++;
		}

		off += rdlen;
	}

	return found;
}

/* Try to get dns servers from dhcpc's state, fallback otherwise */
static unsigned int dns_servers(uint32_t *servers, unsigned int max)
{
	struct dhcpc_dns dns;
	struct in_addr fallback;
	unsigned int num = 0;
	unsigned int i;

	if (!read_struct(DHCPC_DNS_PATH, &dns))
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
	int memfd;
	uint32_t servers[DHCPC_MAX_DNS];
	struct sockaddr_in srv;
	int __cleanup_fd sock = -1;
	unsigned int num_servers;
	const char *hostname;
	uint8_t buf[BUF_SZ];
	uint8_t reply[BUF_SZ];
	unsigned int i;
	int rcode;
	int qlen;
	int ret;
	int len;
	int c;

	struct mapping __cleanup_mapping result_map = MAPPING_NONE;
	bool memfd_mode = false;

	while ((c = getopt(argc, argv, "m:")) != -1) {
		switch (c) {
		case 'm':
			if (!parse_fd_with_msgs(optarg, &memfd))
				return EX_USAGE;
			if (!resolv_mapbuf(&result_map, memfd))
				return EX_OSERR;
			memset(result_map.at, 0, sizeof(struct resolv_buf));
			memfd_mode = true;
			break;

		default:
			usage(USAGE);
			return EX_USAGE;
		}
	}

	if (optind != argc - 1) {
		usage(USAGE);
		return EX_USAGE;
	}

	hostname = argv[optind];

	num_servers = dns_servers(servers, ARRAY_SIZE(servers));

	sock = smolutils_net_udp_socket(DNS_TIMEOUT);
	if (sock < 0)
		return EX_OSERR;

	dns_id = (uint16_t) random_u32();

	qlen = build_query(hostname, buf, sizeof(buf));
	if (qlen < 0) {
		error("Bad hostname\n");
		return EX_USAGE;
	}

	for (i = 0, len = 0; i < num_servers && len < 1; i++) {
		struct timespec deadline = deadline_after(DNS_TIMEOUT);

		sockaddr_in_init(&srv, servers[i], DNS_PORT);

		if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)))
			continue;

		ret = sendto(sock, buf, qlen, 0, NULL, 0);
		if (ret < 0)
			continue;

		while ((len = smolutils_net_recv(sock, reply, sizeof(reply), sizeof(struct dns_hdr),
						 NULL, NULL, &deadline)) > 0 &&
		       !is_reply_to(buf, qlen, reply, len))
			;

		if (len < 1)
			continue;

		rcode = DNS_RCODE(ntohs(((struct dns_hdr *) reply)->flags));
		if (rcode == DNS_NXDOMAIN) {
			error("%s: no such name\n", hostname);
			return EX_NOINPUT;
		}

		if (rcode) {
			verbose("Server error %d\n", rcode);
			len = 0;
		}
	}

	if (len < 1) {
		error("%s: no answer\n", hostname);
		return EX_UNAVAILABLE;
	}

	if (memfd_mode)
		ret = parse_response(reply, len, parse_response_cb_memfd,
				     result_map.at);
	else
		ret = parse_response(reply, len, parse_response_cb_stdout, NULL);

	if (ret <= 0) {
		error("%s: %s\n", hostname,
		      ret ? "bad response" : "no address");
		return ret ? EX_DATAERR : EX_NOINPUT;
	}

	return 0;
}
