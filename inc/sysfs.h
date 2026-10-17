// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_SYSFS_H
#define _SMOLUTILS_SYSFS_H

#define SYSFS_VALUE_MAX 128

#define sysfs_read(_dir, _name, _out, _len)	read_file_in(_dir, _name, _out, _len)
#define sysfs_read_number(_dir, _name, _out)	read_number_in(_dir, _name, _out)

#endif /* _SMOLUTILS_SYSFS_H */
