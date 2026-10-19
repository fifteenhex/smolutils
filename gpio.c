// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "inc/multicall.h"

#include <linux/gpio.h>

#define GPIO_DEV_PREFIX	"gpiochip"
#define GPIO_DEV_DIR	"/dev"

/* arg can be gpiochip<n>, <n> or the path */
static int open_chip(const char *arg, int flags)
{
	char path[sizeof(GPIO_DEV_DIR "/" GPIO_DEV_PREFIX) + 24];
	unsigned long n;

	if (arg[0] == '/')
		return open_with_msgs(arg, flags, 0);

	if (parse_ul(arg, 10, &n))
		snprintf(path, sizeof(path), GPIO_DEV_DIR "/" GPIO_DEV_PREFIX "%lu", n);
	else
		snprintf(path, sizeof(path), GPIO_DEV_DIR "/%s", arg);

	return open_with_msgs(path, flags, 0);
}

static bool chip_info(int fd, const char *what, struct gpiochip_info *info)
{
	if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, info)) {
		error("%s isn't a gpio chip: %d\n", what, errno);
		return false;
	}

	return true;
}

static int detect_cb(const char *name, int dir, void *priv)
{
	struct gpiochip_info info;
	int __cleanup_fd fd = -1;

	if (strncmp(name, GPIO_DEV_PREFIX, STRLEN(GPIO_DEV_PREFIX)))
		return 0;

	fd = openat(dir, name, O_RDONLY);
	if (fd < 0 || ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info))
		return 0;

	printf("%s [%s] (%u lines)\n", info.name, info.label, info.lines);

	return 0;
}

#define GPIODETECT_USAGE "usage: gpiodetect\n"

static int prog_gpiodetect(int argc, char **argv, char **envp)
{
	if (argc != 1) {
		usage(GPIODETECT_USAGE);
		return 1;
	}

	return iterate_dir(GPIO_DEV_DIR, detect_cb, NULL) < 0 ? 1 : 0;
}

static const struct multicall_prog progs[] = {
	{ "gpiodetect", prog_gpiodetect },
};

MULTICALL_MAIN(progs)
