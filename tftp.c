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

int main (int argc, char **argv, char **envp)
{
	unsigned long port = TFTP_PORT;
	const char *remote = NULL;
	const char *local = NULL;
	unsigned long blksize = 0;
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

	return 0;
}
