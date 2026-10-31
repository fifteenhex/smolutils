// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_NUMBERS_H
#define _SMOLUTILS_NUMBERS_H

#include "printing.h"

#ifndef UINT_MAX
#define UINT_MAX (~0U)
#endif

/* Did we parse the whole string into a number without trailing junk? */
static inline bool __whole_number(const char *s, const char *end)
{
	if (end == s || *end != '\0')
		return false;

	/* WORKAROUND: nolibc's strtoul() currently parses "0x" as 0, glibc stops after the 0 */
	return (end[-1] | 0x20) != 'x';
}

/* explictly check if there is a sign before parsing the number */
static inline bool __has_sign(const char *s)
{
	while (isspace((unsigned char) *s))
		s++;

	return *s == '-';
}

static inline bool parse_ul(const char *s, int base, unsigned long *out)
{
	char *end;

	if (__has_sign(s))
		return false;

	*out = strtoul(s, &end, base);

	return __whole_number(s, end);
}

static inline bool parse_ull(const char *s, int base, unsigned long long *out)
{
	char *end;

	if (__has_sign(s))
		return false;

	*out = strtoull(s, &end, base);

	return __whole_number(s, end);
}

static inline bool parse_l(const char *s, int base, long *out)
{
	char *end;

	*out = strtol(s, &end, base);

	return __whole_number(s, end);
}

static inline bool parse_l_range(const char *s, int base, long min, long max,
				 long *out)
{
	if (!parse_l(s, base, out))
		return false;

	return *out >= min && *out <= max;
}

static inline bool parse_ul_range(const char *s, int base, unsigned long min,
				  unsigned long max, unsigned long *out)
{
	if (!parse_ul(s, base, out))
		return false;

	return *out >= min && *out <= max;
}

static inline bool parse_int_range(const char *s, int base, int min, int max,
				   int *out)
{
	long v;

	if (!parse_l_range(s, base, min, max, &v))
		return false;

	*out = v;

	return true;
}

static inline bool parse_int(const char *s, int base, int *out)
{
	return parse_int_range(s, base, INT_MIN, INT_MAX, out);
}

/* Parse a file descriptor passed as a string, guessing this is always decimal */
static inline bool parse_fd(const char *s, int *out)
{
	return parse_int_range(s, 10, 0, INT_MAX, out);
}

/* Parse a valid TCP/UDP port number */
static inline bool parse_port(const char *s, uint16_t *out)
{
	unsigned long v;

	if (!parse_ul_range(s, 0, 1, 0xffff, &v))
		return false;

	*out = v;

	return true;
}

static inline bool parse_size(const char *s, unsigned long long *out)
{
	char *end;

	if (__has_sign(s))
		return false;

	*out = strtoull(s, &end, 0);
	if (end == s)
		return false;

	switch (*end) {
	case 'G':
		*out <<= 10;
		/* fallthrough */
	case 'M':
		*out <<= 10;
		/* fallthrough */
	case 'K':
		*out <<= 10;
		end++;
		break;
	}

	return __whole_number(s, end);
}

static inline void format_size(char *buf, size_t len,
			       unsigned long long bytes)
{
	const char *unit = "KiB";
	unsigned int shift = 10;
	unsigned int frac;

	if (bytes >= (1ULL << 30)) {
		unit = "GiB";
		shift = 30;
	} else if (bytes >= (1ULL << 20)) {
		unit = "MiB";
		shift = 20;
	}

	frac = (((bytes & ((1ULL << shift) - 1)) >> (shift - 4)) * 10) >> 4;

	snprintf(buf, len, "%llu.%u %s", bytes >> shift, frac, unit);
}

static inline bool parse_fd_with_msgs(const char *s, int *out)
{
	if (parse_fd(s, out))
		return true;

	error("Not an fd: %s\n", s);

	return false;
}

#endif /* _SMOLUTILS_NUMBERS_H */
