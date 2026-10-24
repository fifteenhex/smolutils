// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "inc/multicall.h"

#include <linux/gpio.h>

#define GPIO_DEV_PREFIX	"gpiochip"
#define GPIO_DEV_DIR	"/dev"

/* Does a /dev name look like a chip? */
static bool is_chip(const char *name)
{
	return STARTS_WITH(name, GPIO_DEV_PREFIX);
}

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

static bool parse_offset(const char *arg, unsigned int *offset)
{
	unsigned long v;

	if (!parse_ul_range(arg, 10, 0, GPIO_V2_LINES_MAX - 1, &v)) {
		error("Not a line offset: %s\n", arg);
		return false;
	}

	*offset = v;

	return true;
}

/*
 * Request lines with the given flags (direction, active low, edges) and
 * consumer name. The fd that comes back is how the lines are read, set
 * and watched, and closing it lets them go. Outputs start out driven to
 * values, which is ignored for inputs.
 */
static int request_lines(int chip, const unsigned int *offsets,
			 unsigned int num, uint64_t flags, uint64_t values,
			 const char *consumer)
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
		attr->mask = (1ULL << num) - 1;
	}

	if (ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &req)) {
		error("Failed to request the lines: %d\n", errno);
		return -1;
	}

	return req.fd;
}

