// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_MULTICALL_H
#define _SMOLUTILS_MULTICALL_H

struct multicall_prog {
	const char *progname;
	int (*progcb)(int argc, char **argv, char **envp);
};

#define MULTICALL_DISPATCH(_progname, _progs)				\
{									\
	int i;								\
									\
	for (i = 0; i < ARRAY_SIZE(_progs); i++) {			\
		if (strcmp(_progs[i].progname, _progname) == 0)		\
			return _progs[i].progcb(argc, argv, envp);	\
	}								\
}

#endif  /* _SMOLUTILS_MULTICALL_H */
