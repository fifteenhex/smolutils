// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_AUTH_H
#define _SMOLUTILS_AUTH_H

static bool auth_is_secure(int fd)
{
	struct stat here, there;

	if (fstat(fd, &here) || stat(SMOL_SECURETTY_PATH, &there))
		return false;

	return S_ISCHR(here.st_mode) && here.st_rdev == there.st_rdev;
}

#endif  /* _SMOLUTILS_AUTH_H */
