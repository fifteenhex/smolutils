#ifndef _SMOLUTILS_RESOLV_H
#define _SMOLUTILS_RESOLV_H


void resolv_doit(const char *hostname)
{
	char * const newargv[] = {
		"resolv",
		hostname,
		NULL
	};

	spawn_and_wait_args("/bin/resolv", newargv);
}

#endif  /* _SMOLUTILS_RESOLV_H */
