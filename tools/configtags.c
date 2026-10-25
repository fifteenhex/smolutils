// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * output the tags for the configuration
 */

#include <stdio.h>
#include <string.h>

#include "../config.h"

struct tag {
	const char *name;
	int on;
};

#define TAG(_name, _config) { .name = #_name, .on = is_enabled(_config) }

static const struct tag tags[] = {
	TAG(modules, CONFIG_MODULES),
	TAG(telnetd, CONFIG_TELNETD),
	TAG(initramfs, CONFIG_INITRAMFS),
	TAG(mtd, CONFIG_MTD),
	TAG(gpio, CONFIG_GPIO),
	TAG(i2c, CONFIG_I2C),
};

int main(int argc, char **argv)
{
	int all = 0;
	unsigned int i;

	if (argc == 2 && strcmp(argv[1], "-a") == 0)
		all = 1;
	else if (argc != 1) {
		fprintf(stderr, "usage: %s [-a]\n", argv[0]);
		return 1;
	}

	for (i = 0; i < sizeof(tags) / sizeof(tags[0]); i++)
		if (all || tags[i].on)
			puts(tags[i].name);

	return 0;
}
