// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

int main (int argc, char **argv, char **envp)
{
	gid_t gid = (gid_t) -1;
	uid_t uid;
	char *end;
	int ret = 0;
	int i;

	if (argc < 3) {
		usage("usage: chown <uid>[:<gid>] <file>...\n");
		return 1;
	}

	uid = strtoul(argv[1], &end, 10);
	if (end == argv[1]) {
		error("Not a uid: %s\n", argv[1]);
		return 1;
	}

	if (*end == ':') {
		char *group = end + 1;

		gid = strtoul(group, &end, 10);
		if (end == group) {
			error("Not a gid: %s\n", group);
			return 1;
		}
	}

	if (*end != '\0') {
		error("Not a uid: %s\n", argv[1]);
		return 1;
	}

	for (i = 2; i < argc; i++) {
		if (chown(argv[i], uid, gid)) {
			error("chown(%s) failed: %d\n", argv[i], errno);
			ret = 1;
		}
	}

	return ret;
}
