// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_MULTICALL_H
#define _SMOLUTILS_MULTICALL_H

#include "printing.h"
#include "paths.h"
#include "util.h"

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

#define MULTICALL_MAIN(_progs)						\
int main(int argc, char **argv, char **envp)				\
{									\
	MULTICALL_DISPATCH(argv[0], _progs);				\
									\
	error("%s isn't a program in this binary\n", argv[0]);		\
									\
	return 1;							\
}

#endif  /* _SMOLUTILS_MULTICALL_H */
