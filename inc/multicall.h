// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_MULTICALL_H
#define _SMOLUTILS_MULTICALL_H

struct multicall_prog {
	const char *progname;
	int (*progcb)(int argc, char **argv, char **envp);
};

#define MULTICALL_DISPATCH(_progname, _progs)				\
{									\
									\
	const char *_name = path_basename(_progname);			\
									\
	foreach(_prog, _progs) {					\
		if (strcmp(_prog->progname, _name) == 0)		\
			return _prog->progcb(argc, argv, envp);		\
	}								\
}

#endif  /* _SMOLUTILS_MULTICALL_H */
