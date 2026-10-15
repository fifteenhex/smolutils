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
#define TFTP_DEF_BLKSIZE	512
#define TFTP_MIN_BLKSIZE	8
#define TFTP_MAX_BLKSIZE	1468

enum tftp_op {
	OP_RRQ = 1,
	OP_WRQ,
	OP_DATA,
	OP_ACK,
	OP_ERROR,
	OP_OACK,
};

struct tftp {
	int sock;
	struct sockaddr_in server;

	unsigned int blksize;
	unsigned int want_blksize;
};

static struct tftp tftp;

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
