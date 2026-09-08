// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_AUTH_H
#define _SMOLUTILS_AUTH_H

#define AUTH_CODE_BYTES	3
#define AUTH_CODE_LEN	(AUTH_CODE_BYTES * 2)

static bool auth_is_secure(int fd)
{
	struct stat here, there;

	if (fstat(fd, &here) || stat(SMOL_SECURETTY_PATH, &there))
		return false;

	return S_ISCHR(here.st_mode) && here.st_rdev == there.st_rdev;
}

static int auth_say(const char *who, const char *code)
{
	char line[AUTH_CODE_LEN + 96];
	int len, fd;

	fd = open(SMOL_SECURETTY_PATH, O_WRONLY | O_NOCTTY);
	if (fd < 0)
		return -1;

	len = snprintf(line, sizeof(line), "\r\n%s joined the party, code %s\r\n",
		       who, code);

	if (len > 0)
		write_full(fd, line, len);

	close(fd);

	return 0;
}

#endif  /* _SMOLUTILS_AUTH_H */
