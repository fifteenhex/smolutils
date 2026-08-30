// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef __NOLIBC_EXT_SIGNAL_H
#define __NOLIBC_EXT_SIGNAL_H

#define SIGSETSZ 8

#define __NOLIBC_EXT_STR(_s) #_s
#define NOLIBC_EXT_STR(_s) __NOLIBC_EXT_STR(_s)

#if defined(SA_RESTORER) && defined(__x86_64__)
#define NOLIBC_EXT_HAS_RESTORER
/* x86-64 doesn't have trampolines in the kernel, so we must provide it here */
__asm__(
	".text\n"
	".weak __nolibc_ext_restorer\n"
	"__nolibc_ext_restorer:\n"
	"	movq $" NOLIBC_EXT_STR(__NR_rt_sigreturn) ", %rax\n"
	"	syscall\n"
);
void __nolibc_ext_restorer(void);
#endif

static long _sys_sigaction(int sig,
			   const struct sigaction *act,
			   struct sigaction *oldact)
{
	return __nolibc_syscall4(__NR_rt_sigaction,
				 (long)sig,
				 (long)act,
				 (long)oldact,
				 SIGSETSZ);
}

static int sigaction(int sig,
		     const struct sigaction *act,
		     struct sigaction *oldact)
{
	struct sigaction tmpact;

	memcpy(&tmpact, act, sizeof(tmpact));

	/* We don't use oldact yet, so don't handle it */
	if (oldact)
		return __sysret(-EINVAL);

	if (!act)
		return __sysret(_sys_sigaction(sig, NULL, NULL));

#ifdef NOLIBC_EXT_HAS_RESTORER
	tmpact.sa_flags |= SA_RESTORER;
	tmpact.sa_restorer = __nolibc_ext_restorer;
#endif

	return __sysret(_sys_sigaction(sig, &tmpact, NULL));
}

#endif /* __NOLIBC_EXT_SIGNAL_H */
