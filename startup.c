// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "startup"

#include "common.h"
#include "later.h"

#include "nolibc_extensions/unistd.h"
#include "nolibc_extensions/xattr.h"

#if is_enabled(CONFIG_NETWORK)
#include "net.h"
#endif

#include <linux/capability.h>
#include <linux/magic.h>

#define DHCPC_PATH "/sbin/dhcpc"

/* tmpfs supports xattrs but apparently there is no way to embed them in a cpio? */
struct capability {
	const char *path;
	unsigned int caps;
};

static const struct capability capabilities[] = {
	{"/bin/su", (1 << CAP_SETUID) | (1 << CAP_SETGID)},
#if is_enabled(CONFIG_NETWORK)
	{"/bin/ping", 1 << CAP_NET_RAW},
#endif
};

/* Check if we are running from initramfs */
static bool root_is_ram(void)
{
	struct statfs st;

	if (statfs("/", &st))
		return false;

	return st.f_type == RAMFS_MAGIC || st.f_type == TMPFS_MAGIC;
}

static void set_capabilities(void)
{
	if (!is_enabled(CONFIG_INITRAMFS))
		return;

	if (!root_is_ram())
		return;

	verbose("Adding caps\n");

	for (int i = 0; i < ARRAY_SIZE(capabilities); i++) {
		const struct capability *c = &capabilities[i];
		struct vfs_cap_data data = {
			.magic_etc = htole32(VFS_CAP_REVISION_2 |
					     VFS_CAP_FLAGS_EFFECTIVE),
			/* Ok for now ... */
			.data[0].permitted = htole32(c->caps),
		};

		/*
		 * If setxattr doesn't work its because your fs is
		 * poop or your kernel config hasn't enable xattrs for
		 * tmpfs.
		 */
		if (setxattr(c->path, "security.capability",
			     &data, sizeof(data), 0))
			debug("Failed to label %s: %d\n", c->path, errno);
		else
			verbose("labelled %s\n", c->path);
	}
}

static int do_mount(const char *source, const char *target, const char *type)
{
	int ret;

	if (access(target, F_OK) && mkdir(target, 0755)) {
		error("mkdir(%s) failed: %d\n", target, errno);
		return -1;
	}

	ret = mount(source, target, type, 0, NULL);
	if (ret) {
		error("mount(%s) failed: %d\n", target, errno);
		return ret;
	}

	verbose("mounted %s(%s) on %s\n", source, type, target);

	return 0;
}

static bool already_mounted(const char *path)
{
	struct stat st, parent;
	char tmp[64];

	if (stat(path, &st))
		return false;

	if (snprintf(tmp, sizeof(tmp), "%s/..", path) >= (int) sizeof(tmp))
		return false;

	if (stat(tmp, &parent))
		return false;

	return st.st_dev != parent.st_dev;
}

struct mountpoint {
	const char *source;
	const char *target;
	const char *type;
};

struct rundir {
	const char *path;
	mode_t mode;
};

static const struct rundir rundirs[] = {
	{ "/run", 0755 },
	{ SMOL_RUN_DIR, 0755 },
	{ SMOL_RUN_PUBLIC_DIR, 0755 },
	{ SMOL_RUN_PRIVATE_DIR, 0700 },
	{ LATER_DIR, 0700 },
};

static const struct mountpoint fstab[] = {
	{"devtmpfs", "/dev", "devtmpfs"},
	{"devpts", "/dev/pts", "devpts"},
	{"sysfs", "/sys", "sysfs"},
	{"proc", "/proc", "proc"},
	{"tmp", "/tmp", "tmpfs"},
	{"run", "/run", "tmpfs"},
};

/* This avoids having to use a mount command, fstab etc */
static int mount_filesystems(void)
{
	int ret;

	verbose("mounting filesystems...\n");

	for (int i = 0; i < ARRAY_SIZE(fstab); i++) {
		const struct mountpoint *mp = &fstab[i];

		if (already_mounted(mp->target)) {
			verbose("%s is already mounted\n", mp->target);
			continue;
		}

		ret = do_mount(mp->source, mp->target, mp->type);
		if (ret)
			goto err;
	}

	/* State sharing directories */
	for (int i = 0; i < ARRAY_SIZE(rundirs); i++) {
		const struct rundir *d = &rundirs[i];

		if (mkdir(d->path, d->mode) && errno != EEXIST) {
			error("mkdir(%s) failed: %d\n", d->path, errno);
			continue;
		}

		/* mkdir only asks, umask decides, so say it again */
		if (chmod(d->path, d->mode))
			error("chmod(%s) failed: %d\n", d->path, errno);
	}

	return 0;

err:
	return ret;
}

#if is_enabled(CONFIG_NETWORK)
static void setup_loopback(void)
{
	verbose("bringing up loopback\n");

	if (smolutils_net_interface_set_up("lo"))
		verbose("Failed to bring up lo: %d\n", errno);
}

static int setup_network(const char *netif)
{
	char * const dhcpc_args[] = {
		"dhcpc",
		"-i",
		(char *) netif,
		NULL
	};

	verbose("configuring network on %s\n", netif);

	/* Let dhcpc do its thing in the background instead of stalling boot */
	if (!is_enabled(CONFIG_DHCP_WAIT)) {
		if (spawn(DHCPC_PATH, dhcpc_args, environ) < 0)
			error("Failed to start dhcpc: %d\n", errno);

		return 0;
	}

	spawn_and_wait_args(DHCPC_PATH, dhcpc_args);

	return 0;
}
#endif

int main (int argc, char **argv, char **envp)
{
	int c;
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

	set_capabilities();

#if is_enabled(CONFIG_NETWORK)
	setup_loopback();

	if (netif)
		setup_network(netif);
#endif

	return 0;
}
