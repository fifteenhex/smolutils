// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_UNALIGNED_H
#define _SMOLUTILS_UNALIGNED_H

static inline uint16_t get_unaligned_be16(const void *buf)
{
	uint16_t v;

	memcpy(&v, buf, sizeof(v));

	return be16toh(v);
}

static inline uint32_t get_unaligned_be32(const void *buf)
{
	uint32_t v;

	memcpy(&v, buf, sizeof(v));

	return be32toh(v);
}

static inline void put_unaligned_be16(void *buf, uint16_t v)
{
	v = htobe16(v);

	memcpy(buf, &v, sizeof(v));
}

#endif /* _SMOLUTILS_UNALIGNED_H */
