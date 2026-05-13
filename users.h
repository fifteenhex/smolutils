#ifndef _SMOLUTILS_USERS_H
#define _SMOLUTILS_USERS_H

#include "nolibc_extensions/unistd.h"

#define SMOLUTILS_USERS_NORMAL_MIN	1024

static int users_changeuser(gid_t gid, uid_t uid)
{
	if (setgid(gid) < 0) {
		return -1;
	}

	if (setuid(uid) < 0) {
		return -1;
	}

	return 0;
}

static const char *users_map_user(uid_t uid)
{
	if (uid == 0)
		return "root";

	return "dunno";
}

#endif  /* _SMOLUTILS_USERS_H */
