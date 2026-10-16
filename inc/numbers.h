// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_NUMBERS_H
#define _SMOLUTILS_NUMBERS_H

static inline bool parse_ul(const char *s, int base, unsigned long *out)
{
	char *end;

	*out = strtoul(s, &end, base);

	return end != s && *end == '\0';
}

static inline bool parse_ull(const char *s, int base, unsigned long long *out)
{
	char *end;

	*out = strtoull(s, &end, base);

	return end != s && *end == '\0';
}

static inline bool parse_size(const char *s, unsigned long long *out)
{
	char *end;

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

	return *end == '\0';
}

#endif /* _SMOLUTILS_NUMBERS_H */
