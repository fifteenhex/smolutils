// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "inc/sysfs.h"

#include "inc/multicall.h"

#include <mtd/mtd-abi.h>

#define SYSFS_CHAR_PATH	"/sys/dev/char"

/* 16K is the biggest NAND page, reads and writes go through this */
#define CHUNK_SIZE	(16 * 1024)
static uint8_t chunk[CHUNK_SIZE];
static uint8_t check[CHUNK_SIZE];

static bool get_info(int fd, struct mtd_info_user *info)
{
	if (ioctl(fd, MEMGETINFO, info)) {
		error("MEMGETINFO failed: %d\n", errno);
		return false;
	}

	return true;
}

static bool is_nand(const struct mtd_info_user *info)
{
	return info->type == MTD_NANDFLASH || info->type == MTD_MLCNANDFLASH;
}

static const char *type_name(const struct mtd_info_user *info)
{
	switch (info->type) {
	case MTD_ABSENT:
		return "absent";
	case MTD_RAM:
		return "ram";
	case MTD_ROM:
		return "rom";
	case MTD_NORFLASH:
		return "nor";
	case MTD_NANDFLASH:
		return "nand";
	case MTD_DATAFLASH:
		return "dataflash";
	case MTD_UBIVOLUME:
		return "ubi";
	case MTD_MLCNANDFLASH:
		return "mlc-nand";
	default:
		return "unknown";
	}
}

#define MTDINFO_USAGE	"usage: mtdinfo <dev>\n"

static int prog_mtdinfo(int argc, char **argv, char **envp)
{
	char path[sizeof(SYSFS_CHAR_PATH) + 32];
	char name[SYSFS_VALUE_MAX];
	struct mtd_info_user info;
	int __cleanup_fd fd = -1;
	char human[32];
	struct stat st;

	if (argc != 2) {
		usage(MTDINFO_USAGE);
		return 1;
	}

	fd = open_with_msgs(argv[1], O_RDONLY, 0);
	if (fd < 0)
		return 1;

	if (!get_info(fd, &info))
		return 1;

	printf("%s\n", path_basename(argv[1]));

	if (!fstat(fd, &st)) {
		snprintf(path, sizeof(path), SYSFS_CHAR_PATH "/%u:%u",
			 major(st.st_rdev), minor(st.st_rdev));
		if (sysfs_read(path, "name", name, sizeof(name)))
			printf("Name:                           %s\n", name);
	}

	printf("Type:                           %s\n", type_name(&info));

	format_size(human, sizeof(human), info.erasesize);
	printf("Eraseblock size:                %u bytes, %s\n",
	       info.erasesize, human);

	format_size(human, sizeof(human), info.size);
	printf("Amount of eraseblocks:          %u (%u bytes, %s)\n",
	       info.size / info.erasesize, info.size, human);

	printf("Minimum input/output unit size: %u byte%s\n", info.writesize,
	       info.writesize == 1 ? "" : "s");

	if (info.oobsize)
		printf("OOB size:                       %u bytes\n",
		       info.oobsize);

	printf("Bad blocks are allowed:         %s\n",
	       is_nand(&info) ? "true" : "false");
	printf("Device is writable:             %s\n",
	       (info.flags & MTD_WRITEABLE) ? "true" : "false");

	return 0;
}

/* An ioctl failure counts as good */
static bool is_bad_block(int fd, const struct mtd_info_user *info, off_t off)
{
	long long at = off;

	return is_nand(info) && ioctl(fd, MEMGETBADBLOCK, &at) > 0;
}

static int erase_block(int fd, off_t off, uint32_t len)
{
	struct erase_info_user64 erase = { .start = off, .length = len };

	if (ioctl(fd, MEMERASE64, &erase)) {
		error("\nFailed to erase 0x%llx: %d\n",
		      (unsigned long long) off, errno);
		return -1;
	}

	return 0;
}

#define FLASH_ERASE_USAGE \
	"usage: flash_erase [-q] <dev> <start> <count>\n"

