// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "net.h"

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

static int encode_name(const char *name, uint8_t *out)
{
	int off = 0;

	while (*name) {
		const char *dot = strchr(name, '.');
		int len = dot ? dot - name : (int)strlen(name);

		out[off++] = len;
		memcpy(out + off, name, len);
		off += len;
		name += len + (dot ? 1 : 0);
	}

	out[off++] = 0;

	return off;
}

static int build_query(const char *host, uint8_t *buf)
{
	struct dns_hdr *hdr = (struct dns_hdr *)buf;
	int off = sizeof(*hdr);

	memset(hdr, 0, sizeof(*hdr));
	hdr->id = htons(0x1337);
	/* RD set */
	hdr->flags = htons(0x0100);
	hdr->qdcount = htons(1);

	off += encode_name(host, buf + off);

	/* QTYPE  A */
	*(uint16_t *)(buf + off) = htons(1);
	off += 2;
	/* QCLASS IN */
	*(uint16_t *)(buf + off) = htons(1);
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

static void parse_response(const uint8_t *buf, int len)
{
	const struct dns_hdr *hdr = (const struct dns_hdr *)buf;
	int ancount = ntohs(hdr->ancount);
	int off = sizeof(*hdr);

	/* skip question section */
	off = skip_name(buf, len, off);
	if (off < 0)
		return;
	/* QTYPE + QCLASS */
	off += 4;

	for (int i = 0; i < ancount && off < len; i++) {
		uint16_t type, rdlen;
		char ip[INET_ADDRSTRLEN];

		off = skip_name(buf, len, off);
		if (off < 0 || off + 10 > len)
			return;

		type = ntohs(*(uint16_t *)(buf + off));
		off += 2;
		/* class */
		off += 2;
		/* ttl   */
		off += 4;
		rdlen = ntohs(*(uint16_t *)(buf + off));      off += 2;

		if (type == 1 && rdlen == 4) {
			inet_ntop(AF_INET, buf + off, ip, sizeof(ip));
			printf("%s\n", ip);
		}

		off += rdlen;
	}
}

int main (int argc, char **argv, char **envp)
{
	struct sockaddr_in srv = {
		.sin_family = AF_INET,
		.sin_port   = htons(DNS_PORT),
	};
	int __cleanup_fd sock = -1;
	const char *hostname;
	uint8_t buf[BUF_SZ];
	int ret;
	int len;

	if (argc != 2)
		return 1;

	hostname = argv[1];

	inet_pton(AF_INET, DNS_SERVER, &srv.sin_addr);

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return 1;

	ret = smolutils_net_setsockrxtimeout(sock, DNS_TIMEOUT);
	if (ret)
		return 1;

	len = build_query(hostname, buf);
	ret = sendto(sock, buf, len, 0, (struct sockaddr *)&srv, sizeof(srv));
	if (ret < 0)
		return 1;

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 1)
		return 1;

	parse_response(buf, len);

	return 0;
}
