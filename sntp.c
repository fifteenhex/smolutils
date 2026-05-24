// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#define VERBOSE
#include "common.h"
#include "net.h"
#include "resolv.h"

#define NTP_SERVER	"pool.ntp.org"
#define NTP_PORT	123
#define NTP_VERSION	4
#define NTP_MODE_CLIENT	3
#define NTP_PACKET_SIZE	48
#define NTP_UNIX_DELTA	2208988800UL

struct ntp_packet {
	uint8_t  li_vn_mode;
	uint8_t  stratum;
	uint8_t  poll;
	int8_t   precision;
	uint32_t root_delay;
	uint32_t root_dispersion;
	uint32_t ref_id;
	uint32_t ref_ts_sec;
	uint32_t ref_ts_frac;
	uint32_t orig_ts_sec;
	uint32_t orig_ts_frac;
	uint32_t rx_ts_sec;
	uint32_t rx_ts_frac;
	uint32_t tx_ts_sec;
	uint32_t tx_ts_frac;
};
_Static_assert(sizeof(struct ntp_packet) == NTP_PACKET_SIZE, "");

static int do_sntp(int sock, struct sockaddr_in *addr)
{
	struct ntp_packet pkt = {
		.li_vn_mode = (NTP_VERSION << 3) | NTP_MODE_CLIENT,
	};
	uint32_t ntp_frac;
	uint32_t ntp_sec;
	int ret;

	ret = sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *) addr, sizeof(*addr));
	if (ret < 0) {
		error("Failed to send SNTP packet\n");
		return -1;
	}

	ret = recvfrom(sock, &pkt, sizeof(pkt), 0, NULL, NULL);
	if (ret < 0) {
		error("No SNTP response\n");
		return -1;
	}

	if (ret != sizeof(pkt)) {
		error("Bad recv'd message size\n");
		return -1;
	}

	ntp_sec  = ntohl(pkt.tx_ts_sec);
	ntp_frac = ntohl(pkt.tx_ts_frac);

	debug("NTP time %u.%u\n", (unsigned int) ntp_sec, (unsigned int) ntp_frac);

	return 0;
}

int main (int argc, char **argv, char **envp)
{
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(NTP_PORT)
	};
	struct resolv_buf server_addresses;
	int __cleanup_fd sock = -1;
	int ret;
	int i;

	ret = resolv_doit(NTP_SERVER, &server_addresses);
	if (ret)
		return 1;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return 1;

	ret = smolutils_net_setsockrxtimeout(sock, 5);
	if (ret)
		return 1;

	for (i = 0; i < server_addresses.num_addr_v4; i++) {
		memcpy(&addr.sin_addr.s_addr, &server_addresses.addr_v4[i], sizeof(addr.sin_addr.s_addr));
		ret = do_sntp(sock, &addr);

		/* success */
		if (!ret)
			break;
	}

	return 0;
}