static int prog_flash_erase(int argc, char **argv, char **envp)
{
	struct mtd_info_user info;
	unsigned long long start;
	int __cleanup_fd fd = -1;
	unsigned long count;
	bool quiet = false;
	unsigned long i;
	int c;

	while ((c = getopt(argc, argv, "q")) != -1) {
		if (c != 'q') {
			usage(FLASH_ERASE_USAGE);
			return 1;
		}
		quiet = true;
	}

	if (argc - optind != 3) {
		usage(FLASH_ERASE_USAGE);
		return 1;
	}

	if (!parse_size(argv[optind + 1], &start)) {
		error("Not an offset: %s\n", argv[optind + 1]);
		return 1;
	}

	if (!parse_ul(argv[optind + 2], 0, &count)) {
		error("Not a count: %s\n", argv[optind + 2]);
		return 1;
	}

	fd = open_with_msgs(argv[optind], O_RDWR, 0);
	if (fd < 0)
		return 1;

	if (!get_info(fd, &info))
		return 1;

	if (start % info.erasesize || start >= info.size) {
		error("0x%llx isn't an eraseblock on this device\n", start);
		return 1;
	}

	if (!count)
		count = (info.size - start) / info.erasesize;

	if (count > (info.size - start) / info.erasesize) {
		error("%lu blocks from 0x%llx is past the end\n", count, start);
		return 1;
	}

	for (i = 0; i < count; i++) {
		off_t off = start + i * (off_t) info.erasesize;

		if (is_bad_block(fd, &info, off)) {
			if (!quiet)
				printf("\nSkipping bad block at 0x%llx\n",
				       (unsigned long long) off);
			continue;
		}

		if (!quiet)
			printf("\rErasing %u Kibyte @ %llx -- %lu %% complete",
			       info.erasesize / 1024, (unsigned long long) off,
			       (i + 1) * 100 / count);

		if (erase_block(fd, off, info.erasesize))
			return 1;
	}

	if (!quiet)
		printf("\n");

	return 0;
}

#define NANDDUMP_USAGE \
	"usage: nanddump [-f <file>] [-s <start>] [-l <length>] <dev>\n"

