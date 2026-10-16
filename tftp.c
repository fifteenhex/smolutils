// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "tftp"

#include "common.h"
#include "inc/net.h"
#include "resolv.h"

#define USAGE "usage: tftp -g|-p -r <remote file> [-l <local file>] " \
	      "[-b <block size>] <host> [port]\n" \
	      "       -l defaults to the name of the remote file, - is stdio\n"

#define TFTP_PORT		69
#define TFTP_TIMEOUT		3
#define TFTP_RETRIES		5
#define TFTP_HDR_LEN		4
#define TFTP_DEF_BLKSIZE	512
#define TFTP_MIN_BLKSIZE	8
#define TFTP_MAX_BLKSIZE	1468
#define TFTP_MODE		"octet"
#define TFTP_OPT_BLKSIZE	"blksize"

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
	char blksize[8];

	tftp_set_header(t, op, 0);
	t->txlen = 2;

	if (!tftp_add_string(t, filename)) {
		error("Remote filename is too long\n");
		return -1;
	}

	tftp_add_string(t, TFTP_MODE);

	if (t->want_blksize) {
		snprintf(blksize, sizeof(blksize), "%u", t->want_blksize);
		tftp_add_string(t, TFTP_OPT_BLKSIZE);
		tftp_add_string(t, blksize);
	}

	return 0;
}

static bool tftp_option_is(const char *name, const char *want)
{
	for (; *name && *want; name++, want++) {
		char c = *name;

		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';

		if (c != *want)
			return false;
	}

	return *name == *want;
}

