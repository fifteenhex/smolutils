// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "net.h"
#include "resolv.h"

#define TIMEOUT 2
#define ICMP_ECHO 8
#define ICMP_ECHOREPLY 0
#define IP_MIN_HDR_LEN 20
struct icmphdr {
	__u8	type;
	__u8	code;
	__sum16	checksum;
	union {
		struct {
			__be16	id;
			__be16	sequence;
		} echo;
		__be32	gateway;
		struct {
			__be16	__unused;
			__be16	mtu;
		} frag;
		__u8	reserved[4];
	} un;
};

static const char payload[] = "smol4life";
#define PACKET_LEN (sizeof(struct icmphdr) + sizeof(payload))

static uint16_t checksum(void *data, int len) {
	uint16_t *buf = data;
	uint32_t sum = 0;

	while (len > 1) {
		sum += *buf++;
		len -= 2;
	}
	if (len == 1)
		sum += *(uint8_t *)buf;

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);

	return ~sum;
}

static int send_request(int sock, struct sockaddr_in *dst,
			uint16_t id, uint16_t seq)
{
	uint8_t buf[PACKET_LEN] = { 0 };
	struct icmphdr *hdr = (struct icmphdr *)buf;
	int ret;

	hdr->type = ICMP_ECHO;
	hdr->un.echo.id = id;
	hdr->un.echo.sequence = seq;
	memcpy(buf + sizeof(struct icmphdr), payload, sizeof(payload));
	hdr->checksum = checksum(buf, PACKET_LEN);

	ret = sendto(sock, buf, sizeof(buf), 0, (struct sockaddr *)dst, sizeof(*dst));
	if (ret != sizeof(buf)) {
		error("Failed to send ICMP packet: %d:%d\n", ret, errno);
		return -1;
	}

	return 0;
}

/* 0 - no error, no response, 1 - no error, response, <0 error */
static int wait_for_response(int sock, uint16_t id, uint16_t seq)
{
	struct sockaddr_in src;
	socklen_t srclen = sizeof(src);
	struct icmphdr hdr;
	uint8_t resp[256] = { 0 };
	int iphdrlen;
	ssize_t len;

	/*
	 * We might need to check multiple packets to find our response,
	 * I think we could get stuck in this loop if someone dribbles icmp packets? revist
	 */
	while (true) {
		len = recvfrom(sock, resp, sizeof(resp), 0,
			       (struct sockaddr *)&src, &srclen);

		if (len < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				error("Timeout waiting for response\n");
				return 0;
			}

			return -1;
		}

		/* We get the IP header too so len always has to be bigger */
		if (len < IP_MIN_HDR_LEN)
			continue;

		iphdrlen = (resp[0] & 0xf) * 4;
		if (len < (iphdrlen + (int) sizeof(hdr)))
			continue;

		memcpy(&hdr, resp + iphdrlen, sizeof(hdr));

		if (hdr.type != ICMP_ECHOREPLY)
			continue;

		if (hdr.un.echo.id != id || hdr.un.echo.sequence != seq)
			continue;

		return 1;
	}
}

static int try_to_lookup_host(const char *host, struct in_addr *result)
{
	struct resolv_buf addresses;
	int ret;

	ret = resolv_doit(host, &addresses);
	if (ret)
		return ret;

	if (!addresses.num_addr_v4)
		return -1;

	memcpy(result, &addresses.addr_v4[0], sizeof(*result));

	return 0;
}

int main (int argc, char **argv, char **envp)
{
	struct timeval t0 = { 0 }, t1 = { 0 };
	struct sockaddr_in dst = {
		.sin_family = AF_INET,
	};
	int __cleanup_fd sock = -1;
	const char *host;
	uint16_t id;
	int replies;
	int ret;
	int i;

	if (argc != 2)
		return 1;

	host = argv[1];

	/* Do lookup if directly converting from an ipv4 address failed */
	if (!inet_aton(host, &dst.sin_addr)) {
		char ip[INET_ADDRSTRLEN];

		ret = try_to_lookup_host(host, &dst.sin_addr);
		if (ret) {
			error("Failed to resolv host\n");
			return 1;
		}

		inet_ntop(AF_INET, &dst.sin_addr, ip, sizeof(ip));
		verbose("Resolved %s to %s\n", host, ip);
	}

	sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sock < 0) {
		error("Failed to create socket: %d\n", errno);
		return 1;
	}

	ret = smolutils_net_setsockrxtimeout(sock, TIMEOUT);
	if (ret)
		return 1;

	id = (uint16_t) getpid();
	replies = 0;

	for (i = 0; i < 10; i++)
	{
		uint16_t seq = i + 1;
		long ms;

		ret = send_request(sock, &dst, id, seq);
		if (ret)
			return 1;

		gettimeofday(&t0, NULL);

		ret = wait_for_response(sock, id, seq);
		if (ret == 0)
			continue;

		if (ret < 0)
			return 1;

		gettimeofday(&t1, NULL);

		ms = ((t1.tv_sec - t0.tv_sec) * 1000) +
		     ((t1.tv_usec - t0.tv_usec) / 1000);

		printf("Reply from %s: seq=%d time=%ldms\n", host, seq, ms);
		replies++;
		sleep(2);
	}

	return replies ? 0 : 1;
}
