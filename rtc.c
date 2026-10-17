// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "inc/date.h"
#include "inc/multicall.h"

#include <linux/rtc.h>

#define RTC_PATH		"/dev/rtc0"

static int read_rtc(int fd, struct rtc_time *t)
{
	if (ioctl(fd, RTC_RD_TIME, t)) {
		error("Failed to read the time: %d\n", errno);
		return -1;
	}

	return 0;
}

/* No timezone support, the RTC is taken to be UTC */
static void print_rtc(const struct rtc_time *t)
{
	printf("%04d-%02d-%02d %02d:%02d:%02d", t->tm_year + 1900,
	       t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
}

static int show_rtc(int fd)
{
	struct rtc_time t;

	if (read_rtc(fd, &t))
		return 1;

	print_rtc(&t);
	printf("\n");

	return 0;
}

static time_t rtc_to_epoch(const struct rtc_time *t)
{
	struct date date = {
		.year = t->tm_year + 1900,
		.mon = t->tm_mon + 1,
		.mday = t->tm_mday,
		.hour = t->tm_hour,
		.min = t->tm_min,
		.sec = t->tm_sec,
	};

	return date_to_epoch(&date);
}

static int hctosys(int fd)
{
	struct timespec ts = { 0 };
	struct rtc_time t;

	if (read_rtc(fd, &t))
		return 1;

	ts.tv_sec = rtc_to_epoch(&t);

	if (clock_settime(CLOCK_REALTIME, &ts)) {
		error("Failed to set the system clock: %d\n", errno);
		return 1;
	}

	return 0;
}

static void epoch_to_rtc(time_t s, struct rtc_time *t)
{
	struct date date;

	date_from_epoch(s, &date);

	memset(t, 0, sizeof(*t));
	t->tm_year = date.year - 1900;
	t->tm_mon = date.mon - 1;
	t->tm_mday = date.mday;
	t->tm_hour = date.hour;
	t->tm_min = date.min;
	t->tm_sec = date.sec;
	t->tm_wday = date.wday;
}

static int systohc(int fd)
{
	struct timespec ts;
	struct rtc_time t;

	clock_gettime(CLOCK_REALTIME, &ts);
	if (ts.tv_sec < 0) {
		error("The system clock is before 1970\n");
		return 1;
	}

	epoch_to_rtc(ts.tv_sec, &t);

	if (ioctl(fd, RTC_SET_TIME, &t)) {
		error("Failed to set the RTC: %d\n", errno);
		return 1;
	}

	return 0;
}

#define HWCLOCK_USAGE	"usage: hwclock [-f <dev>] [-r|-s|-w]\n"

static int prog_hwclock(int argc, char **argv, char **envp)
{
	const char *path = RTC_PATH;
	int __cleanup_fd fd = -1;
	char what = 0;
	int c;

	while ((c = getopt(argc, argv, "f:rsw")) != -1) {
		switch (c) {
		case 'f':
			path = optarg;
			break;

		case 'r':
		case 's':
		case 'w':
			if (what) {
				usage(HWCLOCK_USAGE);
				return 1;
			}
			what = c;
			break;

		default:
			usage(HWCLOCK_USAGE);
			return 1;
		}
	}

	if (optind != argc) {
		usage(HWCLOCK_USAGE);
		return 1;
	}

	fd = open_with_msgs(path, O_RDONLY, 0);
	if (fd < 0)
		return 1;

	switch (what) {
	case 's':
		return hctosys(fd);
	case 'w':
		return systohc(fd);
	default:
		return show_rtc(fd);
	}
}

enum rtcwake_mode {
	MODE_SHOW,
};

static const char *const rtcwake_modes[] = {
	[MODE_SHOW] = "show",
};

static int show_alarm(int fd)
{
	struct rtc_wkalrm a;

	if (ioctl(fd, RTC_WKALM_RD, &a)) {
		error("Failed to read the alarm: %d\n", errno);
		return 1;
	}

	if (!a.enabled) {
		printf("alarm: off\n");
		return 0;
	}

	printf("alarm: on ");
	print_rtc(&a.time);
	printf("\n");

	return 0;
}

#define RTCWAKE_USAGE \
	"usage: rtcwake [-d <dev>] -m <mode>\n" \
	"       modes: show\n"

static int prog_rtcwake(int argc, char **argv, char **envp)
{
	const char *path = RTC_PATH;
	int __cleanup_fd fd = -1;
	int mode = -1;
	int c;

	while ((c = getopt(argc, argv, "d:m:")) != -1) {
		switch (c) {
		case 'd':
			path = optarg;
			break;

		case 'm':
			foreach(m, rtcwake_modes) {
				if (!strcmp(*m, optarg))
					mode = m - rtcwake_modes;
			}
			if (mode < 0) {
				error("Not a mode: %s\n", optarg);
				return 1;
			}
			break;

		default:
			usage(RTCWAKE_USAGE);
			return 1;
		}
	}

	if (optind != argc || mode < 0) {
		usage(RTCWAKE_USAGE);
		return 1;
	}

	fd = open_with_msgs(path, O_RDONLY, 0);
	if (fd < 0)
		return 1;

	return show_alarm(fd);
}

static const struct multicall_prog progs[] = {
	{ "hwclock", prog_hwclock },
	{ "rtcwake", prog_rtcwake },
};

MULTICALL_MAIN(progs)