static int prog_nanddump(int argc, char **argv, char **envp)
{
	unsigned long long length = 0;
	unsigned long long start = 0;
	struct mtd_info_user info;
	int __cleanup_fd out = -1;
	int __cleanup_fd fd = -1;
	const char *file = NULL;
	bool bad = false;
	int c;

	while ((c = getopt(argc, argv, "f:s:l:")) != -1) {
		switch (c) {
		case 'f':
			file = optarg;
			break;

		case 's':
			if (!parse_size(optarg, &start)) {
				error("Not an offset: %s\n", optarg);
				return 1;
			}
			break;

		case 'l':
			if (!parse_size(optarg, &length)) {
				error("Not a length: %s\n", optarg);
				return 1;
			}
			break;

		default:
			usage(NANDDUMP_USAGE);
			return 1;
		}
	}

	if (argc - optind != 1) {
		usage(NANDDUMP_USAGE);
		return 1;
	}

	fd = open_with_msgs(argv[optind], O_RDONLY, 0);
	if (fd < 0)
		return 1;

	if (!get_info(fd, &info))
		return 1;

	if (start >= info.size || length > info.size - start) {
		error("0x%llx + 0x%llx is past the end\n", start, length);
		return 1;
	}

	if (!length)
		length = info.size - start;

	if (file)
		out = open_with_msgs(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	else
		out = dup(STDOUT_FILENO);
	if (out < 0)
		return 1;

	while (length) {
		unsigned int n = info.erasesize - (start % info.erasesize);

		if (n > CHUNK_SIZE)
			n = CHUNK_SIZE;
		if (n > length)
			n = length;

		if (!(start % info.erasesize) || start == 0)
			bad = is_bad_block(fd, &info,
					   start - (start % info.erasesize));

		if (bad) {
			memset(chunk, 0xff, n);
		} else if (lseek(fd, start, SEEK_SET) != (off_t) start ||
			   read_full(fd, chunk, n) != (int) n) {
			error("Failed to read at 0x%llx: %d\n", start, errno);
			return 1;
		}

		if (write_full(out, chunk, n)) {
			error("Failed to write: %d\n", errno);
			return 1;
		}

		start += n;
		length -= n;
	}

	return 0;
}

#define NANDWRITE_USAGE \
	"usage: nandwrite [-p] [-s <start>] <dev> <file>\n"

static int prog_nandwrite(int argc, char **argv, char **envp)
{
	unsigned long long start = 0;
	struct mtd_info_user info;
	int __cleanup_fd fd = -1;
	int __cleanup_fd in = -1;
	bool pad = false;
	int c;

	while ((c = getopt(argc, argv, "ps:")) != -1) {
		switch (c) {
		case 'p':
			pad = true;
			break;

		case 's':
			if (!parse_size(optarg, &start)) {
				error("Not an offset: %s\n", optarg);
				return 1;
			}
			break;

		default:
			usage(NANDWRITE_USAGE);
			return 1;
		}
	}

	if (argc - optind != 2) {
		usage(NANDWRITE_USAGE);
		return 1;
	}

	fd = open_with_msgs(argv[optind], O_RDWR, 0);
	if (fd < 0)
		return 1;

	if (!get_info(fd, &info))
		return 1;

	if (info.writesize > CHUNK_SIZE || CHUNK_SIZE % info.writesize) {
		error("Unsupported page size: %u\n", info.writesize);
		return 1;
	}

	if (start % info.writesize || start >= info.size) {
		error("0x%llx isn't a page on this device\n", start);
		return 1;
	}

	in = open_with_msgs(argv[optind + 1], O_RDONLY, 0);
	if (in < 0)
		return 1;

	while (true) {
		unsigned int n = info.erasesize - (start % info.erasesize);
		unsigned int padding;
		int got;

		if (n > CHUNK_SIZE)
			n = CHUNK_SIZE;

		got = read_full(in, chunk, n);
		if (got < 0) {
			error("Failed to read %s: %d\n", argv[optind + 1],
			      errno);
			return 1;
		}
		if (!got)
			break;

		padding = (info.writesize - (got % info.writesize)) %
			  info.writesize;
		if (padding && !pad) {
			error("%d bytes isn't whole pages, use -p\n", got);
			return 1;
		}
		memset(chunk + got, 0xff, padding);
		got += padding;

		while (start < info.size &&
		       is_bad_block(fd, &info, start - start % info.erasesize))
			start += info.erasesize - start % info.erasesize;

		if (start + got > info.size) {
			error("Ran out of good blocks at 0x%llx\n", start);
			return 1;
		}

		if (lseek(fd, start, SEEK_SET) != (off_t) start ||
		    write_full(fd, chunk, got)) {
			error("Failed to write at 0x%llx: %d\n", start, errno);
			return 1;
		}

		start += got;
	}

	return 0;
}

#define FLASHCP_USAGE	"usage: flashcp [-v] <file> <dev>\n"

static int prog_flashcp(int argc, char **argv, char **envp)
{
	struct mtd_info_user info;
	int __cleanup_fd fd = -1;
	int __cleanup_fd in = -1;
	bool verbose = false;
	unsigned long blocks;
	unsigned long done;
	const char *file;
	unsigned long i;
	off_t size;
	int c;

	while ((c = getopt(argc, argv, "v")) != -1) {
		if (c != 'v') {
			usage(FLASHCP_USAGE);
			return 1;
		}
		verbose = true;
	}

	if (argc - optind != 2) {
		usage(FLASHCP_USAGE);
		return 1;
	}

	file = argv[optind];

	in = open_with_msgs(file, O_RDONLY, 0);
	if (in < 0)
		return 1;

	size = file_size(in);
	if (size <= 0) {
		error("Nothing to write in %s\n", file);
		return 1;
	}

	fd = open_with_msgs(argv[optind + 1], O_RDWR, 0);
	if (fd < 0)
		return 1;

	if (!get_info(fd, &info))
		return 1;

	if (is_nand(&info)) {
		error("%s is NAND, use nandwrite\n", argv[optind + 1]);
		return 1;
	}

	if (size > (off_t) info.size) {
		error("%s is bigger than the device\n", file);
		return 1;
	}

	blocks = (size + info.erasesize - 1) / info.erasesize;

	for (i = 0; i < blocks; i++) {
		if (verbose)
			printf("\rErasing block %lu of %lu", i + 1, blocks);

		if (erase_block(fd, i * (off_t) info.erasesize, info.erasesize))
			return 1;
	}

	for (done = 0; done < size; done += CHUNK_SIZE) {
		unsigned int n = size - done;

		if (n > CHUNK_SIZE)
			n = CHUNK_SIZE;

		if (verbose)
			printf("\rWriting %luk of %luk", (done + n) / 1024,
			       (unsigned long) size / 1024);

		if (read_full(in, chunk, n) != (int) n) {
			error("\nFailed to read %s: %d\n", file, errno);
			return 1;
		}

		if (write_full(fd, chunk, n)) {
			error("\nFailed to write at 0x%lx: %d\n", done, errno);
			return 1;
		}
	}

	if (lseek(in, 0, SEEK_SET) != 0 || lseek(fd, 0, SEEK_SET) != 0) {
		error("\nFailed to seek back: %d\n", errno);
		return 1;
	}

	for (done = 0; done < size; done += CHUNK_SIZE) {
		unsigned int n = size - done;

		if (n > CHUNK_SIZE)
			n = CHUNK_SIZE;

		if (verbose)
			printf("\rVerifying %luk of %luk", (done + n) / 1024,
			       (unsigned long) size / 1024);

		if (read_full(in, chunk, n) != (int) n ||
		    read_full(fd, check, n) != (int) n) {
			error("\nFailed to read back at 0x%lx: %d\n", done,
			      errno);
			return 1;
		}

		if (memcmp(chunk, check, n)) {
			error("\n%s doesn't match the flash at 0x%lx\n", file,
			      done);
			return 1;
		}
	}

	if (verbose)
		printf("\n");

	return 0;
}

static const struct multicall_prog progs[] = {
	{ "flash_erase", prog_flash_erase },
	{ "flashcp", prog_flashcp },
	{ "mtdinfo", prog_mtdinfo },
	{ "nanddump", prog_nanddump },
	{ "nandwrite", prog_nandwrite },
};

MULTICALL_MAIN(progs)
