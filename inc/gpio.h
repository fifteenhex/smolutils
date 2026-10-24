// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_GPIO_H
#define _SMOLUTILS_GPIO_H

#include "fs.h"
#include "numbers.h"
#include "printing.h"
#include "strings.h"

#include <linux/gpio.h>

#define GPIO_DEV_PREFIX	"gpiochip"
#define GPIO_DEV_DIR	"/dev"

/* Does a /dev name look like a chip? */
static inline bool gpio_is_chip(const char *name)
{
	return STARTS_WITH(name, GPIO_DEV_PREFIX);
}

/* arg can be gpiochip<n>, <n> or the path */
static inline int gpio_open_chip(const char *arg, int flags)
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

static inline bool gpio_chip_info(int fd, const char *what,
				  struct gpiochip_info *info)
{
	if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, info)) {
		error("%s isn't a gpio chip: %d\n", what, errno);
		return false;
	}

	return true;
}

static inline bool gpio_parse_offset(const char *arg, unsigned int *offset)
{
	unsigned long v;

	if (!parse_ul_range(arg, 10, 0, GPIO_V2_LINES_MAX - 1, &v)) {
		error("Not a line offset: %s\n", arg);
		return false;
	}

	*offset = v;

	return true;
}

/* generate a mask for the first num lines of a request */
static inline uint64_t gpio_lines_mask(unsigned int num)
{
	return num >= 64 ? ~0ULL : (1ULL << num) - 1;
}

/*
 * Request lines with the given flags (direction, active low, edges) and
 * consumer name. The fd that comes back is how the lines are read, set
 * and watched, and closing it lets them go. Outputs start out driven to
 * values, which is ignored for inputs.
 */
static inline int gpio_request_lines(int chip, const unsigned int *offsets,
				     unsigned int num, uint64_t flags,
				     uint64_t values, const char *consumer)
{
	struct gpio_v2_line_config_attribute *attr;
	struct gpio_v2_line_request req = { 0 };
	unsigned int i;

	for (i = 0; i < num; i++)
		req.offsets[i] = offsets[i];
	req.num_lines = num;
	req.config.flags = flags;
	snprintf(req.consumer, sizeof(req.consumer), "%s", consumer);

	if (flags & GPIO_V2_LINE_FLAG_OUTPUT) {
		attr = &req.config.attrs[0];
		req.config.num_attrs = 1;
		attr->attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
		attr->attr.values = values;
		attr->mask = gpio_lines_mask(num);
	}

	if (ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &req)) {
		error("Failed to request the lines: %d\n", errno);
		return -1;
	}

	return req.fd;
}

#endif /* _SMOLUTILS_GPIO_H */
