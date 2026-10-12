// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "memfd.h"

#define DEVMEM_PATH	"/dev/mem"
#define XXD_PATH	"/bin/xxd"

#define DEFAULT_LENGTH	4
#define MAX_LENGTH	4096
#define MAX_ADDRESSES	32

#define USAGE_TEXT \
	"usage: devmem [-l <length>] [-w <width>] [-s <value>] <address>...\n" \
	"       -s writes <value> to each address instead of reading\n"

static uint8_t *block;

struct mapping {
	volatile char *at;
	void *mapped;
	unsigned long span;
};

static void cleanup_mapping(struct mapping *m)
{
	if (m->mapped != MAP_FAILED)
		munmap(m->mapped, m->span);
}

#define __cleanup_mapping __attribute__((cleanup(cleanup_mapping)))

static bool map_block(int fd, unsigned long addr, unsigned int len, int prot,
		      struct mapping *m)
{
	unsigned long page = getpagesize();
	unsigned long base = addr & ~(page - 1);
	unsigned long into = addr - base;

	m->span = (into + len + page - 1) & ~(page - 1);

	m->mapped = mmap(NULL, m->span, prot, MAP_SHARED, fd, base);
	if (m->mapped == MAP_FAILED) {
		error("Failed to map %s at 0x%lx: %d\n",
		      DEVMEM_PATH, base, errno);
		return false;
	}

	m->at = (volatile char *) m->mapped + into;

	return true;
}

static void read_one(volatile void *at, uint8_t *to, unsigned int width)
{
	switch (width) {
	case 1:
		*to = *(volatile uint8_t *) at;
		break;

	case 2: {
		uint16_t v = *(volatile uint16_t *) at;

		memcpy(to, &v, sizeof(v));
		break;
	}

	case 8: {
		uint64_t v = *(volatile uint64_t *) at;

		memcpy(to, &v, sizeof(v));
		break;
	}

	default: {
		uint32_t v = *(volatile uint32_t *) at;

		memcpy(to, &v, sizeof(v));
		break;
	}
	}
}

static void write_one(volatile void *at, uint64_t value, unsigned int width)
{
	switch (width) {
	case 1:
		*(volatile uint8_t *) at = value;
		break;

	case 2:
		*(volatile uint16_t *) at = value;
		break;

	case 8:
		*(volatile uint64_t *) at = value;
		break;

	default:
		*(volatile uint32_t *) at = value;
		break;
	}
}

static bool read_block(int fd, unsigned long addr, unsigned int len,
		       unsigned int width)
{
	struct mapping __cleanup_mapping m = { .mapped = MAP_FAILED };
	unsigned int i;

	if (!map_block(fd, addr, len, PROT_READ, &m))
		return false;

	for (i = 0; i < len; i += width)
		read_one(m.at + i, block + i, width);

	return true;
}

static bool write_block(int fd, unsigned long addr, unsigned int len,
			unsigned int width, uint64_t value)
{
	struct mapping __cleanup_mapping m = { .mapped = MAP_FAILED };
	unsigned int i;

	if (!map_block(fd, addr, len, PROT_READ | PROT_WRITE, &m))
		return false;

	for (i = 0; i < len; i += width)
		write_one(m.at + i, value, width);

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

static bool parse_address(const char *arg, unsigned long width,
			  unsigned long *addr)
{
	if (!parse_ul(arg, 0, addr)) {
		error("Not an address: %s\n", arg);
		return false;
	}

	if (*addr % width) {
		error("0x%lx isn't %lu byte aligned\n", *addr, width);
		return false;
	}

	return true;
}

int main (int argc, char **argv, char **envp)
{
	unsigned long length = DEFAULT_LENGTH;
	unsigned long addrs[MAX_ADDRESSES];
	unsigned long long value = 0;
	const char *setarg = NULL;
	bool have_length = false;
	int __cleanup_fd fd = -1;
	unsigned long width = 4;
	int naddrs, ret = 0;
	int i, c;

	while ((c = getopt(argc, argv, "l:w:s:")) != -1) {
		switch (c) {
		case 'l':
			if (!parse_ul(optarg, 0, &length) || !length ||
			    length > MAX_LENGTH) {
				usage("Not a length: %s\n", optarg);
				return 1;
			}
			have_length = true;
			break;

		case 's':
			if (!parse_ull(optarg, 0, &value)) {
				usage("Not a value: %s\n", optarg);
				return 1;
			}
			setarg = optarg;
			break;

		case 'w':
			if (!parse_ul(optarg, 0, &width) ||
			    (width != 1 && width != 2 && width != 4 &&
			     width != 8)) {
				usage("Not a width: %s\n", optarg);
				return 1;
			}
			break;

		default:
			usage(USAGE_TEXT);
			return 1;
		}
	}

	if (optind == argc) {
		usage(USAGE_TEXT);
		return 1;
	}

	if (setarg) {
		if (!have_length)
			length = width;

		if (width < 8 && (value >> (width * 8))) {
			error("%s doesn't fit in %lu bytes\n", setarg, width);
			return 1;
		}
	}

	if (length % width) {
		usage("A length of %lu isn't a whole number of %lu byte accesses\n",
		      length, width);
		return 1;
	}

	/* check all of the addresses before touching any of them */
	naddrs = argc - optind;
	if (naddrs > MAX_ADDRESSES) {
		error("Too many addresses, %d is the limit\n", MAX_ADDRESSES);
		return 1;
	}

	for (i = 0; i < naddrs; i++)
		if (!parse_address(argv[optind + i], width, &addrs[i]))
			return 1;

	if (!setarg) {
		block = mmap(NULL, MAX_LENGTH, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (block == MAP_FAILED) {
			error("No room for a %d byte buffer: %d\n", MAX_LENGTH,
			      errno);
			return 1;
		}
	}

	fd = open(DEVMEM_PATH, setarg ? O_RDWR : O_RDONLY);
	if (fd < 0) {
		error("Failed to open %s: %d\n", DEVMEM_PATH, errno);
		return 1;
	}

	for (i = 0; i < naddrs; i++) {
		unsigned long addr = addrs[i];

		if (setarg) {
			if (!write_block(fd, addr, length, width, value))
				ret = 1;
			continue;
		}

		if (!read_block(fd, addr, length, width) ||
		    !show(addr, length))
			ret = 1;
	}

	return ret;
}
