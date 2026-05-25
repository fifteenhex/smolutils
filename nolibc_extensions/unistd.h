#ifndef __NOLIBC_EXT_UNISTD_H
#define __NOLIBC_EXT_UNISTD_H

static long _sys_setgid(gid_t gid)
{
	return __nolibc_syscall1(__NR_setgid, gid);
}

static long setgid(gid_t gid)
{
        return __sysret(_sys_setgid(gid));
}

static long _sys_setuid(uid_t uid)
{
	return __nolibc_syscall1(__NR_setuid, uid);
}

static long setuid(uid_t uid)
{
        return __sysret(_sys_setuid(uid));
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
