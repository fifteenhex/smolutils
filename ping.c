// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define TOOL_NAME	"ping"
#define TOOL_USAGE	"<host>"
#include "inc/tool.h"
#include "inc/net.h"
#include "inc/resolv.h"
#include "inc/misc.h"

#define TIMEOUT 2
#define ICMP_ECHO 8
#define ICMP_ECHOREPLY 0
#define IP_MIN_HDR_LEN 20

/* From <linux/icmp.h> pulls in libc headers */
#define SOL_RAW		255
#define ICMP_FILTER	1
struct icmp_filter {
	__u32 data;
};
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

static_assert_size(struct icmphdr, 8);

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
static int wait_for_response(int sock, const struct sockaddr_in *dst, uint16_t id, uint16_t seq)
{
	struct timespec deadline = deadline_after(TIMEOUT);
	uint8_t resp[256] = { 0 };
	struct icmphdr hdr;
	int iphdrlen;
	ssize_t len;

	/* We will get packets aside from the one we want so we need to loop */
	while ((len = smolutils_net_recv(sock, resp, sizeof(resp), IP_MIN_HDR_LEN,
					 dst, NULL, &deadline)) > 0) {
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

	if (len < 0) {
		error("Failed to receive: %d\n", errno);
		return -1;
	}

	error("Timeout waiting for response\n");

	return 0;
}

static int prog_ping(int argc, char **argv, char **envp)
{
	struct timeval t0 = { 0 }, t1 = { 0 };
	int __cleanup_fd sock = -1;
	struct icmp_filter filter;
	struct sockaddr_in dst;
	const char *host;
	uint16_t id;
	int replies;
	int ret;
	int i;

	host = argv[1];

	sockaddr_in_init(&dst, INADDR_ANY, 0);

	ret = resolv_host(host, &dst.sin_addr);
	if (ret) {
		error("Failed to resolv host\n");
		return EX_NOINPUT;
	}

	sock = smolutils_net_socket_timeout(SOCK_RAW, IPPROTO_ICMP, TIMEOUT);
	if (sock < 0)
		return EX_OSERR;

	/* Set a filter for ping replies */
	filter.data = ~(1U << ICMP_ECHOREPLY);
	setsockopt(sock, SOL_RAW, ICMP_FILTER, &filter, sizeof(filter));

	id = (uint16_t) random_u32();
	replies = 0;

	for (i = 0; i < 10; i++)
	{
		uint16_t seq = i + 1;
		long ms;

		ret = send_request(sock, &dst, id, seq);
		if (ret)
			return EX_IOERR;

		gettimeofday(&t0, NULL);

		ret = wait_for_response(sock, &dst, id, seq);
		if (ret == 0)
			continue;

		if (ret < 0)
			return EX_IOERR;

		gettimeofday(&t1, NULL);

		/*
		 * The round trip should fit into a long,
		 * if it doesn't you have problems, and truncation
		 * avoids pulling in libgcc on m68k.
		 */
		ms = ((long) (t1.tv_sec - t0.tv_sec) * 1000) +
		     ((long) (t1.tv_usec - t0.tv_usec) / 1000);

		printf("Reply from %s: seq=%d time=%ldms\n", host, seq, ms);
		replies++;
		sleep(2);
	}

	return replies ? 0 : 1;
}

TOOL_SIMPLE_MAIN(1, 1, prog_ping)
