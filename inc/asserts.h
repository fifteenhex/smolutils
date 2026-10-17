// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_ASSERTS_H
#define _SMOLUTILS_ASSERTS_H

/* Check the size of a struct is what you think it is */
#define static_assert_size(_type, _size)				\
	_Static_assert(sizeof(_type) == (_size),			\
		       #_type " isn't " #_size " bytes")

#endif /* _SMOLUTILS_ASSERTS_H */
