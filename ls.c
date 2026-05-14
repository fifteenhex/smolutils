// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "users.h"

static int cb_short(const char *name, int dir, void *priv)
{
	printf("%s\t", name);

	return 0;
}

static void print_perms(bool r, bool w, bool x)
{
	char _r = '-', _w = '-', _x = '-';

	if (r)
		_r = 'r';
	if (w)
		_w = 'w';
	if (x)
		_x = 'x';

	printf("%c%c%c", _r, _w, _x);
}

static void print_user(uid_t uid)
{
	const char *user = users_map_user(uid);

	if (user)
		printf("%10s", users_map_user(uid));
	else
		printf("%10u", (unsigned int) uid);
}

static void print_group(uid_t gid)
{
	if (gid == 0)
		printf("%10s", "root");
	else
		printf("%10u", gid);
}

static int cb_long(const char *name, int dir, void *priv)
{
	char type = '-';
        struct stat st;

        if (fstatat(dir, name, &st, AT_SYMLINK_NOFOLLOW) == -1)
                return -1;

	/* Type */
	if (st.st_mode & S_IFDIR)
		type = 'd';

	printf("%c", type);

	/* User permissions */
	print_perms(!!(st.st_mode & S_IRUSR),
		    !!(st.st_mode & S_IWUSR),
		    !!(st.st_mode & S_IXUSR));

	/* Group permissions */
	print_perms(!!(st.st_mode & S_IRGRP),
		    !!(st.st_mode & S_IWGRP),
		    !!(st.st_mode & S_IXGRP));

	/* Others permissions */
	print_perms(!!(st.st_mode & S_IROTH),
		    !!(st.st_mode & S_IWOTH),
		    !!(st.st_mode & S_IXOTH));

	/* hard links */
	printf(" %5lu", (unsigned long)st.st_nlink);

	/* user */
	print_user(st.st_uid);

	/* gid */
	print_group(st.st_gid);

	/* size */
	printf(" %10lld", (long long)st.st_size);

	printf(" %s\n", name);

	return 0;
}

int main(int argc, char **argv, char **envp)
{
	bool long_format = false;
	char *path;
	char c;

	while ((c = getopt(argc, argv, "l")) != -1) {
		switch (c) {
		case 'l':
			long_format = true;
			break;
		}
	}

	path = (optind < argc) ? argv[optind] : ".";

	iterate_dir(path, long_format ? cb_long : cb_short, NULL);

	if (!long_format)
		printf("\n");

	return 0;
}
