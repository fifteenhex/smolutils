// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_AUTH_H
#define _SMOLUTILS_AUTH_H

#include "readln.h"

#define AUTH_CODE_BYTES	3
#define AUTH_CODE_LEN	(AUTH_CODE_BYTES * 2)
#define AUTH_TRIES	3

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

static int auth_ask(const char *who)
{
	unsigned char raw[AUTH_CODE_BYTES];
	char code[AUTH_CODE_LEN + 1];
	char said[AUTH_CODE_LEN + 1];
	unsigned int i;

	if (getrandom(raw, sizeof(raw), 0) != sizeof(raw)) {
		verbose("getrandom() failed trying to generate code: %d\n", errno);
		return -1;
	}

	for (i = 0; i < sizeof(raw); i++)
		snprintf(code + i * 2, 3, "%02x", raw[i]);

	if (auth_say(who, code)) {
		verbose("Error showing code: %d\n", errno);
		return -1;
	}

	for (i = 0; i < AUTH_TRIES; i++) {
		int len;

		printf("code: ");

		len = readln(said, sizeof(said) - 1, NULL);
		if (len < 0)
			return -1;

		if (len == AUTH_CODE_LEN && !strcmp(said, code))
			return 0;

		printf("no\n");
	}

	return -1;
}

static int auth_check(const char *who)
{
	struct stat link;

	if (!is_enabled(CONFIG_AUTH))
		return 0;

	if (lstat(SMOL_SECURETTY_PATH, &link))
		return 0;

	if (auth_is_secure(STDIN_FILENO))
		return 0;

	return auth_ask(who);
}

#endif  /* _SMOLUTILS_AUTH_H */
