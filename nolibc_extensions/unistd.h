// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef __NOLIBC_EXT_UNISTD_H
#define __NOLIBC_EXT_UNISTD_H

/* archs that don't have setgid32 never had 16 bit gids, same for uids */
#ifndef __NR_setgid32
#define __NR_setgid32 __NR_setgid
#endif

#ifndef __NR_setuid32
#define __NR_setuid32 __NR_setuid
#endif

static long _sys_setgid(gid_t gid)
{
	return __nolibc_syscall1(__NR_setgid32, gid);
}

static long setgid(gid_t gid)
{
        return __sysret(_sys_setgid(gid));
}

static long _sys_setuid(uid_t uid)
{
	return __nolibc_syscall1(__NR_setuid32, uid);
}

static long setuid(uid_t uid)
{
        return __sysret(_sys_setuid(uid));
}

static long _sys_rename(const char *old, const char *new)
{
	return __nolibc_syscall2(__NR_rename, old, new);
}

static int rename(const char *old, const char *new)
{
	return __sysret(_sys_rename(old, new));
}

static long _sys_sync(void)
{
	return __nolibc_syscall0(__NR_sync);
}

static void sync(void)
{
	_sys_sync();
}

static long _sys_readlinkat(int dir, const char *path, char *buf, size_t size)
{
	return __nolibc_syscall4(__NR_readlinkat, dir, path, buf, size);
}

static ssize_t readlinkat(int dir, const char *path, char *buf, size_t size)
{
	return __sysret(_sys_readlinkat(dir, path, buf, size));
}

static long _sys_unlinkat(int dir, const char *path, int flags)
{
	return __nolibc_syscall3(__NR_unlinkat, dir, path, flags);
}

static int unlinkat(int dir, const char *path, int flags)
{
	return __sysret(_sys_unlinkat(dir, path, flags));
}

static long _sys_sethostname(const char *name, size_t size)
{
	return __nolibc_syscall2(__NR_sethostname, name, size);
}

static long sethostname(const char *name, size_t size)
{
        return __sysret(_sys_sethostname(name, size));
}

#endif /* __NOLIBC_EXT_UNISTD_H */