static int tftp_parse_oack(struct tftp *t)
{
	const char *end = (const char *) t->rx + t->rxlen;
	const char *p = (const char *) t->rx + 2;

	while (p < end) {
		const char *name = p;
		const char *value;
		size_t len;

		len = strnlen(name, end - name);
		if (name + len >= end)
			break;
		value = name + len + 1;

		len = strnlen(value, end - value);
		if (value + len >= end)
			break;
		p = value + len + 1;

		verbose("option %s = %s\n", name, value);

		if (tftp_option_is(name, TFTP_OPT_BLKSIZE)) {
			unsigned long blksize;

			/* blk size can only be smaller than what we requested */
			if (!t->want_blksize ||
			    !parse_ul(value, 10, &blksize) ||
			    blksize < TFTP_MIN_BLKSIZE ||
			    blksize > t->want_blksize) {
				error("Server offered a bad block size: %s\n", value);
				tftp_send_error(t, ERR_OPTION, "bad blksize");
				return -1;
			}

			t->blksize = blksize;
		}
	}

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

static int tftp_transact(struct tftp *t, enum tftp_op op, uint16_t block,
			 bool oack_ok, bool answer_dup)
{
	int tries;

	for (tries = 0; tries < TFTP_RETRIES; tries++) {
		if (tftp_send(t))
			return -1;

		while (true) {
			int got = tftp_recv(t);
			uint16_t got_block;

			/* timeout, send it again */
			if (got == 0)
				break;

			if (got < 0)
				return -1;

			if (got == OP_OACK && oack_ok)
				return got;

			got_block = get_unaligned_be16(t->rx + 2);

			if (got == (int) op && got_block == block)
				return got;

			if (got == (int) op && got_block == (uint16_t) (block - 1) &&
			    answer_dup) {
				verbose("Duplicate block %d, resending\n", got_block);
				if (tftp_send(t))
					return -1;
				continue;
			}

			verbose("Ignoring op %d block %d\n", got, got_block);
		}

		verbose("Timeout, retransmitting\n");
	}

	error("Timed out waiting for the server\n");

	return -1;
}

static int tftp_get(struct tftp *t, const char *remote, int fd)
{
	unsigned long total = 0;
	bool oack_ok = true;
	uint16_t block = 1;
	int ret;

	ret = tftp_build_request(t, OP_RRQ, remote);
	if (ret)
		return -1;

	while (true) {
		size_t len;

		ret = tftp_transact(t, OP_DATA, block, oack_ok, true);
		if (ret < 0)
			return -1;

		if (ret == OP_OACK) {
			if (tftp_parse_oack(t))
				return -1;
			tftp_set_header(t, OP_ACK, 0);
			oack_ok = false;
			continue;
		}

		len = t->rxlen - TFTP_HDR_LEN;
		if (len > t->blksize) {
			error("Block %d is too big: %u\n", block, (unsigned int) len);
			tftp_send_error(t, ERR_ILLEGAL_OP, "block too big");
			return -1;
		}

		if (write_full(fd, t->rx + TFTP_HDR_LEN, len)) {
			error("Failed to write local file: %d\n", errno);
			tftp_send_error(t, ERR_DISK_FULL, "write failed");
			return -1;
		}

		total += len;
		tftp_set_header(t, OP_ACK, block);
		oack_ok = false;

		/* a short block is the last one */
		if (len < t->blksize) {
			tftp_send(t);
			break;
		}

		block++;
	}

	debug("Received %lu bytes\n", total);

	return 0;
}

static int tftp_read_block(struct tftp *t, int fd, uint16_t block)
{
	int len;

	tftp_set_header(t, OP_DATA, block);

	len = read_full(fd, t->tx + TFTP_HDR_LEN, t->blksize);
	if (len < 0) {
		error("Failed to read local file: %d\n", errno);
		tftp_send_error(t, ERR_UNDEFINED, "read failed");
		return -1;
	}

	t->txlen += len;

	return len;
}

static int tftp_put(struct tftp *t, const char *remote, int fd)
{
	unsigned long total = 0;
	bool oack_ok = true;
	uint16_t block = 0;
	bool last = false;
	int ret;

	ret = tftp_build_request(t, OP_WRQ, remote);
	if (ret)
		return -1;

	while (true) {
		int len;

		ret = tftp_transact(t, OP_ACK, block, oack_ok, false);
		if (ret < 0)
			return -1;

		if (ret == OP_OACK && tftp_parse_oack(t))
			return -1;
		oack_ok = false;

		if (last)
			break;

		block++;

		len = tftp_read_block(t, fd, block);
		if (len < 0)
			return -1;

		total += len;

		/* A short block to signal that we are done */
		if ((unsigned int) len < t->blksize)
			last = true;
	}

	debug("Sent %lu bytes\n", total);

	return 0;
}

static int open_local(const char *path, bool get)
{
	int fd;

	if (strcmp(path, "-") == 0)
		return dup(get ? STDOUT_FILENO : STDIN_FILENO);

	if (get)
		fd = open_with_msgs(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	else
		fd = open_with_msgs(path, O_RDONLY, 0);

	return fd;
}

int main (int argc, char **argv, char **envp)
{
	uint16_t port = TFTP_PORT;
	int __cleanup_fd sock = -1;
	const char *remote = NULL;
	int __cleanup_fd fd = -1;
	const char *local = NULL;
	struct tftp *t = &tftp;
	unsigned long blksize;
	enum tftp_op op = 0;
	const char *host;
	int ret;
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
		if (!parse_port(argv[optind + 1], &port)) {
			error("Not a port: %s\n", argv[optind + 1]);
			return 1;
		}
	}

	if (!local)
		local = path_basename(remote);

	sockaddr_in_init(&t->server, INADDR_ANY, port);
	t->blksize = TFTP_DEF_BLKSIZE;

	if (resolv_host(host, &t->server.sin_addr)) {
		error("Failed to resolve %s\n", host);
		return 1;
	}

	sock = smolutils_net_udp_socket(TFTP_TIMEOUT);
	if (sock < 0)
		return 1;

	t->sock = sock;

	fd = open_local(local, op == OP_RRQ);
	if (fd < 0)
		return 1;

	if (op == OP_RRQ)
		ret = tftp_get(t, remote, fd);
	else
		ret = tftp_put(t, remote, fd);

	return ret ? 1 : 0;
}
