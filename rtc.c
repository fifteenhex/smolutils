// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "inc/date.h"
#include "inc/multicall.h"

#include <linux/rtc.h>

#define RTC_PATH		"/dev/rtc0"
#define POWER_STATE_PATH	"/sys/power/state"

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
		return ex_errno(errno);

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
		return ex_errno(errno);

	ts.tv_sec = rtc_to_epoch(&t);

	if (clock_settime(CLOCK_REALTIME, &ts)) {
		error("Failed to set the system clock: %d\n", errno);
		return ex_errno(errno);
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
		return EX_DATAERR;
	}

	epoch_to_rtc(ts.tv_sec, &t);

	if (ioctl(fd, RTC_SET_TIME, &t)) {
		error("Failed to set the RTC: %d\n", errno);
		return ex_errno(errno);
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
				return EX_USAGE;
			}
			what = c;
			break;

		default:
			usage(HWCLOCK_USAGE);
			return EX_USAGE;
		}
	}

	if (optind != argc) {
		usage(HWCLOCK_USAGE);
		return EX_USAGE;
	}

	fd = open_with_msgs(path, O_RDONLY, 0);
	if (fd < 0)
		return ex_errno(errno);

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
	MODE_DISABLE,
	MODE_NO,
	MODE_MEM,
	MODE_STANDBY,
};

static const char *const rtcwake_modes[] = {
	[MODE_SHOW] = "show",
	[MODE_DISABLE] = "disable",
	[MODE_NO] = "no",
	[MODE_MEM] = "mem",
	[MODE_STANDBY] = "standby",
};

static int show_alarm(int fd)
{
	struct rtc_wkalrm a;

	if (ioctl(fd, RTC_WKALM_RD, &a)) {
		error("Failed to read the alarm: %d\n", errno);
		return EX_IOERR;
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

static int disable_alarm(int fd)
{
	if (ioctl(fd, RTC_AIE_OFF, 0)) {
		error("Failed to disable the alarm: %d\n", errno);
		return EX_IOERR;
	}

	return 0;
}

static int set_alarm(int fd, time_t when)
{
	struct rtc_wkalrm a = { .enabled = 1 };

	epoch_to_rtc(when, &a.time);

	if (ioctl(fd, RTC_WKALM_SET, &a)) {
		error("Failed to set the alarm: %d\n", errno);
		return EX_IOERR;
	}

	return 0;
}

#define RTCWAKE_USAGE \
	"usage: rtcwake [-d <dev>] -m <mode> [-s <seconds>|-t <epoch>]\n" \
	"       modes: show, disable, no, mem, standby\n"

static int prog_rtcwake(int argc, char **argv, char **envp)
{
	unsigned long long epoch = 0;
	const char *path = RTC_PATH;
	unsigned long seconds = 0;
	int __cleanup_fd fd = -1;
	struct rtc_time now;
	struct timespec ts;
	time_t rtc_now;
	int mode = -1;
	time_t when;
	int c;

	while ((c = getopt(argc, argv, "d:m:s:t:")) != -1) {
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
				return EX_USAGE;
			}
			break;

		case 's':
			if (!parse_ul_range(optarg, 0, 1, ULONG_MAX,
					    &seconds)) {
				error("Bad number of seconds: %s\n", optarg);
				return EX_USAGE;
			}
			break;

		case 't':
			if (!parse_ull(optarg, 0, &epoch) || !epoch) {
				error("Bad time: %s\n", optarg);
				return EX_USAGE;
			}
			break;

		default:
			usage(RTCWAKE_USAGE);
			return EX_USAGE;
		}
	}

	if (optind != argc || mode < 0 || (seconds && epoch)) {
		usage(RTCWAKE_USAGE);
		return EX_USAGE;
	}

	if (mode >= MODE_NO && !seconds && !epoch) {
		error("Need -s or -t to say when\n");
		return EX_USAGE;
	}

	fd = open_with_msgs(path, O_RDONLY, 0);
	if (fd < 0)
		return ex_errno(errno);

	if (mode == MODE_SHOW)
		return show_alarm(fd);

	if (mode == MODE_DISABLE)
		return disable_alarm(fd);

	/* The alarm is compared against the RTC, not the system clock */
	if (read_rtc(fd, &now))
		return ex_errno(errno);

	rtc_now = rtc_to_epoch(&now);

	if (seconds) {
		when = rtc_now + seconds;
	} else {
		clock_gettime(CLOCK_REALTIME, &ts);
		when = epoch - ts.tv_sec + rtc_now;
	}

	if (when <= rtc_now) {
		error("The alarm would be in the past\n");
		return EX_DATAERR;
	}

	if (set_alarm(fd, when))
		return EX_IOERR;

	if (mode == MODE_NO)
		return 0;

	if (write_file(POWER_STATE_PATH, rtcwake_modes[mode],
		       strlen(rtcwake_modes[mode])))
		return EX_IOERR;

	return 0;
}

static const struct multicall_prog progs[] = {
	{ "hwclock", prog_hwclock },
	{ "rtcwake", prog_rtcwake },
};

MULTICALL_MAIN(progs)
