// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "tftp"

#include "common.h"
#include "net.h"
#include "resolv.h"

#define USAGE "usage: tftp -g|-p -r <remote file> [-l <local file>] " \
	      "[-b <block size>] <host> [port]\n" \
	      "       -l defaults to the name of the remote file, - is stdio\n"

#define TFTP_PORT		69
#define TFTP_TIMEOUT		3
#define TFTP_HDR_LEN		4
#define TFTP_DEF_BLKSIZE	512
#define TFTP_MIN_BLKSIZE	8
#define TFTP_MAX_BLKSIZE	1468
#define TFTP_MODE		"octet"

enum tftp_op {
	OP_RRQ = 1,
	OP_WRQ,
	OP_DATA,
	OP_ACK,
	OP_ERROR,
	OP_OACK,
};

enum tftp_err {
	ERR_UNDEFINED = 0,
	ERR_NOT_FOUND,
	ERR_ACCESS,
	ERR_DISK_FULL,
	ERR_ILLEGAL_OP,
	ERR_UNKNOWN_TID,
	ERR_EXISTS,
	ERR_NO_USER,
	ERR_OPTION,
};

struct tftp {
	int sock;
	struct sockaddr_in server;
	bool have_tid;

	unsigned int blksize;
	unsigned int want_blksize;

	uint8_t tx[TFTP_HDR_LEN + TFTP_MAX_BLKSIZE];
	size_t txlen;
	uint8_t rx[TFTP_HDR_LEN + TFTP_MAX_BLKSIZE + 1];
	size_t rxlen;
};

static struct tftp tftp;

static int tftp_send(struct tftp *t)
{
	int ret;

	ret = sendto(t->sock, t->tx, t->txlen, 0,
		     (struct sockaddr *) &t->server, sizeof(t->server));
	if (ret != (int) t->txlen) {
		error("Failed to send: %d\n", errno);
		return -1;
	}

	return 0;
}

static void tftp_set_header(struct tftp *t, enum tftp_op op, uint16_t arg)
{
	put_unaligned_be16(t->tx, op);
	put_unaligned_be16(t->tx + 2, arg);
	t->txlen = TFTP_HDR_LEN;
}

static void tftp_send_error(struct tftp *t, enum tftp_err code,
			    const char *msg)
{
	size_t len = strlen(msg) + 1;

	if (len > sizeof(t->tx) - TFTP_HDR_LEN)
		len = sizeof(t->tx) - TFTP_HDR_LEN;

	tftp_set_header(t, OP_ERROR, code);
	memcpy(t->tx + t->txlen, msg, len);
	t->tx[t->txlen + len - 1] = '\0';
	t->txlen += len;

	tftp_send(t);
}

static bool tftp_add_string(struct tftp *t, const char *s)
{
	size_t len = strlen(s) + 1;

	if (!off_len_fits(t->txlen, len, sizeof(t->tx)))
		return false;

	memcpy(t->tx + t->txlen, s, len);
	t->txlen += len;

	return true;
}

static int tftp_build_request(struct tftp *t, enum tftp_op op,
			      const char *filename)
{
	tftp_set_header(t, op, 0);
	t->txlen = 2;

	if (!tftp_add_string(t, filename)) {
		error("Remote filename is too long\n");
		return -1;
	}

	tftp_add_string(t, TFTP_MODE);

	return 0;
}

/*
 * opcode, 0 on timeout, <0 == error.
 */
static int tftp_recv(struct tftp *t)
{
	while (true) {
		socklen_t fromlen = sizeof(struct sockaddr_in);
		struct sockaddr_in from;
		ssize_t got;
		int op;

		got = recvfrom(t->sock, t->rx, sizeof(t->rx) - 1, 0,
			       (struct sockaddr *) &from, &fromlen);
		if (got < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return 0;

			error("Failed to receive: %d\n", errno);
			return -1;
		}

		if (got < TFTP_HDR_LEN)
			continue;

		if (from.sin_addr.s_addr != t->server.sin_addr.s_addr)
			continue;

		if (t->have_tid) {
			if (from.sin_port != t->server.sin_port) {
				verbose("Packet from unexpected port\n");
				continue;
			}
		} else {
			t->server.sin_port = from.sin_port;
			t->have_tid = true;
		}

		t->rxlen = got;
		t->rx[got] = '\0';

		op = get_unaligned_be16(t->rx);
		if (op == OP_ERROR) {
			error("Server error %d: %s\n",
			      get_unaligned_be16(t->rx + 2),
			      (const char *) t->rx + TFTP_HDR_LEN);
			return -1;
		}

		return op;
	}
}

static int open_local(const char *path, bool get)
{
	int fd;

	if (strcmp(path, "-") == 0)
		return dup(get ? STDOUT_FILENO : STDIN_FILENO);

	if (get)
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	else
		fd = open(path, O_RDONLY);

	if (fd < 0)
		error("Failed to open %s: %d\n", path, errno);

	return fd;
}

int main (int argc, char **argv, char **envp)
{
	unsigned long port = TFTP_PORT;
	int __cleanup_fd sock = -1;
	const char *remote = NULL;
	int __cleanup_fd fd = -1;
	const char *local = NULL;
	struct tftp *t = &tftp;
	unsigned long blksize;
	enum tftp_op op = 0;
	const char *host;
	int c;

	while ((c = getopt(argc, argv, "gpr:l:b:")) != -1) {
		switch (c) {
		case 'g':
			op = OP_RRQ;
			break;
		case 'p':
			op = OP_WRQ;
			break;
		case 'r':
			remote = optarg;
			break;
		case 'l':
			local = optarg;
			break;
		case 'b':
			if (!parse_ul(optarg, 0, &blksize) ||
			    blksize < TFTP_MIN_BLKSIZE ||
			    blksize > TFTP_MAX_BLKSIZE) {
				error("Block size must be %d to %d\n",
				      TFTP_MIN_BLKSIZE, TFTP_MAX_BLKSIZE);
				return 1;
			}
			t->want_blksize = blksize;
			break;
		default:
			usage(USAGE);
			return 1;
		}
	}

	if (!op || !remote || optind >= argc || optind + 2 < argc) {
		usage(USAGE);
		return 1;
	}

	host = argv[optind];

	if (optind + 1 < argc) {
		if (!parse_ul(argv[optind + 1], 0, &port) || !port || port > 0xffff) {
			error("Not a port: %s\n", argv[optind + 1]);
			return 1;
		}
	}

	if (!local)
		local = path_basename(remote);

	t->server.sin_family = AF_INET;
	t->server.sin_port = htons(port);
	t->blksize = TFTP_DEF_BLKSIZE;

	if (resolv_host(host, &t->server.sin_addr)) {
		error("Failed to resolve %s\n", host);
		return 1;
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		error("Failed to create socket: %d\n", errno);
		return 1;
	}

	if (smolutils_net_setsockrxtimeout(sock, TFTP_TIMEOUT))
		return 1;

	t->sock = sock;

	fd = open_local(local, op == OP_RRQ);
	if (fd < 0)
		return 1;

	error("Transfers are not implemented yet\n");

	return 1;
}
