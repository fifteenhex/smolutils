// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "inc/multicall.h"

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define I2C_DEV_PATH	"/dev/i2c-"

/* 7 bit addresses, the reserved ones at each end are left alone */
#define I2C_FIRST_ADDR	0x03
#define I2C_LAST_ADDR	0x77

static int open_bus(const char *arg)
{
	char path[sizeof(I2C_DEV_PATH) + 20];
	unsigned long n;

	if (parse_ul(arg, 10, &n)) {
		snprintf(path, sizeof(path), I2C_DEV_PATH "%lu", n);
		arg = path;
	}

	return open_with_msgs(arg, O_RDWR, 0);
}

static bool parse_chip(const char *arg, unsigned int *chip)
{
	unsigned long v;

	if (!parse_ul_range(arg, 0, I2C_FIRST_ADDR, I2C_LAST_ADDR, &v)) {
		error("Not a chip address: %s\n", arg);
		return false;
	}

	*chip = v;

	return true;
}

static bool parse_byte(const char *arg, const char *what, uint8_t *out)
{
	unsigned long v;

	if (!parse_ul_range(arg, 0, 0, 0xff, &v)) {
		error("Not a %s: %s\n", what, arg);
		return false;
	}

	*out = v;

	return true;
}

/* I2C_RDWR bypasses a bound driver, refuse unless forced */
static int check_chip(int fd, unsigned int chip, bool force)
{
	if (force || !ioctl(fd, I2C_SLAVE, chip))
		return 0;

	if (errno == EBUSY)
		error("0x%02x is in use by a driver, -f to force\n", chip);
	else
		error("Failed to select 0x%02x: %d\n", chip, errno);

	return -1;
}

/* A write then a read in one transfer, either part can be empty */
static int xfer(int fd, unsigned int chip, const uint8_t *w, unsigned int wl,
		uint8_t *r, unsigned int rl)
{
	struct i2c_rdwr_ioctl_data d;
	struct i2c_msg msgs[2];
	int ret;

	d.msgs = msgs;
	d.nmsgs = 0;

	if (wl) {
		msgs[d.nmsgs].addr = chip;
		msgs[d.nmsgs].flags = 0;
		msgs[d.nmsgs].len = wl;
		msgs[d.nmsgs].buf = (uint8_t *) w;
		d.nmsgs++;
	}

	if (rl) {
		msgs[d.nmsgs].addr = chip;
		msgs[d.nmsgs].flags = I2C_M_RD;
		msgs[d.nmsgs].len = rl;
		msgs[d.nmsgs].buf = r;
		d.nmsgs++;
	}

	if (!d.nmsgs) {
		errno = EINVAL;
		return -1;
	}

	ret = ioctl(fd, I2C_RDWR, &d);
	if (ret < 0)
		return -1;

	if (ret != (int) d.nmsgs) {
		errno = EIO;
		return -1;
	}

	return 0;
}

/* -y and anything in extra are accepted and ignored, -f sets force */
static int parse_options(int argc, char **argv, const char *extra,
			 bool *force)
{
	char optstring[8];
	int c;

	snprintf(optstring, sizeof(optstring), "yf%s", extra);

	while ((c = getopt(argc, argv, optstring)) != -1) {
		switch (c) {
		case 'f':
			*force = true;
			break;
		case '?':
			return -1;
		default:
			break;
		}
	}

	return optind;
}

#define I2CGET_USAGE \
	"usage: i2cget [-y] [-f] <bus> <chip> [<reg> [b|w]]\n"

static int prog_i2cget(int argc, char **argv, char **envp)
{
	int __cleanup_fd fd = -1;
	unsigned int wl = 0;
	unsigned int n = 1;
	bool force = false;
	unsigned int chip;
	uint8_t buf[2];
	uint8_t reg;
	int first;

	first = parse_options(argc, argv, "", &force);
	if (first < 0 || argc - first < 2 || argc - first > 4) {
		usage(I2CGET_USAGE);
		return 1;
	}
	argv += first;
	argc -= first;

	if (!parse_chip(argv[1], &chip))
		return 1;

	if (argc > 2) {
		if (!parse_byte(argv[2], "register", &reg))
			return 1;
		wl = 1;
	}

	if (argc > 3) {
		if (!strcmp(argv[3], "w")) {
			n = 2;
		} else if (strcmp(argv[3], "b")) {
			usage(I2CGET_USAGE);
			return 1;
		}
	}

	fd = open_bus(argv[0]);
	if (fd < 0)
		return 1;

	if (check_chip(fd, chip, force))
		return 1;

	if (xfer(fd, chip, &reg, wl, buf, n)) {
		error("Read from 0x%02x failed: %d\n", chip, errno);
		return 1;
	}

	/* SMBus byte order */
	if (n == 2)
		printf("0x%04x\n", buf[0] | (buf[1] << 8));
	else
		printf("0x%02x\n", buf[0]);

	return 0;
}

static const struct multicall_prog progs[] = {
	{ "i2cget", prog_i2cget },
};

MULTICALL_MAIN(progs)
