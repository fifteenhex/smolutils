// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "inc/gpio.h"
#include "inc/multicall.h"

static int detect_cb(const char *name, int dir, void *priv)
{
	struct gpiochip_info info;
	int __cleanup_fd fd = -1;

	if (!gpio_is_chip(name))
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
		return EX_USAGE;
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

	fd = gpio_open_chip(arg, O_RDONLY);
	if (fd < 0)
		return ex_errno(errno);
	if (!gpio_chip_info(fd, arg, &info))
		return EX_DATAERR;

	printf("%s - %u lines:\n", info.name, info.lines);

	for (i = 0; i < info.lines; i++) {
		struct gpio_v2_line_info line;

		if (gpio_line_info(fd, i, &line))
			return EX_IOERR;

		print_line(&line);
	}

	return 0;
}

static int info_cb(const char *name, int dir, void *priv)
{
	int *status = priv;

	if (!gpio_is_chip(name))
		return 0;

	*status = info_chip(name);

	return *status ? -1 : 0;
}

#define GPIOINFO_USAGE "usage: gpioinfo [<chip>...]\n"

static int prog_gpioinfo(int argc, char **argv, char **envp)
{
	int ret;
	int i;

	if (argc == 1) {
		ret = 0;
		if (iterate_dir(GPIO_DEV_DIR, info_cb, &ret))
			return ret ? ret : ex_errno(errno);
		return 0;
	}

	for (i = 1; i < argc; i++) {
		ret = info_chip(argv[i]);
		if (ret)
			return ret;
	}

	return 0;
}

#define GPIOGET_USAGE "usage: gpioget [-l] <chip> <offset>...\n"

static int prog_gpioget(int argc, char **argv, char **envp)
{
	unsigned int offsets[GPIO_V2_LINES_MAX];
	uint64_t flags = GPIO_V2_LINE_FLAG_INPUT;
	int __cleanup_fd lines = -1;
	int __cleanup_fd chip = -1;
	unsigned int num;
	uint64_t values;
	unsigned int i;
	int c;

	while ((c = getopt(argc, argv, "l")) != -1) {
		if (c != 'l') {
			usage(GPIOGET_USAGE);
			return EX_USAGE;
		}
		flags |= GPIO_V2_LINE_FLAG_ACTIVE_LOW;
	}

	num = argc - optind - 1;
	if (argc - optind < 2 || num > GPIO_V2_LINES_MAX) {
		usage(GPIOGET_USAGE);
		return EX_USAGE;
	}

	for (i = 0; i < num; i++) {
		if (!gpio_parse_offset(argv[optind + 1 + i], &offsets[i]))
			return EX_USAGE;
	}

	chip = gpio_open_chip(argv[optind], O_RDONLY);
	if (chip < 0)
		return ex_errno(errno);

	lines = gpio_request_lines(chip, offsets, num, flags, 0, "gpioget");
	if (lines < 0)
		/* Busy, or no such line */
		return EX_UNAVAILABLE;

	if (gpio_get_values(lines, gpio_lines_mask(num), &values))
		return EX_IOERR;

	for (i = 0; i < num; i++)
		printf("%s%u", i ? " " : "",
		       (unsigned int) ((values >> i) & 1));
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
				return EX_USAGE;
			}
			break;

		default:
			usage(GPIOSET_USAGE);
			return EX_USAGE;
		}
	}

	num = argc - optind - 1;
	if (argc - optind < 2 || num > GPIO_V2_LINES_MAX) {
		usage(GPIOSET_USAGE);
		return EX_USAGE;
	}

	for (i = 0; i < num; i++) {
		char *arg = argv[optind + 1 + i];
		char *eq = strchr(arg, '=');
		unsigned long v;

		if (!eq || !parse_ul_range(eq + 1, 10, 0, 1, &v)) {
			error("Not an <offset>=<value>: %s\n", arg);
			return EX_USAGE;
		}

		*eq = '\0';
		if (!gpio_parse_offset(arg, &offsets[i]))
			return EX_USAGE;

		values |= (uint64_t) v << i;
	}

	chip = gpio_open_chip(argv[optind], O_RDONLY);
	if (chip < 0)
		return ex_errno(errno);

	lines = gpio_request_lines(chip, offsets, num, flags, values,
				   "gpioset");
	if (lines < 0)
		/* Busy, or no such line */
		return EX_UNAVAILABLE;

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
				return EX_USAGE;
			}
			break;

		default:
			usage(GPIOMON_USAGE);
			return EX_USAGE;
		}
	}

	/* Neither means both */
	if (!(flags & (GPIO_V2_LINE_FLAG_EDGE_RISING | GPIO_V2_LINE_FLAG_EDGE_FALLING)))
		flags |= GPIO_V2_LINE_FLAG_EDGE_RISING | GPIO_V2_LINE_FLAG_EDGE_FALLING;

	num = argc - optind - 1;
	if (argc - optind < 2 || num > GPIO_V2_LINES_MAX) {
		usage(GPIOMON_USAGE);
		return EX_USAGE;
	}

	for (i = 0; i < num; i++) {
		if (!gpio_parse_offset(argv[optind + 1 + i], &offsets[i]))
			return EX_USAGE;
	}

	chip = gpio_open_chip(argv[optind], O_RDONLY);
	if (chip < 0)
		return ex_errno(errno);

	lines = gpio_request_lines(chip, offsets, num, flags, 0, "gpiomon");
	if (lines < 0)
		/* Busy, or no such line */
		return EX_UNAVAILABLE;

	while (!wanted || seen < wanted) {
		struct gpio_v2_line_event event;

		if (gpio_read_event(lines, &event))
			return EX_IOERR;

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
