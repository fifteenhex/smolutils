// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_MISC_H
#define _SMOLUTILS_MISC_H

static inline uint32_t random_u32(void)
{
	uint32_t v = 0;

	if (getrandom(&v, sizeof(v), 0) == sizeof(v))
		return v;

	/* getrandom() can fail, fallback */
	return ((uint32_t) time(NULL) << 16) ^ (uint32_t) getpid();
}

#endif /* _SMOLUTILS_MISC_H */
