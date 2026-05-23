// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "resolv.h"

#define NTP_SERVER "pool.ntp.org"

int main (int argc, char **argv, char **envp)
{
	resolv_doit(NTP_SERVER);


	return 0;
}
