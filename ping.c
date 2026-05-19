// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "net.h"

int inet_aton(const char *cp, struct in_addr *inp) {
	const char *start = cp;
	uint32_t tmp;
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

#define TIMEOUT 2
#define ICMP_ECHO 8
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

#define PAYLOAD_LEN 4

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

static int send_request(int sock, struct sockaddr_in *dst)
{
	uint8_t buf[PACKET_LEN] = { 0 };
	struct icmphdr *hdr = (struct icmphdr *)buf;
	int ret;

	hdr->type = ICMP_ECHO;
	hdr->un.echo.id = (uint16_t)getpid();
	hdr->un.echo.sequence = 1;
	memcpy(buf + sizeof(struct icmphdr), payload, sizeof(payload));
	hdr->checksum = checksum(buf, PACKET_LEN);

	ret = sendto(sock, buf, sizeof(buf), 0, (struct sockaddr *)dst, sizeof(*dst));
	if (ret != sizeof(buf)) {
		error("Failed to send ICMP packet: %d:%d\n", ret, errno);
		return -1;
	}

	return 0;
}

/* 0 - no error, no response, 1 - no error, response, >0 error */
static int wait_for_response(int sock)
{
	struct sockaddr_in src;
	socklen_t srclen = sizeof(src);
	uint8_t resp[256];
	ssize_t len;

	len = recvfrom(sock, resp, sizeof(resp), 0, (struct sockaddr *)&src, &srclen);

	if (len < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			printf("Timeout waiting for response\n");
			return 0;
		}
		else
			return -1;
	}

	return 1;
}

int main (int argc, char **argv, char **envp)
{
	int __cleanup_fd sock = -1;
	struct timeval t0, t1;
	int ret;
	struct sockaddr_in dst = {
		.sin_family = AF_INET,
	};
	int i;

	if (argc != 2)
		return 1;

	if (!inet_aton(argv[1], &dst.sin_addr)) {
		error("Failed to parse address\n");
		return 1;
	}

	sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sock < 0) {
		verbose("Failed to create socket\n");
		return 1;
	}

	ret = smolutils_net_setsockrxtimeout(sock, TIMEOUT);
	if (ret)
		return 1;

	for (i = 0; i < 10; i++)
	{
		ret = send_request(sock, &dst);
		if (ret)
			return 1;

		gettimeofday(&t0, NULL);

		ret = wait_for_response(sock);
		if (ret == 0)
			continue;

		if (ret < 0)
			return 1;

		gettimeofday(&t1, NULL);

		printf("Got reply\n");
		sleep(2);
	}

	return 0;
}
