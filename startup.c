// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#include "nolibc_extensions/unistd.h"

static int do_mount(const char *source, const char *target, const char *type)
{
	int ret;

	ret = mount(source, target, type, 0, NULL);
	if (ret) {
		error("mount(%s) failed: %d\n", target, ret);
		return ret;
	}

	debug("mounted %s(%s) on %s\n", source, type, target);

	return 0;
}

struct mountpoint {
	const char *source;
	const char *target;
	const char *type;
};

static const struct mountpoint fstab[] = {
	{"sysfs", "/sys", "sysfs"},
	{"proc", "/proc", "proc"},
	{"tmp", "/tmp", "tmpfs"},
	{"run", "/run", "tmpfs"},
};

/* This avoids having to use a mount command, fstab etc */
static int mount_filesystems(void)
{
	int ret;

	debug("mounting filesystems...\n");

	for (int i = 0; i < ARRAY_SIZE(fstab); i++) {
		const struct mountpoint *mp = &fstab[i];

		ret = do_mount(mp->source, mp->target, mp->type);
		if (ret)
			goto err;
	}

	return 0;

err:
	return ret;
}

static int setup_network(const char *netif)
{
	debug("configuring network\n");

	spawn_and_wait("dhcpc", "/sbin/dhcpc");

	return 0;
}

int main (int argc, char **argv, char **envp)
{
	char c;
	char *hostname = NULL;
	char *netif = NULL;

        while ((c = getopt(argc, argv, "h:n:")) != -1) {
                switch (c) {
		case 'h':
			hostname = optarg;
			break;
                case 'n':
                        netif = optarg;
                        break;
                }
        }

	if (hostname)
		sethostname(hostname, strlen(hostname));

	mount_filesystems();

	if (is_enabled(CONFIG_NETWORK) && netif)
		setup_network(netif);

	return 0;
}
