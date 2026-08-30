// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef __NOLIBC_EXT_MODULES_H
#define __NOLIBC_EXT_MODULES_H

static long _sys_finit_module(int fd, const char *params, int flags)
{
	return __nolibc_syscall3(__NR_finit_module, fd, params, flags);
}

static int finit_module(int fd, const char *params, int flags)
{
	return __sysret(_sys_finit_module(fd, params, flags));
}

#endif /* __NOLIBC_EXT_MODULES_H */
