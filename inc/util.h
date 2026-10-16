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

#endif /* _SMOLUTILS_UTIL_H */
