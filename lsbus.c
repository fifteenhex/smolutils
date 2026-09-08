// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "sysfs.h"

#include "multicall.h"

#define PCI_DEVICES "/sys/bus/pci/devices"

static int cb_pci(const char *name, int dir, void *priv)
{
	unsigned long vendor, device, class;
	char path[256];

	if (snprintf(path, sizeof(path), "%s/%s", PCI_DEVICES, name)
	    >= (int) sizeof(path))
		return 0;

	if (!sysfs_read_number(path, "vendor", &vendor) ||
	    !sysfs_read_number(path, "device", &device) ||
	    !sysfs_read_number(path, "class", &class))
		return 0;

	printf("%s [%04lx]: %04lx:%04lx\n", name, (class >> 8) & 0xffff,
	       vendor, device);

	return 0;
}

static int list_bus(const char *devices,
		    int (*cb)(const char *name, int dir, void *priv))
{
	if (iterate_dir(devices, cb, NULL) < 0) {
		if (errno == ENOENT)
			return 0;

		error("Failed to read %s: %d\n", devices, errno);
		return 1;
	}

	return 0;
}

static int prog_lspci(int argc, char **argv, char **envp)
{
	return list_bus(PCI_DEVICES, cb_pci);
}

static const struct multicall_prog progs[] = {
	{ "lspci", prog_lspci },
};

int main (int argc, char **argv, char **envp)
{
	MULTICALL_DISPATCH(argv[0], progs);

	return 1;
}
