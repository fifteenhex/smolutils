// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_CONFIG_H
#define _SMOLUTILS_CONFIG_H

/* This is like KConfig, but awful */
#define CONFIG_DEBUG	y

#ifndef CONFIG_NETWORK
#define CONFIG_NETWORK	y
#endif

#ifndef CONFIG_USAGE
#define CONFIG_USAGE	y
#endif

/* Magic */
#define __ARG_PLACEHOLDER_y			0,
#define __take_second_arg(__ignored, val, ...)	val

#define is_enabled(opt)			__is_enabled(opt)
#define __is_enabled(val)		___is_enabled(__ARG_PLACEHOLDER_##val)
#define ___is_enabled(arg1_or_junk)	__take_second_arg(arg1_or_junk 1, 0)

#endif /* _SMOLUTILS_CONFIG_H */
