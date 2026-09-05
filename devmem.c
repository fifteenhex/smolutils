// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "memfd.h"

#define DEVMEM_PATH	"/dev/mem"
#define XXD_PATH	"/bin/xxd"

#define DEFAULT_LENGTH	4
#define MAX_LENGTH	4096

static uint8_t *block;

static bool read_block(int fd, unsigned long addr, unsigned int len,
		       unsigned int width)
{
	unsigned long page = getpagesize();
	unsigned long base = addr & ~(page - 1);
	unsigned long into = addr - base;
	unsigned long span = into + len;
	volatile void *mapped;
	unsigned int i;

	span = (span + page - 1) & ~(page - 1);

	mapped = mmap(NULL, span, PROT_READ, MAP_SHARED, fd, base);
	if (mapped == MAP_FAILED) {
		error("Failed to map %s at 0x%lx: %d\n",
		      DEVMEM_PATH, base, errno);
		return false;
	}

	for (i = 0; i < len; i += width) {
		volatile void *at = (volatile char *) mapped + into + i;

		switch (width) {
		case 1:
			block[i] = *(volatile uint8_t *) at;
			break;

		case 2: {
			uint16_t v = *(volatile uint16_t *) at;

			memcpy(block + i, &v, sizeof(v));
			break;
		}

		case 8: {
			uint64_t v = *(volatile uint64_t *) at;

			memcpy(block + i, &v, sizeof(v));
			break;
		}

		default: {
			uint32_t v = *(volatile uint32_t *) at;

			memcpy(block + i, &v, sizeof(v));
			break;
		}
		}
	}

	munmap((void *) mapped, span);

	return true;
}

static bool show(unsigned long addr, unsigned int len)
{
	int __cleanup_fd memfd = -1;
	char addrarg[32];
	char fdarg[16];
	char * const argv[] = {
		"xxd",
		"-m",
		fdarg,
		"-o",
		addrarg,
		NULL
	};

	if (memfd_create_and_size("devmem", &memfd, 0) < 0) {
		verbose("Failed to make a memfd: %d\n", errno);
		return false;
	}

	if (write(memfd, block, len) != (int) len) {
		verbose("Failed to fill the memfd: %d\n", errno);
		return false;
	}

	if (lseek(memfd, 0, SEEK_SET) < 0)
		return false;

	snprintf(fdarg, sizeof(fdarg), "%d", memfd);
	snprintf(addrarg, sizeof(addrarg), "0x%lx", addr);

	if (spawn_and_wait_args(XXD_PATH, argv) < 0) {
		verbose("Failed to run %s\n", XXD_PATH);
		return false;
	}

	return true;
}

static bool parse_number(const char *what, unsigned long *out)
{
	char *endptr;

	*out = strtoul(what, &endptr, 0);

	return endptr != what && *endptr == '\0';
}

int main (int argc, char **argv, char **envp)
{
	unsigned long length = DEFAULT_LENGTH;
	int __cleanup_fd fd = -1;
	unsigned long width = 4;
	int ret = 0;
	int i, c;

	while ((c = getopt(argc, argv, "l:w:")) != -1) {
		switch (c) {
		case 'l':
			if (!parse_number(optarg, &length) || !length ||
			    length > MAX_LENGTH) {
				usage("Not a length: %s\n", optarg);
				return 1;
			}
			break;

		case 'w':
			if (!parse_number(optarg, &width) ||
			    (width != 1 && width != 2 && width != 4 &&
			     width != 8)) {
				usage("Not a width: %s\n", optarg);
				return 1;
			}
			break;

		default:
			usage("usage: devmem [-l <length>] [-w <width>] <address>...\n");
			return 1;
		}
	}

	if (optind == argc) {
		usage("usage: devmem [-l <length>] [-w <width>] <address>...\n");
		return 1;
	}

	if (length % width) {
		usage("A length of %lu isn't a whole number of %lu byte reads\n",
		      length, width);
		return 1;
	}

	block = mmap(NULL, MAX_LENGTH, PROT_READ | PROT_WRITE,
		     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (block == MAP_FAILED) {
		error("No room for a %d byte buffer: %d\n", MAX_LENGTH, errno);
		return 1;
	}

	fd = open(DEVMEM_PATH, O_RDONLY);
	if (fd < 0) {
		error("Failed to open %s: %d\n", DEVMEM_PATH, errno);
		return 1;
	}

	for (i = optind; i < argc; i++) {
		unsigned long addr;

		if (!parse_number(argv[i], &addr)) {
			error("Not an address: %s\n", argv[i]);
			ret = 1;
			continue;
		}

		if (addr % width) {
			error("0x%lx isn't %lu byte aligned\n", addr, width);
			ret = 1;
			continue;
		}

		if (!read_block(fd, addr, length, width) ||
		    !show(addr, length))
			ret = 1;
	}

	return ret;
}
