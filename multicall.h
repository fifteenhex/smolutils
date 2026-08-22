// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_MULTICALL_H
#define _SMOLUTILS_MULTICALL_H

struct multicall_prog {
	const char *progname;
	int (*progcb)(int argc, char **argv, char **envp);
};

/* multicall works on the basename but we might have been called by path */
static const char *multicall_basename(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

#define MULTICALL_DISPATCH(_progname, _progs)				\
{									\
	const char *_name = multicall_basename(_progname);		\
	unsigned int i;							\
									\
	for (i = 0; i < ARRAY_SIZE(_progs); i++) {			\
		if (strcmp(_progs[i].progname, _name) == 0)		\
			return _progs[i].progcb(argc, argv, envp);	\
	}								\
}

#endif  /* _SMOLUTILS_MULTICALL_H */
