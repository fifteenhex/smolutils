// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_NUMBERS_H
#define _SMOLUTILS_NUMBERS_H

/* Did we parse the whole string into a number without trailing junk? */
static inline bool __whole_number(const char *s, const char *end)
{
	return end != s && *end == '\0';
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

#endif /* _SMOLUTILS_NUMBERS_H */
