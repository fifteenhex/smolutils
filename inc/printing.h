// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_PRINTING_H
#define _SMOLUTILS_PRINTING_H

/* config.h has to be included before this for the CONFIG_ switches */

/*
 * #define TAG "something" before including this
 * to get tagged log lines
 */
#ifdef TAG
#define TAG_PREFIX	TAG ": "
#else
#define TAG_PREFIX	""
#endif

#if is_enabled(CONFIG_DEBUG)
#define debug(fmt, ...) printf(TAG_PREFIX fmt, ##__VA_ARGS__)
#else
#define debug(...)
#endif

/*
 * VERBOSE is for really noisy messages that you
 * probably don't want to enable globally, define
 * this in the file you are debugging.
 */
#ifdef VERBOSE
#define verbose(...) debug(__VA_ARGS__)
#else
#define verbose(...)
#endif

#define error(fmt, ...) fprintf(stderr, TAG_PREFIX fmt, ##__VA_ARGS__)

#define info(...) printf(__VA_ARGS__)

/*
 * Usage messages, CONFIG_USAGE=n removes them if you are really tight on space.
 */
#if is_enabled(CONFIG_USAGE)
#define usage(...) fprintf(stderr, __VA_ARGS__)
#else
#define usage(...)
#endif

#endif /* _SMOLUTILS_PRINTING_H */
