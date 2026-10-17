// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_COMMON_H
#define _SMOLUTILS_COMMON_H

#include "inc/cleanup.h"
#include "inc/fs.h"
#include "inc/io.h"
#include "inc/kernfs.h"
#include "inc/numbers.h"
#include "inc/paths.h"
#include "inc/printing.h"
#include "inc/signals.h"
#include "inc/spawn.h"
#include "inc/strings.h"
#include "inc/util.h"

/* Shared state directories, tmpfs abuse */
#define SMOL_RUN_DIR		"/run/smol"
#define SMOL_RUN_PRIVATE_DIR	SMOL_RUN_DIR "/private"
#define SMOL_RUN_PUBLIC_DIR	SMOL_RUN_DIR "/public"

/* A tty we trust with important stuff */
#define SMOL_SECURETTY_PATH SMOL_RUN_PRIVATE_DIR "/securetty"

#endif /* _SMOLUTILS_COMMON_H */
