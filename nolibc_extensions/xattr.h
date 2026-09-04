// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef __NOLIBC_EXT_XATTR_H
#define __NOLIBC_EXT_XATTR_H

static long _sys_setxattr(const char *path, const char *name,
			  const void *value, size_t size, int flags)
{
	return __nolibc_syscall5(__NR_setxattr, path, name, value, size, flags);
}

static int setxattr(const char *path, const char *name,
		    const void *value, size_t size, int flags)
{
	return __sysret(_sys_setxattr(path, name, value, size, flags));
}

static long _sys_getxattr(const char *path, const char *name,
			  void *value, size_t size)
{
	return __nolibc_syscall4(__NR_getxattr, path, name, value, size);
}

static ssize_t getxattr(const char *path, const char *name,
			void *value, size_t size)
{
	return __sysret(_sys_getxattr(path, name, value, size));
}

#endif
