// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_CONFIG_H
#define _SMOLUTILS_CONFIG_H

/* This is like KConfig, but awful */
#ifndef CONFIG_DEBUG
#define CONFIG_DEBUG	y
#endif

/* Support running in an initramfs */
#ifndef CONFIG_INITRAMFS
#define CONFIG_INITRAMFS y
#endif

#ifndef CONFIG_NETWORK
#define CONFIG_NETWORK	y
#endif

#ifndef CONFIG_MODULES
#define CONFIG_MODULES	y
#endif

#ifndef CONFIG_USAGE
#define CONFIG_USAGE	y
#endif

#ifndef CONFIG_TELNETD
#define CONFIG_TELNETD	y
#endif

/* Wait on dhcp to finish during startup */
#ifndef CONFIG_DHCP_WAIT
#define CONFIG_DHCP_WAIT	y
#endif

/* Make everything look like knock-off systemd */
#ifndef CONFIG_COLOUR
#define CONFIG_COLOUR	y
#endif

/* Magic */
#define __ARG_PLACEHOLDER_y			0,
#define __take_second_arg(__ignored, val, ...)	val

#define is_enabled(opt)			__is_enabled(opt)
#define __is_enabled(val)		___is_enabled(__ARG_PLACEHOLDER_##val)
#define ___is_enabled(arg1_or_junk)	__take_second_arg(arg1_or_junk 1, 0)

#endif /* _SMOLUTILS_CONFIG_H */
