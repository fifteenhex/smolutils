// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_COLOUR_H
#define _SMOLUTILS_COLOUR_H

#include <asm/ioctls.h>
#include <asm/termbits.h>

#define COLOUR_RESET	"\033[0m"
#define COLOUR_RED	"\033[1;31m"
#define COLOUR_GREEN	"\033[32m"
#define COLOUR_YELLOW	"\033[33m"
#define COLOUR_BLUE	"\033[34m"
#define COLOUR_CYAN	"\033[36m"

static inline const char *colour_end(const char *started)
{
	return *started ? COLOUR_RESET : "";
}

#if is_enabled(CONFIG_COLOUR)

static bool colour_wanted = false;

/* FIXME: add isatty() to nolibc
 */
static inline void colour_setup(void)
{
	struct termios tos;

	colour_wanted = ioctl(STDOUT_FILENO, TCGETS, &tos) == 0;
}

static inline bool colour_on(void)
{
	return colour_wanted;
}

static inline const char *colour(const char *what)
{
	return colour_wanted ? what : "";
}

#else

static inline void colour_setup(void)
{
}

static inline bool colour_on(void)
{
	return false;
}

static inline const char *colour(const char *what)
{
	return "";
}

#endif

#endif /* _SMOLUTILS_COLOUR_H */
