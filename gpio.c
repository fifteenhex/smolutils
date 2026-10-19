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

static void print_line(const struct gpio_v2_line_info *line)
{
	uint64_t f = line->flags;

	printf("\tline %3u: %-20s %-12s %6s %11s%s%s%s\n", line->offset,
	       line->name[0] ? line->name : "unnamed",
	       line->consumer[0] ? line->consumer : "unused",
	       f & GPIO_V2_LINE_FLAG_OUTPUT ? "output" : "input",
	       f & GPIO_V2_LINE_FLAG_ACTIVE_LOW ? "active-low" : "active-high",
	       f & GPIO_V2_LINE_FLAG_USED ? " [used]" : "",
	       f & GPIO_V2_LINE_FLAG_BIAS_PULL_UP ? " [pull-up]" : "",
	       f & GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN ? " [pull-down]" : "");
}

static int info_chip(const char *arg)
{
	struct gpiochip_info info;
	int __cleanup_fd fd = -1;
	unsigned int i;

	fd = open_chip(arg, O_RDONLY);
	if (fd < 0 || !chip_info(fd, arg, &info))
		return 1;

	printf("%s - %u lines:\n", info.name, info.lines);

	for (i = 0; i < info.lines; i++) {
		struct gpio_v2_line_info line = { .offset = i };

		if (ioctl(fd, GPIO_V2_GET_LINEINFO_IOCTL, &line)) {
			error("Failed to read line %u: %d\n", i, errno);
			return 1;
		}

		print_line(&line);
	}

	return 0;
}

static int info_cb(const char *name, int dir, void *priv)
{
	char path[sizeof(GPIO_DEV_DIR) + GPIO_MAX_NAME_SIZE + 1];

	if (strncmp(name, GPIO_DEV_PREFIX, STRLEN(GPIO_DEV_PREFIX)))
		return 0;

	snprintf(path, sizeof(path), GPIO_DEV_DIR "/%s", name);

	return info_chip(path) ? -1 : 0;
}

#define GPIOINFO_USAGE "usage: gpioinfo [<chip>...]\n"

static int prog_gpioinfo(int argc, char **argv, char **envp)
{
	int i;

	if (argc == 1)
		return iterate_dir(GPIO_DEV_DIR, info_cb, NULL) ? 1 : 0;

	for (i = 1; i < argc; i++) {
		if (info_chip(argv[i]))
			return 1;
	}

	return 0;
}

static const struct multicall_prog progs[] = {
	{ "gpiodetect", prog_gpiodetect },
	{ "gpioinfo", prog_gpioinfo },
};

MULTICALL_MAIN(progs)
