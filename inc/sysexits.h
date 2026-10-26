// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_SYSEXITS_H
#define _SMOLUTILS_SYSEXITS_H

/*
 * Exit codes, copy paste from BSD, for now
 */
#define EX_OK		0
#define EX_USAGE	64
#if is_enabled(CONFIG_EXITCODES_FINEGRAINED)

#define EX_DATAERR	65
#define EX_NOINPUT	66
#define EX_UNAVAILABLE	69
#define EX_SOFTWARE	70
#define EX_OSERR	71
#define EX_CANTCREAT	73
#define EX_IOERR	74
#define EX_NOPERM	77

/* The code for a system call that failed with errno */
static inline int ex_errno(int err)
{
	switch (err) {
	case ENOENT:
	case ENOTDIR:
	case ESRCH:
		return EX_NOINPUT;
	case EEXIST:
		return EX_CANTCREAT;
	case EISDIR:
	case ENOTEMPTY:
	case ENOTTY:
		return EX_DATAERR;
	case ENODEV:
	case EBUSY:
	case EAGAIN:
		return EX_UNAVAILABLE;
	case EACCES:
	case EPERM:
		return EX_NOPERM;
	default:
		return EX_OSERR;
	}
}

#else

/*
 * Most error exits become 1 so the compiler
 * can squash them all into the same exit path
 */
#define EX_DATAERR	1
#define EX_NOINPUT	1
#define EX_UNAVAILABLE	1
#define EX_SOFTWARE	1
#define EX_OSERR	1
#define EX_CANTCREAT	1
#define EX_IOERR	1
#define EX_NOPERM	1

static inline int ex_errno(int err)
{
	return 1;
}

#endif /* CONFIG_EXITCODES_FINEGRAINED */

#endif /* _SMOLUTILS_SYSEXITS_H */
