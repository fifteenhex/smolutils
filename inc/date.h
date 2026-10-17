// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_DATE_H
#define _SMOLUTILS_DATE_H

/* Broken down UTC time, nolibc has no struct tm, gmtime() or timegm() */
struct date {
	long year;
	int mon;	/* 1..12 */
	int mday;	/* 1..31 */
	int hour;
	int min;
	int sec;
	int wday;	/* 0 is Sunday */
};

/* Howard Hinnant's civil date arithmetic */
static inline long date_days_from_civil(long y, long m, long d)
{
	long era, yoe, doy, doe;

	y -= m <= 2;
	era = (y >= 0 ? y : y - 399) / 400;
	yoe = y - era * 400;
	doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

	return era * 146097 + doe - 719468;
}

static inline void date_civil_from_days(long z, long *y, long *m, long *d)
{
	long era, doe, yoe, doy, mp;

	z += 719468;
	era = (z >= 0 ? z : z - 146096) / 146097;
	doe = z - era * 146097;
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	*d = doy - (153 * mp + 2) / 5 + 1;
	*m = mp + (mp < 10 ? 3 : -9);
	*y = yoe + era * 400 + (*m <= 2);
}

static inline time_t date_to_epoch(const struct date *date)
{
	time_t days = date_days_from_civil(date->year, date->mon, date->mday);

	return days * 86400 + date->hour * 3600 + date->min * 60 + date->sec;
}

/* s must not be negative */
static inline void date_from_epoch(time_t s, struct date *date)
{
	long days = s / 86400;
	long rem = s % 86400;
	long y, m, d;

	date_civil_from_days(days, &y, &m, &d);
	date->year = y;
	date->mon = m;
	date->mday = d;
	date->hour = rem / 3600;
	date->min = (rem % 3600) / 60;
	date->sec = rem % 60;
	date->wday = (days + 4) % 7;
}

#endif /* _SMOLUTILS_DATE_H */
