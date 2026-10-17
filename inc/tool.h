// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_TOOL_H
#define _SMOLUTILS_TOOL_H

#include "printing.h"

#ifndef TOOL_NAME
#error "TOOL_NAME not defined"
#endif

#ifdef TOOL_USAGE
#define TOOL_USAGE_LINE	"usage: " TOOL_NAME " " TOOL_USAGE "\n"
#else
#define TOOL_USAGE_LINE	"usage: " TOOL_NAME "\n"
#endif

#define TOOL_ARGS_ANY	(-1)

#define TOOL_SIMPLE_MAIN(_min, _max, _run)				\
int main(int argc, char **argv, char **envp)				\
{									\
	int nargs = argc - 1;						\
									\
	if (nargs < (_min) ||						\
	    ((_max) != TOOL_ARGS_ANY && nargs > (_max))) {		\
		usage(TOOL_USAGE_LINE);					\
		return 1;						\
	}								\
									\
	return _run(argc, argv, envp);					\
}

#endif /* _SMOLUTILS_TOOL_H */
