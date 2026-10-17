// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_UTIL_H
#define _SMOLUTILS_UTIL_H

#define __warn_if_unused __attribute__((warn_unused_result))

/* Linux kernel trick to force a build failure */
#define __build_bug_on_zero(_e) ((int) sizeof(struct { int : (-!!(_e)); }))

#define __must_be_array(_a)						\
	__build_bug_on_zero(__builtin_types_compatible_p(__typeof__(_a),	\
							 __typeof__(&(_a)[0])))

#define ARRAY_SIZE(_a) (sizeof(_a) / sizeof((_a)[0]) + __must_be_array(_a))

/* round up/down to a power of 2 */
#define round_down(_x, _align) ((_x) & ~((__typeof__(_x)) (_align) - 1))
#define round_up(_x, _align) round_down((_x) + (_align) - 1, _align)

#define __smol_always_inline inline __attribute__((always_inline))

/* for each item in an array */
#define foreach(_p, _array)						\
	for (__typeof__(&(_array)[0]) _p = (_array);			\
	     _p < (_array) + ARRAY_SIZE(_array);			\
	     _p++)

/* Does (_off + _len) fit into _total, taking care that (_off + _len) didn't overflow */
#define off_len_fits(_off, _len, _total)				\
({									\
	__typeof__((_total) + 0) __olf_end;				\
									\
	!__builtin_add_overflow((_off), (_len), &__olf_end) &&		\
	__olf_end <= (_total);						\
})

#endif /* _SMOLUTILS_UTIL_H */