static int detect_cb(const char *name, int dir, void *priv)
{
	struct gpiochip_info info;
	int __cleanup_fd fd = -1;

	if (!is_chip(name))
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

	if (!is_chip(name))
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

#define GPIOGET_USAGE "usage: gpioget [-l] <chip> <offset>...\n"

static int prog_gpioget(int argc, char **argv, char **envp)
{
	unsigned int offsets[GPIO_V2_LINES_MAX];
	struct gpio_v2_line_values values;
	uint64_t flags = GPIO_V2_LINE_FLAG_INPUT;
	int __cleanup_fd lines = -1;
	int __cleanup_fd chip = -1;
	unsigned int num;
	unsigned int i;
	int c;

	while ((c = getopt(argc, argv, "l")) != -1) {
		if (c != 'l') {
			usage(GPIOGET_USAGE);
			return 1;
		}
		flags |= GPIO_V2_LINE_FLAG_ACTIVE_LOW;
	}

	num = argc - optind - 1;
	if (argc - optind < 2 || num > GPIO_V2_LINES_MAX) {
		usage(GPIOGET_USAGE);
		return 1;
	}

	for (i = 0; i < num; i++) {
		if (!parse_offset(argv[optind + 1 + i], &offsets[i]))
			return 1;
	}

	chip = open_chip(argv[optind], O_RDONLY);
	if (chip < 0)
		return 1;

	lines = request_lines(chip, offsets, num, flags, 0, "gpioget");
	if (lines < 0)
		return 1;

	values.mask = (1ULL << num) - 1;
	if (ioctl(lines, GPIO_V2_LINE_GET_VALUES_IOCTL, &values)) {
		error("Failed to read the lines: %d\n", errno);
		return 1;
	}

	for (i = 0; i < num; i++)
		printf("%s%u", i ? " " : "", (unsigned int) ((values.bits >> i) & 1));
	printf("\n");

	return 0;
}

#define GPIOSET_USAGE \
	"usage: gpioset [-l] [-t <seconds>] <chip> <offset>=<value>...\n" \
	"       -t holds the lines for that long, otherwise they are let go at once\n"

static int prog_gpioset(int argc, char **argv, char **envp)
{
	unsigned int offsets[GPIO_V2_LINES_MAX];
	uint64_t flags = GPIO_V2_LINE_FLAG_OUTPUT;
	int __cleanup_fd lines = -1;
	int __cleanup_fd chip = -1;
	unsigned long hold = 0;
	uint64_t values = 0;
	unsigned int num;
	unsigned int i;
	int c;

	while ((c = getopt(argc, argv, "lt:")) != -1) {
		switch (c) {
		case 'l':
			flags |= GPIO_V2_LINE_FLAG_ACTIVE_LOW;
			break;

		case 't':
			if (!parse_ul(optarg, 10, &hold)) {
				error("Not a number of seconds: %s\n", optarg);
				return 1;
			}
			break;

		default:
			usage(GPIOSET_USAGE);
			return 1;
		}
	}

	num = argc - optind - 1;
	if (argc - optind < 2 || num > GPIO_V2_LINES_MAX) {
		usage(GPIOSET_USAGE);
		return 1;
	}

	for (i = 0; i < num; i++) {
		char *arg = argv[optind + 1 + i];
		char *eq = strchr(arg, '=');
		unsigned long v;

		if (!eq || !parse_ul_range(eq + 1, 10, 0, 1, &v)) {
			error("Not an <offset>=<value>: %s\n", arg);
			return 1;
		}

		*eq = '\0';
		if (!parse_offset(arg, &offsets[i]))
			return 1;

		values |= (uint64_t) v << i;
	}

	chip = open_chip(argv[optind], O_RDONLY);
	if (chip < 0)
		return 1;

	lines = request_lines(chip, offsets, num, flags, values, "gpioset");
	if (lines < 0)
		return 1;

	if (hold)
		sleep(hold);

	return 0;
}

#define GPIOMON_USAGE \
	"usage: gpiomon [-r] [-f] [-n <events>] <chip> <offset>...\n" \
	"       -r rising edges only, -f falling edges only, -n stop after that many\n"

static int prog_gpiomon(int argc, char **argv, char **envp)
{
	unsigned int offsets[GPIO_V2_LINES_MAX];
	int __cleanup_fd lines = -1;
	int __cleanup_fd chip = -1;
	uint64_t flags = GPIO_V2_LINE_FLAG_INPUT;
	unsigned long wanted = 0;
	unsigned long seen = 0;
	unsigned int num;
	unsigned int i;
	int c;

	while ((c = getopt(argc, argv, "rfn:")) != -1) {
		switch (c) {
		case 'r':
			flags |= GPIO_V2_LINE_FLAG_EDGE_RISING;
			break;

		case 'f':
			flags |= GPIO_V2_LINE_FLAG_EDGE_FALLING;
			break;

		case 'n':
			if (!parse_ul(optarg, 10, &wanted) || !wanted) {
				error("Not a number of events: %s\n", optarg);
				return 1;
			}
			break;

		default:
			usage(GPIOMON_USAGE);
			return 1;
		}
	}

	/* Neither means both */
	if (!(flags & (GPIO_V2_LINE_FLAG_EDGE_RISING | GPIO_V2_LINE_FLAG_EDGE_FALLING)))
		flags |= GPIO_V2_LINE_FLAG_EDGE_RISING | GPIO_V2_LINE_FLAG_EDGE_FALLING;

	num = argc - optind - 1;
	if (argc - optind < 2 || num > GPIO_V2_LINES_MAX) {
		usage(GPIOMON_USAGE);
		return 1;
	}

	for (i = 0; i < num; i++) {
		if (!parse_offset(argv[optind + 1 + i], &offsets[i]))
			return 1;
	}

	chip = open_chip(argv[optind], O_RDONLY);
	if (chip < 0)
		return 1;

	lines = request_lines(chip, offsets, num, flags, 0, "gpiomon");
	if (lines < 0)
		return 1;

	while (!wanted || seen < wanted) {
		struct gpio_v2_line_event event;
		int got;

		got = read_full(lines, &event, sizeof(event));
		if (got != (int) sizeof(event)) {
			error("Failed to read an event: %d\n", errno);
			return 1;
		}

		printf("event: %s EDGE offset: %u timestamp: [%llu.%09llu]\n",
		       event.id == GPIO_V2_LINE_EVENT_RISING_EDGE ? " RISING" : "FALLING",
		       event.offset,
		       (unsigned long long) (event.timestamp_ns / 1000000000ULL),
		       (unsigned long long) (event.timestamp_ns % 1000000000ULL));
		seen++;
	}

	return 0;
}

static const struct multicall_prog progs[] = {
	{ "gpiodetect", prog_gpiodetect },
	{ "gpioget", prog_gpioget },
	{ "gpioinfo", prog_gpioinfo },
	{ "gpiomon", prog_gpiomon },
	{ "gpioset", prog_gpioset },
};

MULTICALL_MAIN(progs)
