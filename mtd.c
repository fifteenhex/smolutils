// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "inc/sysfs.h"

#include "inc/multicall.h"

#include <mtd/mtd-abi.h>

#define SYSFS_CHAR_PATH	"/sys/dev/char"

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

static const struct multicall_prog progs[] = {
	{ "flash_erase", prog_flash_erase },
	{ "mtdinfo", prog_mtdinfo },
};

MULTICALL_MAIN(progs)
