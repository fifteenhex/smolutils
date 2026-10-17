// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_SIGNALS_H
#define _SMOLUTILS_SIGNALS_H

static inline int signal_catch(int sig, void (*handler)(int), int flags)
{
	struct sigaction act = {
		.sa_handler = handler,
		.sa_flags = flags,
	};

	return sigaction(sig, &act, NULL);
}

static void __signal_dummy_handler(int sig)
{
}

/* Setup a signal just to interrupt something that is blocking to get control back */
static inline int signal_interrupt(int sig)
{
	return signal_catch(sig, __signal_dummy_handler, 0);
}

#endif /* _SMOLUTILS_SIGNALS_H */
