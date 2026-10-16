// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define TOOL_NAME	"uname"
#include "inc/tool.h"

static int prog_uname(int argc, char **argv, char **envp)
{
	struct utsname u;

	if (uname(&u))
		return 1;

	printf("%s %s %s %s %s nolibc/Linux\n",
		u.sysname, u.nodename, u.release, u.version, u.machine);

	return 0;
}

TOOL_SIMPLE_MAIN(0, 0, prog_uname)
