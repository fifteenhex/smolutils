// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_COMPILER_H
#define _SMOLUTILS_COMPILER_H

#define __warn_if_unused	__attribute__((warn_unused_result))
#define __smol_always_inline	inline __attribute__((always_inline))
#define __noinline		__attribute__((noinline))
#define __packed		__attribute__((packed))
#define __nonnull		__attribute__((nonnull))
#define __returns_nonnull	__attribute__((returns_nonnull))

#define __cleanup(_fn)		__attribute__((cleanup(_fn)))

#define __smolutils_counted_by(_member) __attribute__((counted_by(_member)))

/* _ptr is RO for _len elements */
#define __access_read_only(_ptr, _len) __attribute__((access(read_only, _ptr, _len)))

#endif /* _SMOLUTILS_COMPILER_H */
