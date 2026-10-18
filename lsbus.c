// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "inc/sysfs.h"

#include "inc/multicall.h"

#define PCI_DEVICES "/sys/bus/pci/devices"
#define USB_DEVICES "/sys/bus/usb/devices"

static int cb_pci(const char *name, int dir, void *priv)
{
	unsigned long vendor, device, class;
	char path[256];

	if (!path_join(path, sizeof(path), PCI_DEVICES, name))
		return 0;

	if (!sysfs_read_number(path, "vendor", &vendor) ||
	    !sysfs_read_number(path, "device", &device) ||
	    !sysfs_read_number(path, "class", &class))
		return 0;

	printf("%s [%04lx]: %04lx:%04lx\n", name, (class >> 8) & 0xffff,
	       vendor, device);

	return 0;
}

static int cb_usb(const char *name, int dir, void *priv)
{
	unsigned long bus, dev, vendor, product;
	char what[SYSFS_VALUE_MAX];
	char path[256];

	if (!path_join(path, sizeof(path), USB_DEVICES, name))
		return 0;

	if (!sysfs_read_number(path, "busnum", &bus) ||
	    !sysfs_read_number(path, "devnum", &dev) ||
	    !read_hex_number_in(path, "idVendor", &vendor) ||
	    !read_hex_number_in(path, "idProduct", &product))
		return 0;

	printf("Bus %03lu Device %03lu: ID %04lx:%04lx", bus, dev,
	       vendor, product);

	if (sysfs_read(path, "product", what, sizeof(what)))
		printf(" %s", what);

	printf("\n");

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
	if (argc > 1) {
		usage("usage: lspci\n");
		return 1;
	}

	return list_bus(PCI_DEVICES, cb_pci);
}

static int prog_lsusb(int argc, char **argv, char **envp)
{
	if (argc > 1) {
		usage("usage: lsusb\n");
		return 1;
	}

	return list_bus(USB_DEVICES, cb_usb);
}

static const struct multicall_prog progs[] = {
	{ "lspci", prog_lspci },
	{ "lsusb", prog_lsusb },
};

MULTICALL_MAIN(progs)
