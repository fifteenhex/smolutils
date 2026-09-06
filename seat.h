// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_SEAT_H
#define _SMOLUTILS_SEAT_H

#define SEAT_ENV	"SMOL_SEAT"

#define SEAT_FMT	"/run/seat-%s"
#define SEAT_PATH_MAX	64

#define seat_path(_buf, _len, _tty)					\
	snprintf(_buf, _len, SEAT_FMT, _tty)

static inline const char *seat_name(const char *tty_path)
{
	const char *slash = strrchr(tty_path, '/');

	return slash ? slash + 1 : tty_path;
}

#define SEAT_SOURCES	{ "/dev/input", "/dev/dri" }

#endif  /* _SMOLUTILS_SEAT_H */
