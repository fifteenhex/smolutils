#define SIGSETSZ 8

static int _sys_sigaction(int sig,
			 const struct sigaction *act,
			 struct sigaction *oldact)
{
	return (int)__nolibc_syscall4(__NR_rt_sigaction,
				      (long)sig,
				      (long)act,
				      (long)oldact,
				      SIGSETSZ);
}

static int sigaction(int sig,
		     const struct sigaction *act,
		     struct sigaction *oldact)
{
	return __sysret(_sys_sigaction(sig, act, oldact));
}
